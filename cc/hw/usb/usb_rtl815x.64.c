/**
 * @file usb_rtl815x.64.c
 * @brief USB RTL815X driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb_rtl815x.h>
#include <logging.h>
#include <time/timer.h>
#include <pipeline.h>
#include <network/network_arp.h>
#include <network/network_ethernet.h>
#include <network/network_dhcpv4.h>
#include <cpu/task.h>
#include <strings.h>

MODULE("turnstone.kernel.hw.usb");

typedef struct usb_driver_t {
    usb_device_t*           usb_device;
    usb_interface_t*        interface;
    usb_pipeline_callback_f pipeline_callback;
    uint32_t                expected_packet_size;
    uint16_t                ocp_base;
    list_t*                 return_queue;
    network_mac_address_t   mac;
    usb_endpoint_t*         bulk_in;
    usb_endpoint_t*         bulk_out;
    usb_endpoint_t*         intr;
    uint16_t                intr_value;
    uint64_t                tx_task_id;
} usb_driver_t;

typedef struct usb_rtl815x_tx_t {
    uint32_t length;
    uint32_t flags;
    uint8_t  data[];
} __attribute__((packed)) usb_rtl815x_tx_t;

typedef struct usb_rtl815x_rx_t {
    uint32_t length;
    uint32_t flags1;
    uint32_t flags2;
    uint32_t flags3;
    uint32_t flags4;
    uint32_t flags5;
    uint8_t  data[];
} __attribute__((packed)) usb_rtl815x_rx_t;

_Static_assert(sizeof(usb_rtl815x_rx_t) == 24, "invalid usb_rtl815x_tx_t size");

static list_t* usb_rtl815x_drivers = NULL;

static int8_t usb_rtl815x_read_reg(usb_driver_t* drv, uint16_t type, uint16_t index, void* buf, uint16_t len) {
    if((len & 3) || !len || (index & 3) || !buf) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");
        return -1;
    }

    if((uint32_t)index + (uint32_t)len > 0xFFFF) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");
        return -1;
    }

    uint16_t limit = 64;
    int8_t ret;

    while(len) {
        uint16_t chunk = len > limit ? limit : len;

        ret = usb_vendor_read(drv, RTL8152_REQ_GET_REGS, index, type, buf, chunk);

        if(ret != 0) {
            return ret;
        }

        len   -= chunk;
        index += chunk;
        buf   = (uint8_t*)buf + chunk;
    }

    return 0;
}

static int8_t usb_rtl815x_read_reg8(usb_driver_t* drv, uint16_t type, uint16_t index, uint8_t* data) {
    uint32_t input;
    uint32_t output;

    uint8_t shift = index & 3;

    index &= ~3;

    int8_t ret = usb_rtl815x_read_reg(drv, type, index, &input, sizeof(uint32_t));

    if (ret != 0) {
        return ret;
    }

    output = (input >> (shift * 8)) & 0xFF;

    *data = (uint8_t)output;

    return 0;
}

static int8_t usb_rtl815x_read_reg16(usb_driver_t* drv, uint16_t type, uint16_t index, uint16_t* data) {
    uint32_t input;
    uint32_t output;
    uint16_t byen = RTL815X_BYTE_EN_WORD;
    uint8_t shift = index & 2;

    index &= ~3;

    byen <<= shift;

    int8_t ret = usb_rtl815x_read_reg(drv, type | byen, index, &input, sizeof(uint32_t));

    if (ret != 0) {
        return ret;
    }

    output = (input >> (shift * 8)) & 0xFFFF;

    *data = (uint16_t)output;

    return 0;
}

static int8_t usb_rtl815x_read_reg32(usb_driver_t* drv, uint16_t type, uint16_t index, uint32_t* data) {
    return usb_rtl815x_read_reg(drv, type, index, data, sizeof(uint32_t));
}

static int8_t usb_rtl815x_read_reg64(usb_driver_t* drv, uint16_t type, uint16_t index, uint64_t* data) {
    return usb_rtl815x_read_reg(drv, type, index, data, sizeof(uint64_t));
}

static int8_t usb_rtl815x_write_reg(usb_driver_t* drv, uint16_t byteen, uint16_t type, uint16_t index, void* buf, uint16_t len) {
    uint16_t byteen_start, byteen_end, byen;
    uint16_t limit = 512;
    int8_t ret = 0;
    uint8_t* data = (uint8_t*)buf;

    if ((len & 3) || !len || (index & 3) || !buf) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");
        return -1;
    }

    if ((uint32_t)index + (uint32_t)len > 0xffff) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");
        return -1;
    }

    byteen_start = byteen & RTL815X_BYTE_EN_START_MASK;
    byteen_end = byteen & RTL815X_BYTE_EN_END_MASK;

    byen = byteen_start | (byteen_start << 4);

    if(byen != RTL815X_BYTE_EN_DWORD) {
        ret = usb_vendor_write(drv, RTL8152_REQ_SET_REGS, index, type | byen, buf, 4);

        if(ret != 0) {
            return ret;
        }

        index += 4;
        data += 4;
        len -= 4;
    }

    if(!len) {
        return 0;
    }

    byen = byteen_end | (byteen_end >> 4);

    if(byen != RTL815X_BYTE_EN_DWORD) {
        len -= 4;
    }

    while(len) {
        uint16_t chunk = len > limit ? limit : len;

        ret = usb_vendor_write(drv, RTL8152_REQ_SET_REGS, index, type | RTL815X_BYTE_EN_DWORD, data, chunk);

        if(ret != 0) {
            return ret;
        }

        len   -= chunk;
        index += chunk;
        data  += chunk;
    }

    if(byen != RTL815X_BYTE_EN_DWORD) {
        ret = usb_vendor_write(drv, RTL8152_REQ_SET_REGS, index, type | byen, data, 4);
    }

    return ret;
}

static int8_t usb_rtl815x_write_reg8(usb_driver_t* drv, uint16_t type, uint16_t index, uint8_t data) {
    uint32_t input = data;

    uint16_t byen = RTL815X_BYTE_EN_BYTE;
    uint8_t shift = index & 3;

    if(index & 3) {
        byen <<= shift;
        input <<= (shift * 8);
        index &= ~3;
    }

    return usb_rtl815x_write_reg(drv, byen, type, index, &input, sizeof(uint32_t));
}

static int8_t usb_rtl815x_write_reg16(usb_driver_t* drv, uint16_t type, uint16_t index, uint16_t data) {
    uint32_t input = data;

    uint16_t byen = RTL815X_BYTE_EN_WORD;
    uint8_t shift = index & 2;

    if(index & 2) {
        byen <<= shift;
        input <<= (shift * 8);
        index &= ~3;
    }

    return usb_rtl815x_write_reg(drv, byen, type, index, &input, sizeof(uint32_t));
}

static int8_t usb_rtl815x_write_reg32(usb_driver_t* drv, uint16_t type, uint16_t index, uint32_t data) {
    return usb_rtl815x_write_reg(drv, RTL815X_BYTE_EN_DWORD, type, index, &data, sizeof(uint32_t));
}

#if 0
static int8_t usb_rtl815x_write_reg64(usb_driver_t* drv, uint16_t type, uint16_t index, uint64_t data) {
    uint32_t low = (uint32_t)(data & 0xFFFFFFFF);
    uint32_t high = (uint32_t)((data >> 32) & 0xFFFFFFFF);

    int8_t ret = usb_rtl815x_write_reg32(drv, type, index, low);

    if (ret != 0) {
        return ret;
    }

    return usb_rtl815x_write_reg32(drv, type, index + 4, high);
}

static int8_t usb_rtl815x_sram_write(usb_driver_t* drv, uint16_t addr, uint16_t data) {
    if(usb_rtl815x_ocp_reg_write(drv, RTL815X_OCP_SRAM_ADDR, addr) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write SRAM_ADDR");
        return -1;
    }

    if(usb_rtl815x_ocp_reg_write(drv, RTL815X_OCP_SRAM_DATA, data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write SRAM_DATA");
        return -1;
    }

    return 0;
}

#endif

static int8_t usb_rtl815x_ocp_reg_read(usb_driver_t* drv, uint16_t addr, uint16_t* data) {
    uint16_t ocp_base, ocp_index;

    ocp_base = addr & 0xf000;
    if (ocp_base != drv->ocp_base) {
        if(usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_OCP_GPHY_BASE, ocp_base) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot write OCP base");
            return -1;
        }

        drv->ocp_base = ocp_base;
    }

    ocp_index = (addr & 0x0fff) | 0xb000;
    return usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, ocp_index, data);
}

static int8_t usb_rtl815x_ocp_reg_write(usb_driver_t* drv, uint16_t addr, uint16_t data) {
    uint16_t ocp_base, ocp_index;

    ocp_base = addr & 0xf000;
    if (ocp_base != drv->ocp_base) {
        if(usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_OCP_GPHY_BASE, ocp_base) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot write OCP base");
            return -1;
        }

        drv->ocp_base = ocp_base;
    }

    ocp_index = (addr & 0x0fff) | 0xb000;
    return usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, ocp_index, data);
}

static inline int8_t usb_rtl815x_mdio_write(usb_driver_t* drv, uint32_t reg_addr, uint32_t value) {
    return usb_rtl815x_ocp_reg_write(drv, RTL815X_OCP_BASE_MII + reg_addr * 2, value);
}

static inline int usb_rtl815x_mdio_read(usb_driver_t* drv, uint32_t reg_addr, uint16_t* value) {
    return usb_rtl815x_ocp_reg_read(drv, RTL815X_OCP_BASE_MII + reg_addr * 2, value);
}

static int8_t usb_rtl815x_aldps_en(usb_driver_t* drv, boolean_t enable) {
    uint16_t data;

    if(usb_rtl815x_ocp_reg_read(drv, RTL815X_OCP_POWER_CFG, &data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read OCP_POWER_CFG");
        return -1;
    }

    if (enable) {
        data |= RTL815X_EN_ALDPS;

        if(usb_rtl815x_ocp_reg_write(drv, RTL815X_OCP_POWER_CFG, data) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot write OCP_POWER_CFG");
            return -1;
        }
    } else {

        data &= ~RTL815X_EN_ALDPS;

        if(usb_rtl815x_ocp_reg_write(drv, RTL815X_OCP_POWER_CFG, data) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot write OCP_POWER_CFG");
            return -1;
        }

        time_timer_msleep(200);
    }

    return 0;
}

static int8_t usb_rtl815x_eee_disable(usb_driver_t* drv) {
    uint16_t ocp_data;
    uint16_t config;

    if(usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_EEE_CR, &ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read PLA_EEE_CR");
        return -1;
    }

    if(usb_rtl815x_ocp_reg_read(drv, RTL815X_OCP_EEE_CFG, &config) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read OCP_EEE_CFG");
        return -1;
    }

    ocp_data &= ~(RTL815X_EEE_RX_EN | RTL815X_EEE_TX_EN);
    config &= ~RTL815X_EEE10_EN;

    if(usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_EEE_CR, ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read PLA_EEE_CR");
        return -1;
    }

    if(usb_rtl815x_ocp_reg_write(drv, RTL815X_OCP_EEE_CFG, config) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write OCP_EEE_CFG");
        return -1;
    }

    if(usb_rtl815x_ocp_reg_write(drv, RTL815X_OCP_EEE_ADV, 0) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write OCP_EEE_ADV");
        return -1;
    }

    return 0;
}

static int8_t usb_rtl815x_wait_reset_clear(usb_driver_t* drv, uint32_t timeout_ms) {
    uint8_t cr = 0;
    const uint32_t interval_us = 1000; // 1 ms polling
    uint32_t elapsed = 0;

    while (elapsed < timeout_ms) {
        if (usb_rtl815x_read_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_CR, &cr) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot read control register during reset");
            return -1;
        }

        if (!(cr & RTL815X_CR_RST)) {
            return 0; // reset cleared
        }

        time_timer_spinsleep(interval_us); // sleep 1 ms
        elapsed += interval_us / 1000;
    }

    PRINTLOG(USB, LOG_ERROR, "reset bit still set after %u ms", timeout_ms);
    return -1;
}

static int8_t usb_rtl815x_reset(usb_driver_t* drv) {
    // Set reset bit in CR
    if (usb_rtl815x_write_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_CR, RTL815X_CR_RST) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write reset to control register");
        return -1;
    }

    // Wait for reset to clear
    if (usb_rtl815x_wait_reset_clear(drv, 100) != 0) {
        return -1;
    }

    return 0;
}

static int8_t usb_rtl815x_read_mac(usb_driver_t* drv, uint8_t mac[6]) {
    uint64_t __attribute__((aligned(128))) mac_64b = 0;
    if (usb_rtl815x_read_reg64(drv, RTL815X_PLA_BASE, RTL815X_PLA_MAC, &mac_64b) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot MAC low");
        return -1;
    }

    // Combine bytes into MAC array (little-endian in registers)
    mac[0] = (mac_64b >> 0) & 0xFF;
    mac[1] = (mac_64b >> 8) & 0xFF;
    mac[2] = (mac_64b >> 16) & 0xFF;
    mac[3] = (mac_64b >> 24) & 0xFF;
    mac[4] = (mac_64b >> 32) & 0xFF;
    mac[5] = (mac_64b >> 40) & 0xFF;

    return 0;
}

static int8_t usb_rtl815x_set_mtu(usb_driver_t* drv, uint16_t mtu) {
    if (mtu < 68 || mtu > 9198) {
        PRINTLOG(USB, LOG_ERROR, "invalid MTU size %d", mtu);
        return -1;
    }

    if(usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_RMS, mtu) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot set MTU");
        return -1;
    }

    if(usb_rtl815x_write_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_MTPS, RTL815X_MTPS_JUMBO) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot set MTU");
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "MTU set to %d", mtu);
    return 0;
}

static int8_t usb_rtl815x_get_speed(usb_driver_t* drv, uint16_t* speed) {
    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_PHYSTATUS, speed) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read PHYSTATUS");
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "link speed status: 0x%04x", *speed);

    return 0;
}

static int8_t usb_rtl815x_configure_extra_status(usb_driver_t* drv) {

    uint16_t extra_status = 0;

    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_EXTRA_STATUS, &extra_status) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read EXTRA_STATUS");
        return -1;
    }

    uint16_t speed = 0;

    if (usb_rtl815x_get_speed(drv, &speed) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get speed");
        return -1;
    }

    if(speed & RTL815X_LINK_STATUS) {
        extra_status |= RTL815X_CUR_LINK_OK;
    } else {
        extra_status &= ~RTL815X_CUR_LINK_OK;
    }

    extra_status |= RTL815X_POLL_LINK_CHG;

    if (usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_EXTRA_STATUS, extra_status) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write EXTRA_STATUS");
        return -1;
    }

    return 0;
}

static int8_t usb_rtl815x_reset_packet_filer(usb_driver_t* drv) {
    uint16_t fmc = 0;

    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_FMC, &fmc) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read FMC");
        return -1;
    }

    fmc &= ~RTL815X_FMC_FCR_MCU_EN;

    if (usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_FMC, fmc) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write FMC");
        return -1;
    }

    fmc |= RTL815X_FMC_FCR_MCU_EN;

    if (usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_FMC, fmc) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write FMC");
        return -1;
    }

    return 0;
}

static int8_t usb_rtl815x_rx_vlan_en(usb_driver_t* drv, boolean_t enable) {
    uint16_t reg = 0;

    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_CPCR, &reg) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read PLA_CPCR, rx_vlan_en failed");
        return -1;
    }

    if (enable) {
        reg |= RTL815X_CPCR_RX_VLAN;
    } else {
        reg &= ~RTL815X_CPCR_RX_VLAN;
    }

    if (usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_CPCR, reg) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write PLA_CPCR, rx_vlan_en failed");
        return -1;
    }

    return 0;
}



static int8_t usb_rtl815x_cfg_dummy1(usb_driver_t* drv) {
    uint8_t dummy1;
    uint16_t burst_size;

    if(usb_rtl815x_read_reg8(drv, RTL815X_USB_BASE, RTL815X_USB_CSR_DUMMY1, &dummy1) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read CSR_DUMMY1");
        return -1;
    }

    if(usb_rtl815x_read_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_BURST_SIZE, &burst_size) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read BURST_SIZE");
        return -1;
    }

    if (burst_size == 0) {
        dummy1 &= ~RTL815X_DYNAMIC_BURST;
    } else {
        dummy1 |= RTL815X_DYNAMIC_BURST;
    }

    if(usb_rtl815x_write_reg8(drv, RTL815X_USB_BASE, RTL815X_USB_CSR_DUMMY1, dummy1) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write CSR_DUMMY1");
        return -1;
    }

    return 0;
}

static int8_t usb_rtl815x_cfg_dummy2(usb_driver_t* drv) {
    uint8_t ocp_data;

    if(usb_rtl815x_read_reg8(drv, RTL815X_USB_BASE, RTL815X_USB_CSR_DUMMY2, &ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read CSR_DUMMY2");
        return -1;
    }

    ocp_data |= RTL815X_EP4_FULL_FC;

    if(usb_rtl815x_write_reg8(drv, RTL815X_USB_BASE, RTL815X_USB_CSR_DUMMY2, ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write CSR_DUMMY2");
        return -1;
    }


    return 0;
}

static int8_t usb_rtl815x_cfg_afe(usb_driver_t* drv) {
    uint16_t ocp_data;

    if(usb_rtl815x_read_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_AFE_CTRL2, &ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read AFE_CTRL2");
        return -1;
    }

    ocp_data &= ~RTL815X_SEN_VAL_MASK;
    ocp_data |= RTL815X_SEN_VAL_NORMAL | RTL815X_SEL_RXIDLE;

    if(usb_rtl815x_write_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_AFE_CTRL2, ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write AFE_CTRL2");
        return -1;
    }


    return 0;
}

static int8_t usb_rtl815x_cfg_wdt11(usb_driver_t* drv) {
    uint16_t ocp_data;

    if(usb_rtl815x_read_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_WDT11_CTRL, &ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read WDT11_CTRL");
        return -1;
    }

    ocp_data &= ~RTL815X_TIMER11_EN;

    if(usb_rtl815x_write_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_WDT11_CTRL, ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write WDT11_CTRL");
        return -1;
    }


    return 0;
}

// may be needed at future
#if 0

static int8_t usb_rtl815x_enable_rx_agg(usb_driver_t* drv) {
    uint16_t rx_agg = 0;
    if (usb_rtl815x_read_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_CTRL, &rx_agg) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read USB_CTRL");
        return -1;
    }

    rx_agg &= ~(RTL815X_RX_AGG_DISABLE | RTL815X_RX_ZERO_EN);

    if (usb_rtl815x_write_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_CTRL, rx_agg) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write USB_CTRL");
        return -1;
    }

    return 0;
}

static int8_t usb_rtl815x_wait_oob_link_list_ready(usb_driver_t* drv, uint32_t timeout_ms) {
    uint8_t oob_ctrl = 0;
    uint32_t elapsed = 0;

    while (elapsed++ < timeout_ms) {
        if (usb_rtl815x_read_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_OOB_CTRL, &oob_ctrl) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot read OOB_CTRL during wait for link list ready");
            return -1;
        }

        if (oob_ctrl & RTL815X_LINK_LIST_READY) {
            return 0; // link list ready
        }

        time_timer_msleep(100); // sleep 1 ms
    }

    PRINTLOG(USB, LOG_DEBUG, "OOB_CTRL: 0x%08x", oob_ctrl);

    PRINTLOG(USB, LOG_ERROR, "link list not ready after %u ms", timeout_ms);
    return -1;
}

#endif

static int8_t usb_rtl815x_reset_bmu(usb_driver_t* drv) {
    uint8_t ocp_data = 0;

    if(usb_rtl815x_read_reg8(drv, RTL815X_USB_BASE, RTL815X_USB_BMU_RESET, &ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read BMU_RESET");
        return -1;
    }

    ocp_data &= ~(RTL815X_BMU_RESET_EP_IN | RTL815X_BMU_RESET_EP_OUT);

    if(usb_rtl815x_write_reg8(drv, RTL815X_USB_BASE, RTL815X_USB_BMU_RESET, ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write BMU_RESET");
        return -1;
    }

    ocp_data |= RTL815X_BMU_RESET_EP_IN | RTL815X_BMU_RESET_EP_OUT;

    if(usb_rtl815x_write_reg8(drv, RTL815X_USB_BASE, RTL815X_USB_BMU_RESET, ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write BMU_RESET");
        return -1;
    }

    return 0;
}

static int8_t usb_rtl815x_read_status(usb_driver_t* drv) {
    uint8_t ocp_data;

    if (usb_rtl815x_read_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_CR, &ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read CR");
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "CR: 0x%02x", ocp_data);

    uint16_t speed = 0;

    if (usb_rtl815x_get_speed(drv, &speed) != 0) {
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "link status 0x%04x", speed);

    PRINTLOG(USB, LOG_INFO, "connection speed: %s, %s-duplex",
             (speed & RTL815X_1000BPS) ? "1000Mbps" :
             (speed & RTL815X_100BPS) ? "100Mbps" : "10Mbps",
             (speed & RTL815X_FULL_DUP) ? "full" : "half");

    uint32_t txfifo = 0;

    if(usb_rtl815x_read_reg32(drv, RTL815X_PLA_BASE, RTL815X_PLA_TXFIFO_CTRL, &txfifo) != 0) {
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "TXFIFO_CTRL: 0x%08x", txfifo);

    uint8_t oob_ctrl = 0;

    if(usb_rtl815x_read_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_OOB_CTRL, &oob_ctrl) != 0) {
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "OOB_CTRL: 0x%02x", oob_ctrl);

    uint16_t ocp_phy_status = 0;

    if(usb_rtl815x_ocp_reg_read(drv, RTL815X_OCP_PHY_STATUS, &ocp_phy_status) != 0) {
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "OCP_PHY_STATUS: 0x%04x", ocp_phy_status);

    uint16_t mac_pwr_ctrl, mac_pwr_ctrl2, mac_pwr_ctrl3, mac_pwr_ctrl4;

    if(usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_MAC_PWR_CTRL, &mac_pwr_ctrl) != 0) {
        return -1;
    }

    if(usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_MAC_PWR_CTRL2, &mac_pwr_ctrl2) != 0) {
        return -1;
    }

    if(usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_MAC_PWR_CTRL3, &mac_pwr_ctrl3) != 0) {
        return -1;
    }

    if(usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_MAC_PWR_CTRL4, &mac_pwr_ctrl4) != 0) {
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "MAC_PWR_CTRL: 0x%04x", mac_pwr_ctrl);
    PRINTLOG(USB, LOG_DEBUG, "MAC_PWR_CTRL2: 0x%04x", mac_pwr_ctrl2);
    PRINTLOG(USB, LOG_DEBUG, "MAC_PWR_CTRL3: 0x%04x", mac_pwr_ctrl3);
    PRINTLOG(USB, LOG_DEBUG, "MAC_PWR_CTRL4: 0x%04x", mac_pwr_ctrl4);

    uint16_t phy_pwr;

    if(usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_PHY_PWR, &phy_pwr) != 0) {
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "PHY_PWR: 0x%04x", phy_pwr);

    uint16_t cpcr;

    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_CPCR, &cpcr) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read PLA_CPCR, rx_vlan_en failed");
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "CPCR: 0x%04x", cpcr);

    uint64_t mar = 0;

    if (usb_rtl815x_read_reg64(drv, RTL815X_PLA_BASE, RTL815X_PLA_MAR, &mar) != 0) {
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "MAR: 0x%016llx", mar);

    uint16_t extra_status = 0;

    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_EXTRA_STATUS, &extra_status) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read EXTRA_STATUS");
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "EXTRA_STATUS: 0x%04x", extra_status);

    return 0;
}

static int8_t usb_rtl815x_init(usb_driver_t* drv) {
    if(usb_rtl815x_reset(drv) != 0) {
        return -1;
    }

    if(usb_rtl815x_reset_bmu(drv) != 0) {
        return -1;
    }

    uint8_t oob_data;

    if (usb_rtl815x_read_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_OOB_CTRL, &oob_data) != 0) {
        return -1;
    }

    oob_data &= ~RTL815X_NOW_IS_OOB;

    if (usb_rtl815x_write_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_OOB_CTRL, oob_data) != 0) {
        return -1;
    }

    if(usb_rtl815x_aldps_en(drv, false) != 0) {
        return -1;
    }

    if(usb_rtl815x_eee_disable(drv) != 0) {
        return -1;
    }

    uint16_t phy_pwr;

    if(usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_PHY_PWR, &phy_pwr) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read PLA_PHY_PWR");
        return -1;
    }

    phy_pwr |= RTL815X_PFM_PWM_SWITCH;

    if(usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_PHY_PWR, phy_pwr) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write PLA_PHY_PWR");
        return -1;
    }

    if(usb_rtl815x_cfg_dummy1(drv) != 0) {
        return -1;
    }

    if(usb_rtl815x_configure_extra_status(drv) != 0) {
        return -1;
    }

    if(usb_rtl815x_cfg_dummy2(drv) != 0) {
        return -1;
    }

    if(usb_rtl815x_cfg_wdt11(drv) != 0) {
        return -1;
    }

    if (usb_rtl815x_write_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_LPM_CTRL,
                                RTL815X_FIFO_EMPTY_1FB | RTL815X_ROK_EXIT_LPM | RTL815X_LPM_TIMER_500US
                                ) != 0) {
        return -1;
    }

    if(usb_rtl815x_cfg_afe(drv) != 0) {
        return -1;
    }

    if (usb_rtl815x_rx_vlan_en(drv, true) != 0) {
        return -1;
    }

    // Set MTU to 1500
    if (usb_rtl815x_set_mtu(drv, 1500) != 0) {
        return -1;
    }

    uint16_t tcr0 = 0;

    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_TCR0, &tcr0) != 0) {
        return -1;
    }

    tcr0 |= RTL815X_TCR0_AUTO_FIFO;

    if (usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_TCR0, tcr0) != 0) {
        return -1;
    }

    if (usb_rtl815x_write_reg32(drv, RTL815X_PLA_BASE, RTL815X_PLA_RXFIFO_CTRL0,
                                RTL815X_RXFIFO_THR1_NORMAL) != 0) {
        return -1;
    }

    if (usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_RXFIFO_CTRL1,
                                RTL815X_RXFIFO_THR2_NORMAL) != 0) {
        return -1;
    }

    if (usb_rtl815x_write_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_RXFIFO_CTRL2,
                                RTL815X_RXFIFO_THR3_NORMAL) != 0) {
        return -1;
    }

    if (usb_rtl815x_write_reg32(drv, RTL815X_PLA_BASE, RTL815X_PLA_TXFIFO_CTRL,
                                RTL815X_TXFIFO_THR_NORMAL2) != 0) {
        return -1;
    }

    if (usb_rtl815x_write_reg32(drv, RTL815X_PLA_BASE, RTL815X_PLA_RCR,
                                RTL815X_RCR_ACPT_ALL) != 0) {
        return -1;
    }

    if (usb_rtl815x_write_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_RX_EARLY_TIMEOUT, RTL815X_COALESCE_SUPER / 8) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot set RX_EARLY_TIMEOUT");
        return -1;
    }

    if (usb_rtl815x_write_reg16(drv, RTL815X_USB_BASE, RTL815X_USB_RX_EARLY_SIZE, 1200 / 4) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot set RX_EARLY_SIZE");
        return -1;
    }

    if (usb_rtl815x_reset_packet_filer(drv) != 0) {
        return -1;
    }

    uint8_t ocp_data = 0;

    if (usb_rtl815x_read_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_CR, &ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read CR");
        return -1;
    }

    ocp_data |= RTL815X_CR_RE | RTL815X_CR_TE;

    // Enable RX and TX by setting CR
    if (usb_rtl815x_write_reg8(drv, RTL815X_PLA_BASE, RTL815X_PLA_CR, ocp_data) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot write CR");
        return -1;
    }

    if(usb_rtl815x_read_status(drv) != 0) {
        return -1;
    }

    return 0;
}

extern uint64_t network_rx_task_id;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t usb_rtl815x_pipeline_callback(const usb_driver_t* driver, uint8_t endpoint, pipeline_t* pipeline) {

    if(endpoint == driver->intr->desc->endpoint_address) {
        usb_driver_t* drv = (usb_driver_t*)driver;
        while(pipeline_available_data(pipeline) > 0) {
            uint16_t new_intr_value;
            pipeline_read(pipeline, 2, (uint8_t*)&new_intr_value);

            if(driver->intr_value != new_intr_value) {
                PRINTLOG(USB, LOG_DEBUG, "interrupt value changed: 0x%04x -> 0x%04x", driver->intr_value, new_intr_value);
                drv->intr_value = new_intr_value;
/*
                uint16_t speed = 0;

                if (usb_rtl815x_get_speed(drv, &speed) == 0) {
                    PRINTLOG(USB, LOG_DEBUG, "link status 0x%04x", speed);

                    PRINTLOG(USB, LOG_DEBUG, "connection speed: %s, %s-duplex",
                             (speed & RTL815X_1000BPS) ? "1000Mbps" :
                             (speed & RTL815X_100BPS) ? "100Mbps" : "10Mbps",
                             (speed & RTL815X_FULL_DUP) ? "full" : "half");
                }

                uint16_t ocp_phy_status = 0;

                if(usb_rtl815x_ocp_reg_read(drv, RTL815X_OCP_PHY_STATUS, &ocp_phy_status) == 0) {
                    PRINTLOG(USB, LOG_DEBUG, "OCP_PHY_STATUS: 0x%04x", ocp_phy_status);
                }
 */
            } else {
                PRINTLOG(USB, LOG_TRACE, "interrupt value unchanged: 0x%04x", driver->intr_value);
            }
        }

        return 0;
    }

    boolean_t notify_network_rx = true;

    while(pipeline_available_data(pipeline) > 0) {
        // Process all available packets

        uint64_t available_data = pipeline_available_data(pipeline);

        PRINTLOG(USB, LOG_TRACE, "pipeline available data: 0x%llx", available_data);

        if(available_data < sizeof(usb_rtl815x_rx_t)) {
            PRINTLOG(USB, LOG_DEBUG, "not enough data for RX header available_data=%llu", available_data);
            break;
        }

        usb_rtl815x_rx_t rx_hdr;

        uint64_t rc = pipeline_read(pipeline, sizeof(usb_rtl815x_rx_t), (uint8_t*)&rx_hdr);

        if(rc == -1ULL || rc != sizeof(usb_rtl815x_rx_t)) {
            PRINTLOG(USB, LOG_ERROR, "cannot read full RX header rc=%llu", rc);
            pipeline_clear(pipeline);
            return -1;
        }

        available_data -= sizeof(usb_rtl815x_rx_t);

        uint32_t pktlen = rx_hdr.length & 0x7FFFU;

        PRINTLOG(USB, LOG_TRACE, "rx header: length=0x%08x flags1=0x%08x pktlen=0x%08x",
                 rx_hdr.length, rx_hdr.flags1, pktlen);

        if(available_data < pktlen) {
            PRINTLOG(USB, LOG_ERROR, "not enough data for full RX packet available_data=%llu pktlen=%u", available_data, pktlen);
            pipeline_clear(pipeline);
            return -1;
        }

        boolean_t is_vlan_tagged = (rx_hdr.flags1 & BIT(16)) != 0;
        uint16_t vlan_id = rx_hdr.flags1 & 0x0FFFU;
        vlan_id = BYTE_SWAP16(vlan_id);

        network_received_packet_t* packet = memory_malloc_ext(list_get_heap(network_received_packets), sizeof(network_received_packet_t), 0);

        if(!packet) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for received packet");
            continue;
        }

        packet->packet_len = pktlen;
        packet->return_queue = driver->return_queue;
        packet->network_info = (void*)driver->mac;
        packet->network_type = NETWORK_TYPE_ETHERNET;
        packet->is_vlan_tagged = is_vlan_tagged;
        packet->vlan_id = vlan_id;
        packet->tx_task_id = driver->tx_task_id;

        packet->packet_data = memory_malloc_ext(list_get_heap(network_received_packets), pktlen, 0);

        if(packet->packet_data == NULL) {
            PRINTLOG(USB, LOG_ERROR, "failed to allocate packet");
            memory_free_ext(list_get_heap(network_received_packets), packet);
            notify_network_rx = true;

            continue;
        }

        rc = pipeline_read(pipeline, pktlen, packet->packet_data);

        if(rc == -1ULL || rc != pktlen) {
            PRINTLOG(USB, LOG_ERROR, "cannot read full RX packet rc=%llu pktlen=%u", rc, pktlen);
            pipeline_clear(pipeline);
            return -1;
        }

        if(list_queue_push(network_received_packets, packet) == -1ULL) {
            PRINTLOG(USB, LOG_ERROR, "failed to queue packet");
            memory_free_ext(list_get_heap(network_received_packets), packet->packet_data);
            memory_free_ext(list_get_heap(network_received_packets), packet);
        } else {
            PRINTLOG(USB, LOG_TRACE, "packet queued");

            if(notify_network_rx && network_rx_task_id) {
                task_set_message_received(network_rx_task_id);
                PRINTLOG(USB, LOG_TRACE, "cleared message waiting for rx task 0x%llx", network_rx_task_id);
                notify_network_rx = false;
            }
        }

        PRINTLOG(USB, LOG_TRACE, "pipeline available data after processing: %llu", pipeline_available_data(pipeline));

        char_t junk[8];

        available_data = pipeline_available_data(pipeline);

        uint64_t junk_size = pktlen % 8;

        if(junk_size) {
            junk_size = 8 - junk_size;
        }

        if(junk_size && available_data >= junk_size) {
            pipeline_read(pipeline, junk_size, (uint8_t*)junk);
        }
    }

    return 0;
}
#pragma GCC diagnostic pop

static boolean_t usb_rtl815x_write(usb_driver_t* usb_driver, usb_rtl815x_tx_t* tx) {
    usb_transfer_t ut = {0};


    ut.driver = usb_driver;
    ut.endpoint = usb_driver->bulk_out;
    ut.is_async = false;

    ut.length = sizeof(usb_rtl815x_tx_t) + (tx->length & 0x3FFFFU);

    ut.data = (uint8_t*)tx;

    int8_t res =  usb_driver->usb_device->controller->data_transfer(usb_driver->usb_device->controller, &ut);

    if(res != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot send data");

        return false;
    }

    PRINTLOG(USB, LOG_TRACE, "sent %d bytes", ut.length);

    return ut.complete && ut.success;
}

static int8_t usb_rtl815x_process_tx(void) {

    for(uint64_t drv_idx = 0; drv_idx < list_size(usb_rtl815x_drivers); drv_idx++) {
        usb_driver_t* drv = (usb_driver_t*)list_get_data_at_position(usb_rtl815x_drivers, drv_idx);

        drv->return_queue = list_create_queue_with_heap(NULL);
        task_add_message_queue(drv->return_queue);

        network_info_t ni = {0};
        memory_memcopy(&drv->mac, &ni.mac, 6);
        ni.has_hw_vlan_support = true;
        ni.is_vlan_tagged = true;
        ni.vlan_id = 12;
        ni.return_queue = drv->return_queue;

        network_register_network_info(&ni);

        void** args = memory_malloc(sizeof(void*) * 2);

        if(args == NULL) {
            return -1;
        }

        char_t* dhcp_task_name = strprintf("dhcp-%02x%02x%02x%02x%02x%02x",
                                           drv->mac[0], drv->mac[1], drv->mac[2],
                                           drv->mac[3], drv->mac[4], drv->mac[5]);

        args[0] = (void*)drv->mac;
        args[1] = drv->return_queue;

        task_create_task(NULL, 1 << 20, 64 << 10, &network_dhcpv4_send_discover, 2, args, dhcp_task_name);
    }

    while(1) {
        boolean_t packet_exists = false;

        for(uint64_t drv_idx = 0; drv_idx < list_size(usb_rtl815x_drivers); drv_idx++) {
            usb_driver_t* drv = (usb_driver_t*)list_get_data_at_position(usb_rtl815x_drivers, drv_idx);

            const network_info_t* ni = map_get(network_info_map, &drv->mac);

            while(list_size(drv->return_queue)) {
                const network_transmit_packet_t* packet = list_queue_pop(drv->return_queue);

                if(packet) {
                    PRINTLOG(NETWORK, LOG_TRACE, "network packet will be sended with length 0x%llx", packet->packet_len);
                    packet_exists = true;

                    uint32_t packet_len = packet->packet_len & 0x3FFFFU;

                    usb_rtl815x_tx_t* tx = memory_malloc(sizeof(usb_rtl815x_tx_t) + packet_len);

                    if(!tx) {
                        PRINTLOG(NETWORK, LOG_ERROR, "cannot allocate memory for tx packet");
                        memory_free((void*)packet);
                        continue;
                    }

                    tx->length = BIT(31) | BIT(30) | packet_len;
                    tx->flags = 0;

                    if(ni->has_hw_vlan_support && packet->is_vlan_tagged) {
                        tx->flags |= BIT(16) | BYTE_SWAP16(packet->vlan_id & 0x0FFFU);
                    }

                    memory_memcopy(packet->packet_data, (uint8_t*)tx + sizeof(usb_rtl815x_tx_t), packet_len);

                    if(!usb_rtl815x_write(drv, tx)) {
                        PRINTLOG(NETWORK, LOG_ERROR, "cannot send packet");
                    }

                    memory_free(tx);

                    memory_free(packet->packet_data);
                    memory_free((void*)packet);
                }

                PRINTLOG(NETWORK, LOG_TRACE, "tx queue size 0x%llx", list_size(drv->return_queue));
            }

        }

        if(!packet_exists) {
            task_set_message_waiting();
            task_yield();
        }

    }

    return 0;
}

int8_t usb_device_rtl815x_init(usb_device_t* device, usb_interface_t* interface) {
    if(!device || !interface) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "initializing RTL8152/RTL8153 device");

    if(!usb_rtl815x_drivers) {
        usb_rtl815x_drivers = list_create_list_with_heap(NULL);

        if(!usb_rtl815x_drivers) {
            PRINTLOG(USB, LOG_ERROR, "cannot create rtl815x drivers list");
            return -1;
        }
    }


    usb_driver_t* drv = memory_malloc(sizeof(usb_driver_t));

    if(!drv) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb driver");
        return -1;
    }

    drv->usb_device = device;
    drv->interface = interface;
    drv->pipeline_callback = NULL;

    interface->driver = drv;

    for(uint32_t i = 0; i < interface->num_endpoints; i++) {
        usb_endpoint_t* ep = interface->endpoints[i];

        if(ep->desc->attributes == USB_ENDPOINT_TYPE_BULK) {
            if(USB_ENDPOINT_DIRECTION_IS_IN(ep->desc->endpoint_address)) {
                drv->bulk_in = ep;
            } else {
                drv->bulk_out = ep;
            }
        } else if(ep->desc->attributes == USB_ENDPOINT_TYPE_INTERRUPT) {
            drv->intr = ep;
        }


    }

    // Get version
    uint16_t version = 0;
    if (usb_rtl815x_read_reg16(drv, RTL815X_PLA_BASE, RTL815X_PLA_VERSION, &version) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot read version");
        memory_free(drv);
        return -1;
    }

    version &= RTL815X_VERSION_MASK;

    PRINTLOG(USB, LOG_INFO, "RTL8152/RTL8153 version: 0x%04x", version);

    if (usb_rtl815x_init(drv) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot initialize device");
        memory_free(drv);
        return -1;
    }

    // read mac address

    if (usb_rtl815x_read_mac(drv, drv->mac) != 0) {
        memory_free(drv);
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
             drv->mac[0], drv->mac[1], drv->mac[2],
             drv->mac[3], drv->mac[4], drv->mac[5]);

    uint16_t rx_ep_size = drv->bulk_in->desc->max_packet_size;

    if(drv->bulk_in->endpoint_companion) {
        rx_ep_size *= drv->bulk_in->endpoint_companion->max_burst + 1;
    }


    drv->expected_packet_size = rx_ep_size;
    drv->pipeline_callback = usb_rtl815x_pipeline_callback;

    uint8_t rx_ep_address = drv->bulk_in->desc->endpoint_address;

    pipeline_t* rx_pipeline = pipeline_create(drv->expected_packet_size * 4);


    if(!usb_device_request(device,
                           interface,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_ENDPOINT,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_ENDPOINT_SETUP_PIPELINE,
                           rx_ep_size, rx_ep_address,
                           0, rx_pipeline)) {
        PRINTLOG(USB, LOG_ERROR, "cannot setup endpoint pipeline");


        return -1;
    }

    uint8_t int_ep_address = drv->intr->desc->endpoint_address;
    uint16_t int_ep_size = drv->intr->desc->max_packet_size;

    pipeline_t* int_pipeline = pipeline_create(int_ep_size * 32);


    if(!usb_device_request(device,
                           interface,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_ENDPOINT,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_ENDPOINT_SETUP_PIPELINE,
                           int_ep_size, int_ep_address,
                           0, int_pipeline)) {
        PRINTLOG(USB, LOG_ERROR, "cannot setup endpoint pipeline");


        return -1;
    }


    uint64_t tx_task_id =  task_create_task(NULL, 2 << 20, 64 << 10, usb_rtl815x_process_tx, 0, NULL, "usb-rtl815x-tx");

    if(tx_task_id == -1ULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot create tx task");
        return -1;
    }

    drv->tx_task_id = tx_task_id;

    list_list_insert(usb_rtl815x_drivers, drv);

    PRINTLOG(USB, LOG_INFO, "RTL8152/RTL8153 device initialized");

    return 0;
}
