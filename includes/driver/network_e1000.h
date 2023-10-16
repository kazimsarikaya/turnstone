/**
 * @file network_e1000.h
 * @brief Network e1000 header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_NETWORK_E1000_H
#define ___NETWORK_NETWORK_E1000_H 0

#include <types.h>
#include <pci.h>
#include <network/network_protocols.h>
#include <network/network_ethernet.h>

// mmio register offsets at bar0
#define NETWORK_E1000_REG_CTRL      0x0000
#define NETWORK_E1000_REG_STATUS    0x0008

#define NETWORK_E1000_REG_EECD      0x0010
#define NETWORK_E1000_REG_EERD      0x0014

#define NETWORK_E1000_REG_MDIC      0x0020

#define E100_REG_ISR_STATUS 0x00C0

#define NETWORK_E1000_REG_IMS       0x00D0

#define NETWORK_E1000_REG_RCTL      0x0100
#define NETWORK_E1000_REG_TCTL      0x0400

#define NETWORK_E1000_REG_RDBAL     0x2800
#define NETWORK_E1000_REG_RDBAH     0x2804
#define NETWORK_E1000_REG_RDLEN     0x2808
#define NETWORK_E1000_REG_RDH       0x2810
#define NETWORK_E1000_REG_RDT       0x2818

#define NETWORK_E1000_REG_TDBAL     0x3800
#define NETWORK_E1000_REG_TDBAH     0x3804
#define NETWORK_E1000_REG_TDLEN     0x3808
#define NETWORK_E1000_REG_TDH       0x3810
#define NETWORK_E1000_REG_TDT       0x3818

#define NETWORK_E1000_REG_MTA       0x5200

#define NETWORK_E1000_REG_RAL       0x5400
#define NETWORK_E1000_REG_RAH       0x5404
// endof mmio register offsets at bar0


#define NETWORK_E1000_PHYREG_PCTRL     0
#define NETWORK_E1000_PHYREG_PSTATUS   1
#define NETWORK_E1000_PHYREG_PSSTAT   17

#define NETWORK_E1000_NUM_RX_DESCRIPTORS  768
#define NETWORK_E1000_NUM_TX_DESCRIPTORS  768

#define NETWORK_E1000_CTRL_FD       (1 << 0)
#define NETWORK_E1000_CTRL_ASDE     (1 << 5)
#define NETWORK_E1000_CTRL_SLU      (1 << 6)

#define NETWORK_E1000_MDIC_PHYADD     (1 << 21)
#define NETWORK_E1000_MDIC_OP_WRITE   (1 << 26)
#define NETWORK_E1000_MDIC_OP_READ    (2 << 26)
#define NETWORK_E1000_MDIC_R          (1 << 28)
#define NETWORK_E1000_MDIC_I          (1 << 29)
#define NETWORK_E1000_MDIC_E          (1 << 30)

#define NETWORK_E1000_RCTL_EN           (1 << 1)
#define NETWORK_E1000_RCTL_SBP          (1 << 2)
#define NETWORK_E1000_RCTL_UPE          (1 << 3)
#define NETWORK_E1000_RCTL_MPE          (1 << 4)
#define NETWORK_E1000_RCTL_LPE          (1 << 5)
#define NETWORK_E1000_RDMTS_HALF        (0 << 8)
#define NETWORK_E1000_RDMTS_QUARTER     (1 << 8)
#define NETWORK_E1000_RDMTS_EIGHTH      (2 << 8)
#define NETWORK_E1000_RCTL_BAM          (1 << 15)
#define NETWORK_E1000_RCTL_BSIZE_256    (3 << 16)
#define NETWORK_E1000_RCTL_BSIZE_512    (2 << 16)
#define NETWORK_E1000_RCTL_BSIZE_1024   (1 << 16)
#define NETWORK_E1000_RCTL_BSIZE_2048   (0 << 16)
#define NETWORK_E1000_RCTL_BSIZE_4096   ((3 << 16) | (1 << 25))
#define NETWORK_E1000_RCTL_BSIZE_8192   ((2 << 16) | (1 << 25))
#define NETWORK_E1000_RCTL_BSIZE_16384  ((1 << 16) | (1 << 25))
#define NETWORK_E1000_RCTL_SECRC        (1 << 26)

#define NETWORK_E1000_TCTL_EN       (1 << 1)
#define NETWORK_E1000_TCTL_PSP      (1 << 3)

typedef struct {
    volatile uint32_t ctrl; ///< NETWORK_E1000_REG_CTRL

    uint8_t pad0[(NETWORK_E1000_REG_STATUS - NETWORK_E1000_REG_CTRL - sizeof(uint32_t))];

    volatile uint32_t status; ///< NETWORK_E1000_REG_STATUS

    uint8_t pad1[(NETWORK_E1000_REG_EECD - NETWORK_E1000_REG_STATUS - sizeof(uint32_t))];

    volatile uint32_t eecd; ///< NETWORK_E1000_REG_EECD
    volatile uint32_t eerd; ///< NETWORK_E1000_REG_EERD

    uint8_t pad2[(NETWORK_E1000_REG_MDIC - NETWORK_E1000_REG_EERD - sizeof(uint32_t))];

    volatile uint32_t mdic; ///< NETWORK_E1000_REG_MDIC

    uint8_t pad3[(E100_REG_ISR_STATUS - NETWORK_E1000_REG_MDIC - sizeof(uint32_t))];

    volatile uint32_t isr_status; ///< E100_REG_ISR_STATUS

    uint8_t pad4[(NETWORK_E1000_REG_IMS - E100_REG_ISR_STATUS - sizeof(uint32_t))];

    volatile uint32_t ims; ///< NETWORK_E1000_REG_IMS

    uint8_t pad5[(NETWORK_E1000_REG_RCTL - NETWORK_E1000_REG_IMS - sizeof(uint32_t))];

    volatile uint32_t rctl; ///< NETWORK_E1000_REG_RCTL

    uint8_t pad6[(NETWORK_E1000_REG_TCTL - NETWORK_E1000_REG_RCTL - sizeof(uint32_t))];

    volatile uint32_t tctl; ///< NETWORK_E1000_REG_TCTL

    uint8_t pad7[(NETWORK_E1000_REG_RDBAL - NETWORK_E1000_REG_TCTL - sizeof(uint32_t))];

    volatile uint32_t rdbal; ///< NETWORK_E1000_REG_RDBAL
    volatile uint32_t rdbah; ///< NETWORK_E1000_REG_RDBAH
    volatile uint32_t rdlen; ///< NETWORK_E1000_REG_RDLEN

    uint8_t pad8[(NETWORK_E1000_REG_RDH - NETWORK_E1000_REG_RDLEN - sizeof(uint32_t))];

    volatile uint32_t rdh; ///< NETWORK_E1000_REG_RDH

    uint8_t pad9[(NETWORK_E1000_REG_RDT - NETWORK_E1000_REG_RDH - sizeof(uint32_t))];

    volatile uint32_t rdt; ///< NETWORK_E1000_REG_RDT

    uint8_t pad10[(NETWORK_E1000_REG_TDBAL - NETWORK_E1000_REG_RDT - sizeof(uint32_t))];

    volatile uint32_t tdbal; ///< NETWORK_E1000_REG_TDBAL
    volatile uint32_t tdbah; ///< NETWORK_E1000_REG_TDBAH
    volatile uint32_t tdlen; ///< NETWORK_E1000_REG_TDLEN

    uint8_t pad11[(NETWORK_E1000_REG_TDH - NETWORK_E1000_REG_TDLEN - sizeof(uint32_t))];

    volatile uint32_t tdh; ///< NETWORK_E1000_REG_TDH

    uint8_t pad12[(NETWORK_E1000_REG_TDT - NETWORK_E1000_REG_TDH - sizeof(uint32_t))];

    volatile uint32_t tdt; ///< NETWORK_E1000_REG_TDT

    uint8_t pad13[(NETWORK_E1000_REG_MTA - NETWORK_E1000_REG_TDT - sizeof(uint32_t))];

    volatile uint32_t mta[128]; ///< NETWORK_E1000_REG_MTA

    uint8_t pad14[(NETWORK_E1000_REG_RAL - NETWORK_E1000_REG_MTA - sizeof(uint32_t) * 128)];

    volatile uint32_t ral; ///< NETWORK_E1000_REG_RAL
    volatile uint32_t rah; ///< NETWORK_E1000_REG_RAH
}__attribute__((packed)) network_e1000_mmio_t;

typedef struct network_e1000_rx_desc_s {
    volatile uint64_t address;

    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t  status;
    volatile uint8_t  errors;
    volatile uint16_t special;
} __attribute__((packed)) network_e1000_rx_desc_t;

typedef struct network_e1000_tx_desc_s {
    volatile uint64_t address;

    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  sta;
    volatile uint8_t  css;
    volatile uint16_t special;
} __attribute__((packed)) network_e1000_tx_desc_t;

typedef struct {
    const pci_dev_t*      pci_netdev;
    network_mac_address_t mac;
    linkedlist_t*         transmit_queue;
    uint8_t               irq_base;

    uint64_t rx_count;
    uint64_t tx_count;
    uint64_t packets_dropped;

    network_e1000_mmio_t* mmio;

    volatile network_e1000_rx_desc_t* rx_desc; // receive descriptor buffer
    volatile uint16_t                 rx_tail;

    volatile network_e1000_tx_desc_t* tx_desc; // transmit descriptor buffer
    volatile uint16_t                 tx_tail;
}network_e1000_dev_t;

int8_t network_e1000_init(const pci_dev_t* pci_netdev);

#endif
