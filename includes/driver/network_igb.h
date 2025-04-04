/**
 * @file network_igb.h
 * @brief Network igb header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_NETWORK_IGB_H
#define ___NETWORK_NETWORK_IGB_H 0

#include <types.h>
#include <pci.h>
#include <network/network_protocols.h>
#include <network/network_ethernet.h>

#ifdef __cplusplus
extern "C" {
#endif

// mmio register offsets at bar0
#define NETWORK_IGB_REG_CTRL      0x00000
#define NETWORK_IGB_REG_STATUS    0x00008
#define NETWORK_IGB_REG_CTRL_EXT  0x00018

#define NETWORK_IGB_REG_EECD      0x0010
#define NETWORK_IGB_REG_EERD      0x0014

#define NETWORK_IGB_REG_MDIC      0x0020

#define NETWORK_IGB_REG_VLAN_ETHER_TYPE 0x0038

#define NETWORK_IGB_REG_ICR       0x00C0

#define NETWORK_IGB_REG_IMS       0x00D0
#define NETWORK_IGB_REG_IMC       0x00D8

#define NETWORK_IGB_REG_RCTL      0x0100
#define NETWORK_IGB_REG_TCTL      0x0400

#define NETWORK_IGB_REG_GPIE      0x1514
#define NETWORK_IGB_REG_EIMS      0x1524
#define NETWORK_IGB_REG_EIMC      0x1528
#define NETWORK_IGB_REG_EIAC      0x152C
#define NETWORK_IGB_REG_EICR      0x1580
#define NETWORK_IGB_REG_IVAR      0x1700
#define NETWORK_IGB_REG_IVAR_MISC 0x1740

#define NETWORK_IGB_REG_RDBAL     0xC000
#define NETWORK_IGB_REG_RDBAH     0xC004
#define NETWORK_IGB_REG_RDLEN     0xC008
#define NETWORK_IGB_REG_SRRCTL    0xC00C
#define NETWORK_IGB_REG_RDH       0xC010
#define NETWORK_IGB_REG_RDT       0xC018
#define NETWORK_IGB_REG_RXDCTL    0xC028

#define NETWORK_IGB_REG_TDBAL     0xE000
#define NETWORK_IGB_REG_TDBAH     0xE004
#define NETWORK_IGB_REG_TDLEN     0xE008
#define NETWORK_IGB_REG_TDH       0xE010
#define NETWORK_IGB_REG_TDT       0xE018
#define NETWORK_IGB_REG_TXDCTL    0xE028

#define NETWORK_IGB_REG_MTA       0x5200

#define NETWORK_IGB_REG_RAL       0x5400
#define NETWORK_IGB_REG_RAH       0x5404

#define NETWORK_IGB_REG_SWSM          0x5B50
#define NETWORK_IGB_REG_FWSM          0x5B54
#define NETWORK_IGB_REG_SWFWSYNC      0x5B5C

// endof mmio register offsets at bar0


#define NETWORK_IGB_PHYREG_PCTRL     0
#define NETWORK_IGB_PHYREG_PSTATUS   1
#define NETWORK_IGB_PHYREG_PSSTAT   17

#define NETWORK_IGB_NUM_RX_DESCRIPTORS  1024
#define NETWORK_IGB_NUM_TX_DESCRIPTORS  1024

#define NETWORK_IGB_RX_BUFFER_SIZE (10 * 1024)
#define NETWORK_IGB_RX_HEADER_SIZE 256

#define NETWORK_IGB_CTRL_FD       (1 << 0)
#define NETWORK_IGB_CTRL_ASDE     (1 << 5)
#define NETWORK_IGB_CTRL_SLU      (1 << 6)
#define NETWORK_IGB_CTRL_RST      (1 << 26)
#define NETWORK_IGB_CTRL_RFCE     (1 << 27)
#define NETWORK_IGB_CTRL_TFCE     (1 << 28)

#define NETWORK_IGB_MDIC_PHYADD     (1 << 21)
#define NETWORK_IGB_MDIC_OP_WRITE   (1 << 26)
#define NETWORK_IGB_MDIC_OP_READ    (2 << 26)
#define NETWORK_IGB_MDIC_R          (1 << 28)
#define NETWORK_IGB_MDIC_I          (1 << 29)
#define NETWORK_IGB_MDIC_E          (1 << 30)

#define NETWORK_IGB_RCTL_EN           (1 << 1)
#define NETWORK_IGB_RCTL_SBP          (1 << 2)
#define NETWORK_IGB_RCTL_UPE          (1 << 3)
#define NETWORK_IGB_RCTL_MPE          (1 << 4)
#define NETWORK_IGB_RCTL_LPE          (1 << 5)
#define NETWORK_IGB_RDMTS_HALF        (0 << 8)
#define NETWORK_IGB_RDMTS_QUARTER     (1 << 8)
#define NETWORK_IGB_RDMTS_EIGHTH      (2 << 8)
#define NETWORK_IGB_RCTL_BAM          (1 << 15)
#define NETWORK_IGB_RCTL_SECRC        (1 << 26)
#define NETWORK_IGB_RXDCTL_ENABLE     (1 << 25)

#define NETWORK_IGB_TCTL_EN       (1 << 1)
#define NETWORK_IGB_TCTL_PSP      (1 << 3)
#define NETWORK_IGB_TXDCTL_ENABLE (1 << 25)

typedef union network_igb_rx_desc_t {
    struct {
        uint64_t pkt_addr; /* Packet buffer address */
        uint64_t hdr_addr; /* Header buffer address */
    } read;
    struct {
        struct {
            struct {
                uint16_t pkt_info; /* RSS type, Packet type */
                uint16_t hdr_info; /* Split Head, buf len */
            } lo_dword;
            union {
                uint32_t rss; /* RSS Hash */
                struct {
                    uint16_t ip_id; /* IP id */
                    uint16_t csum; /* Packet Checksum */
                } csum_ip;
            } hi_dword;
        } lower;
        struct {
            uint32_t status_error; /* ext status/error */
            uint16_t length; /* Packet length */
            uint16_t vlan; /* VLAN tag */
        } upper;
    } wb; /* writeback */
} __attribute__((packed)) network_igb_rx_desc_t;

_Static_assert(sizeof(network_igb_rx_desc_t) == 2 * sizeof(uint64_t), "network_igb_rx_desc_t size is not 16");

typedef struct network_igb_tx_desc_t {
    volatile uint64_t address;
    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  status;
    volatile uint8_t  css;
    volatile uint16_t vlan;
} __attribute__((packed)) network_igb_tx_desc_t;

typedef struct {
    const pci_dev_t*       pci_netdev;
    pci_capability_msix_t* msix_cap;
    network_mac_address_t  mac;
    list_t*                return_queue;
    uint8_t                rx_isr;
    uint8_t                tx_isr;
    uint8_t                other_isr;

    uint64_t rx_task_id;

    uint64_t rx_count;
    uint64_t tx_count;
    uint64_t packets_dropped;

    uint64_t mmio_va;

    uint64_t rx_packet_buffer_fa;
    uint64_t rx_packet_buffer_va;
    uint64_t rx_header_buffer_fa;
    uint64_t rx_header_buffer_va;


    volatile network_igb_rx_desc_t* rx_desc; // receive descriptor buffer
    volatile int32_t                rx_tail;

    volatile network_igb_tx_desc_t* tx_desc; // transmit descriptor buffer
    volatile int32_t                tx_tail;
}network_igb_dev_t;

int8_t network_igb_init(const pci_dev_t* pci_netdev);

#ifdef __cplusplus
}
#endif

#endif
