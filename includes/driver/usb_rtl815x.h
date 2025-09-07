/**
 * @file usb_rtl815x.h
 * @brief USB RTL815X driver header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___USB_RTL815X_H
#define ___USB_RTL815X_H

#include <types.h>
#include <utils.h>
#include <driver/usb_vendor.h>

#define RTL8152_REQ_GET_REGS    0x05
#define RTL8152_REQ_SET_REGS    0x05

#define RTL815X_PLA_BASE    0x0100
#define RTL815X_USB_BASE    0x0000

#define RTL815X_PLA_IDR             0xc000
#define RTL815X_PLA_MAC             RTL815X_PLA_IDR
#define RTL815X_PLA_RCR             0xc010
#define RTL815X_PLA_RMS             0xc016
#define RTL815X_PLA_RXFIFO_CTRL0    0xc0a0
#define RTL815X_PLA_RXFIFO_CTRL1    0xc0a4
#define RTL815X_PLA_RXFIFO_CTRL2    0xc0a8
#define RTL815X_PLA_FMC             0xc0b4
#define RTL815X_PLA_MAR             0xcd00
#define RTL815X_PLA_EXTRA_STATUS    0xd398
#define RTL815X_PLA_EEE_CR          0xe040
#define RTL815X_PLA_MAC_PWR_CTRL    0xe0c0
#define RTL815X_PLA_MAC_PWR_CTRL2   0xe0ca
#define RTL815X_PLA_MAC_PWR_CTRL3   0xe0cc
#define RTL815X_PLA_MAC_PWR_CTRL4   0xe0ce
#define RTL815X_PLA_TCR0            0xe610
#define RTL815X_PLA_VERSION         0xe612
#define RTL815X_PLA_MTPS            0xe615
#define RTL815X_PLA_TXFIFO_CTRL     0xe618
#define RTL815X_PLA_RSTTALLY        0xe800
#define RTL815X_PLA_CR              0xe813
#define RTL815X_PLA_PHY_PWR         0xe84c
#define RTL815X_PLA_OCP_GPHY_BASE   0xe86c
#define RTL815X_PLA_OOB_CTRL        0xe84f
#define RTL815X_PLA_CPCR            0xe854
#define RTL815X_PLA_PHYSTATUS       0xe908

#define RTL815X_USB_CSR_DUMMY1          0xb464
#define RTL815X_USB_CSR_DUMMY2          0xb466
#define RTL815X_USB_BURST_SIZE          0xcfc0
#define RTL815X_USB_CTRL                0xd406
#define RTL815X_USB_LPM_CTRL            0xd41a
#define RTL815X_USB_RX_EARLY_TIMEOUT    0xd42c
#define RTL815X_USB_RX_EARLY_SIZE       0xd42e
#define RTL815X_USB_BMU_RESET           0xd4b0
#define RTL815X_USB_BMU_CONFIG          0xd4b4
#define RTL815X_USB_AFE_CTRL2           0xd824
#define RTL815X_USB_WDT11_CTRL          0xe43c

/* OCP registers */
#define RTL815X_OCP_BASE_MII        0xa400
#define RTL815X_OCP_PHY_STATUS      0xa420
#define RTL815X_OCP_POWER_CFG       0xa430
#define RTL815X_OCP_EEE_CFG         0xa432
#define RTL815X_OCP_SRAM_ADDR       0xa436
#define RTL815X_OCP_SRAM_DATA       0xa438
#define RTL815X_OCP_DOWN_SPEED      0xa442
#define RTL815X_OCP_EEE_ADV         0xa5d0

#define RTL815X_CR_RST      0x10
#define RTL815X_CR_RE       0x08
#define RTL815X_CR_TE       0x04

#define RTL815X_RCR_AAP         0x00000001
#define RTL815X_RCR_APM         0x00000002
#define RTL815X_RCR_AM          0x00000004
#define RTL815X_RCR_AB          0x00000008
#define RTL815X_RCR_ACPT_ALL    (RTL815X_RCR_AAP | RTL815X_RCR_APM | RTL815X_RCR_AM | RTL815X_RCR_AB)
// #define RTL815X_SLOT_EN     BIT(11)

#define RTL815X_RX_AGG_DISABLE    0x0010
#define RTL815X_RX_ZERO_EN        0x0080

/* PLA_EXTRA_STATUS */
#define RTL815X_CUR_LINK_OK        BIT(15)
#define RTL815X_U3P3_CHECK_EN      BIT(7) /* RTL_VER_05 only */
#define RTL815X_LINK_CHANGE_FLAG   BIT(8)
#define RTL815X_POLL_LINK_CHG      BIT(0)

/* USB_CSR_DUMMY1 */
#define RTL815X_DYNAMIC_BURST       0x0001

/* USB_CSR_DUMMY2 */
#define RTL815X_EP4_FULL_FC         0x0001

/* PLA_TCR1 */
#define RTL815X_VERSION_MASK        0x7cf0

/* PLA_EEE_CR */
#define RTL815X_EEE_RX_EN   0x0001
#define RTL815X_EEE_TX_EN   0x0002

/* PLA_WDT6_CTRL */
#define RTL815X_WDT6_SET_MODE           0x0010

/* USB_BMU_RESET */
#define RTL815X_BMU_RESET_EP_IN   0x01
#define RTL815X_BMU_RESET_EP_OUT  0x02

/* PLA_PHY_PWR */
#define RTL815X_TX_10M_IDLE_EN    0x0080
#define RTL815X_PFM_PWM_SWITCH    0x0040
#define RTL815X_TEST_IO_OFF       BIT(4)

/* PLA_CPCR */
#define RTL815X_FLOW_CTRL_EN       BIT(0)
#define RTL815X_CPCR_RX_VLAN       0x0040

#define RTL815X_FMC_FCR_MCU_EN             0x0001

#define RTL815X_TCR0_AUTO_FIFO     0x0080

#define RTL815X_TXFIFO_THR_NORMAL2 0x01000008

#define RTL815X_RXFIFO_THR1_NORMAL  0x00080002
#define RTL815X_RXFIFO_THR2_NORMAL  0x00a0
#define RTL815X_RXFIFO_THR3_NORMAL  0x0110


#define RTL815X_NOW_IS_OOB         0x0080
#define RTL815X_LINK_LIST_READY    0x0002

#define RTL815X_BYTE_EN_START_MASK  0x000f
#define RTL815X_BYTE_EN_END_MASK    0x00f0
#define RTL815X_BYTE_EN_BYTE        0x11
#define RTL815X_BYTE_EN_WORD        0x33
#define RTL815X_BYTE_EN_SIX_BYTES   0x3f
#define RTL815X_BYTE_EN_DWORD       0xff

#define RTL815X_MTPS_JUMBO      (12 * 1024 / 64)
#define RTL815X_MTPS_DEFAULT    (6 * 1024 / 64)


/* USB_LPM_CTRL */
/* bit 4 ~ 5: fifo empty boundary */
#define RTL815X_FIFO_EMPTY_1FB    0x30 /* 0x1fb * 64 = 32448 bytes */
/* bit 2 ~ 3: LMP timer */
#define RTL815X_LPM_TIMER_MASK    0x0c
#define RTL815X_LPM_TIMER_500MS   0x04 /* 500 ms */
#define RTL815X_LPM_TIMER_500US   0x0c /* 500 us */
#define RTL815X_ROK_EXIT_LPM      0x02


/* OCP_PHY_STATUS */
#define RTL815X_PHY_STAT_MASK       0x0007
#define RTL815X_PHY_STAT_EXT_INIT   2
#define RTL815X_PHY_STAT_LAN_ON     3
#define RTL815X_PHY_STAT_PWRDN      5

/* OCP_POWER_CFG */
#define RTL815X_EEE_CLKDIV_EN   0x8000
#define RTL815X_EN_ALDPS        0x0004
#define RTL815X_EN_10M_PLLOFF   0x0001
#define RTL815X_EN_10M_BGOFF    0x0080

/* OCP_EEE_CFG */
#define RTL815X_CTAP_SHORT_EN   0x0040
#define RTL815X_EEE10_EN        0x0010

/* USB_RX_EARLY_TIMEOUT */
#define RTL815X_COALESCE_SUPER      85000U
#define RTL815X_COALESCE_HIGH       250000U
#define RTL815X_COALESCE_SLOW       524280U


/* USB_AFE_CTRL2 */
#define RTL815X_SEN_VAL_MASK        0xf800
#define RTL815X_SEN_VAL_NORMAL      0xa000
#define RTL815X_SEL_RXIDLE          0x0100

/* USB_WDT11_CTRL */
#define RTL815X_TIMER11_EN          0x0001


#ifdef __cplusplus
extern "C" {
#endif

typedef enum usb_rtl815x_register_content_t {
    RTL815X_2500BPS  = BIT(10),
    RTL815X_1250BPS  = BIT(9),
    RTL815X_500BPS   = BIT(8),
    RTL815X_TX_FLOW  = BIT(6),
    RTL815X_RX_FLOW  = BIT(5),
    RTL815X_1000BPS  = 0X10,
    RTL815X_100BPS   = 0X08,
    RTL815X_10BPS    = 0X04,
    RTL815X_LINK_STATUS = 0X02,
    RTL815X_FULL_DUP  = 0X01,
} usb_rtl815x_register_content_t;

#ifdef __cplusplus
}
#endif

#endif
