/**
 * @file usb_xhci.h
 * @brief USB XHCI driver header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___USB_XHCI_H
#define ___USB_XHCI_H



#include <types.h>
#include <driver/usb.h>
#include <pci.h>
#include <utils.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct usb_xhci_capabilities_t {
    uint32_t          caplength_and_revision;
    volatile uint32_t hcs_params_1;
    volatile uint32_t hcs_params_2;
    volatile uint32_t hcs_params_3;
    volatile uint32_t hcc_params_1;
    volatile uint32_t dboff;
    volatile uint32_t rtsoff;
    volatile uint32_t hcc_params_2;
} __attribute__((packed)) usb_xhci_capabilities_t;

typedef union usb_xhci_caplen_rev_t {
    volatile uint32_t raw;
    struct {
        volatile uint8_t  capability_length;
        volatile uint8_t  reserved;
        volatile uint16_t usb_revision;
    } __attribute__((packed)) bits;
} usb_xhci_caplen_rev_t;

typedef union usb_xhci_hcs_params_1_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t max_slots      : 8;
        volatile uint32_t max_interrupts : 11;
        volatile uint32_t reserved       : 5;
        volatile uint32_t max_ports      : 8;
    } __attribute__((packed)) bits;
} usb_xhci_hcs_params_1_t;

_Static_assert(sizeof(usb_xhci_hcs_params_1_t) == 4, "usb_xhci_hcs_params_1_t is not 4 bytes");

typedef union usb_xhci_hcs_params_2_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t ist                    : 4;
        volatile uint32_t erst_max               : 4;
        volatile uint32_t reserved1              : 13;
        volatile uint32_t max_scratchpad_bufs_hi : 5;
        volatile uint32_t scratchpad_restore     : 1;
        volatile uint32_t max_scratchpad_bufs_lo : 5;
    } __attribute__((packed)) bits;
} usb_xhci_hcs_params_2_t;

_Static_assert(sizeof(usb_xhci_hcs_params_2_t) == 4, "usb_xhci_hcs_params_2_t is not 4 bytes");

typedef union usb_xhci_hcs_params_3_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t u1_device_exit_latency : 8;
        volatile uint32_t reserved1              : 8;
        volatile uint32_t u2_device_exit_latency : 16;
    } __attribute__((packed)) bits;
} usb_xhci_hcs_params_3_t;

_Static_assert(sizeof(usb_xhci_hcs_params_3_t) == 4, "usb_xhci_hcs_params_3_t is not 4 bytes");

typedef union usb_xhci_hcc_params_1_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t ac64       : 1;
        volatile uint32_t bnc        : 1;
        volatile uint32_t csz        : 1;
        volatile uint32_t ppc        : 1;
        volatile uint32_t pind       : 1;
        volatile uint32_t lhrc       : 1;
        volatile uint32_t ltc        : 1;
        volatile uint32_t nss        : 1;
        volatile uint32_t pae        : 1;
        volatile uint32_t spc        : 1;
        volatile uint32_t sec        : 1;
        volatile uint32_t cfc        : 1;
        volatile uint32_t maxpsasize : 4;
        volatile uint32_t xecp       : 16;
    } __attribute__((packed)) bits;
} usb_xhci_hcc_params_1_t;

_Static_assert(sizeof(usb_xhci_hcc_params_1_t) == 4, "usb_xhci_hcc_params_t is not 4 bytes");

typedef union usb_xhci_hcc_params_2_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t u3c       : 1;
        volatile uint32_t cmc       : 1;
        volatile uint32_t fsc       : 1;
        volatile uint32_t ctc       : 1;
        volatile uint32_t lec       : 1;
        volatile uint32_t cic       : 1;
        volatile uint32_t etc       : 1;
        volatile uint32_t etc_tsc   : 1;
        volatile uint32_t gsc       : 1;
        volatile uint32_t vtc       : 1;
        volatile uint32_t reserved1 : 22;
    } __attribute__((packed)) bits;
} usb_xhci_hcc_params_2_t;

_Static_assert(sizeof(usb_xhci_hcc_params_2_t) == 4, "usb_xhci_hcc_params_2_t is not 4 bytes");

typedef struct usb_xhci_port_register_set_t {
    volatile uint32_t portsc; ///< Port Status and Control
    volatile uint32_t portpmsc; ///< Port Power Management Status and Control
    volatile uint32_t portli; ///< Port Link Info
    volatile uint32_t porthlpmc; ///< Port Hardware LPM Control
} __attribute__((packed)) usb_xhci_port_register_set_t;

_Static_assert(sizeof(usb_xhci_port_register_set_t) == 16, "usb_xhci_port_register_set_t is not 16 bytes");

typedef struct usb_xhci_operational_registers_t {
    volatile uint32_t                     usbcmd;
    volatile uint32_t                     usbsts;
    volatile uint32_t                     pagesize;
    volatile uint32_t                     reserved1[2];
    volatile uint32_t                     dnctrl;
    volatile uint64_t                     crcr;
    volatile uint32_t                     reserved2[4];
    volatile uint64_t                     dcbaap;
    volatile uint32_t                     config;
    volatile uint32_t                     reserved3[0xf1];
    volatile usb_xhci_port_register_set_t portsc[0];
} __attribute__((packed)) usb_xhci_operational_registers_t;

_Static_assert(offsetof_field(usb_xhci_operational_registers_t, portsc) == 0x400, "usb_xhci_operational_registers_t is not 0x400 bytes");
_Static_assert(sizeof(usb_xhci_operational_registers_t) == 0x400, "usb_xhci_operational_registers_t is not 0x400 bytes");

typedef union usb_xhci_usbcmd_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t run_stop   : 1; ///< Run/Stop
        volatile uint32_t hcrst      : 1; ///< Host Controller Reset
        volatile uint32_t int_enable : 1; ///< Interrupter Enable
        volatile uint32_t hsee       : 1; ///< Host System Error Enable
        volatile uint32_t reserved1  : 3;
        volatile uint32_t lhcrst     : 1; ///< Light Host Controller Reset
        volatile uint32_t css        : 1; ///< Controller Save State
        volatile uint32_t crs        : 1; ///< Controller Restore State
        volatile uint32_t ewe        : 1; ///< Enable Wrap Event
        volatile uint32_t eu3s       : 1; ///< Enable U3 Suspend
        volatile uint32_t reserved2  : 1;
        volatile uint32_t cme        : 1; ///< Configure Memory Enable??
        volatile uint32_t ete        : 1; ///< Extended TBC Enable
        volatile uint32_t tsc_en     : 1; ///< Extended TBC TRB Status Control Enable
        volatile uint32_t vtioe      : 1; ///< VTIO Enable
        volatile uint32_t reserved3  : 15;
    } __attribute__((packed)) bits;
} usb_xhci_usbcmd_t;

_Static_assert(sizeof(usb_xhci_usbcmd_t) == 4, "usb_xhci_usbcmd_t is not 4 bytes");

typedef union usb_xhci_usbsts_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t hch       : 1; ///< Host Controller Halted
        volatile uint32_t reserved1 : 1;
        volatile uint32_t hse       : 1; ///< Host System Error
        volatile uint32_t eint      : 1; ///< Event Interrupt
        volatile uint32_t pcd       : 1; ///< Port Change Detect
        volatile uint32_t reserved2 : 3;
        volatile uint32_t sss       : 1; ///< Save State Status
        volatile uint32_t rss       : 1; ///< Restore State Status
        volatile uint32_t sre       : 1; ///< Save/Restore Error
        volatile uint32_t cnr       : 1; ///< Controller Not Ready
        volatile uint32_t hce       : 1; ///< Host Controller Error
        volatile uint32_t reserved3 : 19;
    } __attribute__((packed)) bits;
} usb_xhci_usbsts_t;

_Static_assert(sizeof(usb_xhci_usbsts_t) == 4, "usb_xhci_usbsts_t is not 4 bytes");

typedef union usb_xhci_crcr_t {
    volatile uint64_t raw;
    struct {
        volatile uint64_t rcs       : 1; ///< Ring Cycle State
        volatile uint64_t cs        : 1; ///< Command Stop
        volatile uint64_t ca        : 1; ///< Command Abort
        volatile uint64_t crr       : 1; ///< Command Ring Running
        volatile uint64_t reserved1 : 2;
        volatile uint64_t ptr       : 58; ///< Command Ring Pointer
    } __attribute__((packed)) bits;
} usb_xhci_crcr_t;

_Static_assert(sizeof(usb_xhci_crcr_t) == 8, "usb_xhci_crcr_t is not 8 bytes");

typedef union usb_xhci_config_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t max_slots_en : 8; ///< Max Slots Enabled
        volatile uint32_t u3c          : 1; ///< U3 Device Capability
        volatile uint32_t cie          : 1; ///< Controller Interface Enable
        volatile uint32_t reserved1    : 22;
    } __attribute__((packed)) bits;
} usb_xhci_config_t;

_Static_assert(sizeof(usb_xhci_config_t) == 4, "usb_xhci_config_t is not 4 bytes");

typedef union usb_xhci_portsc_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t ccs       :1; ///< Current Connect Status
        volatile uint32_t ped       :1; ///< Port Enabled Disabled
        volatile uint32_t reserved0 :1;
        volatile uint32_t oca       :1; ///< Over Current Active
        volatile uint32_t pr        :1; ///< Port Reset
        volatile uint32_t pls       :4; ///< Port Link State
        volatile uint32_t pp        :1; ///< Port Power
        volatile uint32_t ps        :4; ///< Port Speed
        volatile uint32_t pic       :2; ///< Port Indicator Control
        volatile uint32_t lws       :1; ///< Port Link State Write Strobe
        volatile uint32_t csc       :1; ///< Port Connect Status Change
        volatile uint32_t pec       :1; ///< Port Enabled Disabled Change
        volatile uint32_t wrc       :1; ///< Port Warm Reset Change
        volatile uint32_t occ       :1; ///< Port Over Current Change
        volatile uint32_t prc       :1; ///< Port Reset Change
        volatile uint32_t plc       :1; ///< Port Link State Change
        volatile uint32_t cec       :1; ///< Port Config Error Change
        volatile uint32_t cas       :1; ///< Port Attach Status
        volatile uint32_t wce       :1; ///< Port Wake on Connect Enable
        volatile uint32_t wde       :1; ///< Port Wake on Disconnect Enable
        volatile uint32_t woe       :1; ///< Port Wake on Over Current Enable
        volatile uint32_t reserved1 :2;
        volatile uint32_t dr        :1; ///< Device Removable
        volatile uint32_t wpr       :1; ///< Warm Port Reset
    } __attribute__((packed)) bits;
} usb_xhci_portsc_t;

_Static_assert(sizeof(usb_xhci_portsc_t) == 4, "usb_xhci_portsc_t is not 4 bytes");

typedef union usb_xhci_portpmsc_usb3_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t u1_timeout : 8; ///< U1 Timeout
        volatile uint32_t u2_timeout : 8; ///< U2 Timeout
        volatile uint32_t fla        :1; ///< Force Link PM Accept
        volatile uint32_t reserved0  :15;
    } __attribute__((packed)) bits;
} usb_xhci_portpmsc_usb3_t;

_Static_assert(sizeof(usb_xhci_portpmsc_usb3_t) == 4, "usb_xhci_portpmsc_usb3_t is not 4 bytes");

typedef union usb_xhci_portpmsc_usb2_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t l1s               :3; ///< L1 Status
        volatile uint32_t rwe               :1; ///< Remote Wakeup Enable
        volatile uint32_t besl              :4; ///< Best Effort Service Latency
        volatile uint32_t l1_device_slot    :8; ///< L1 Device Slot
        volatile uint32_t hle               :1; ///< Hardware LPM Enable
        volatile uint32_t reserved0         :11;
        volatile uint32_t port_test_control : 4; ///< Port Test Control
    } __attribute__((packed)) bits;
} usb_xhci_portpmsc_usb2_t;

_Static_assert(sizeof(usb_xhci_portpmsc_usb2_t) == 4, "usb_xhci_portpmsc_usb2_t is not 4 bytes");

typedef union usb_xhci_portpmsc_t {
    volatile usb_xhci_portpmsc_usb3_t usb3;
    volatile usb_xhci_portpmsc_usb2_t usb2;
} usb_xhci_portpmsc_t;

_Static_assert(sizeof(usb_xhci_portpmsc_t) == 4, "usb_xhci_portpmsc_t is not 4 bytes");

typedef union usb_xhci_portli_usb3_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t link_error_count: 16; ///< Link Error Count
        volatile uint32_t rlc             :4; ///< Rx Lane Count
        volatile uint32_t tlc             :4; ///< Tx Lane Count
        volatile uint32_t reserved0       :8;
    } __attribute__((packed)) bits;
} usb_xhci_portli_usb3_t;

_Static_assert(sizeof(usb_xhci_portli_usb3_t) == 4, "usb_xhci_portli_usb3_t is not 4 bytes");

typedef union usb_xhci_portli_usb2_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t reserved0:32; ///< Reserved
    } __attribute__((packed)) bits;
} usb_xhci_portli_usb2_t;

_Static_assert(sizeof(usb_xhci_portli_usb2_t) == 4, "usb_xhci_portli_usb2_t is not 4 bytes");

typedef union usb_xhci_portli_t {
    volatile usb_xhci_portli_usb3_t usb3;
    volatile usb_xhci_portli_usb2_t usb2;
} usb_xhci_portli_t;

_Static_assert(sizeof(usb_xhci_portli_t) == 4, "usb_xhci_portli_t is not 4 bytes");

typedef union usb_xhci_porthlpmc_usb3_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t link_soft_error_count: 16; ///< Link Soft Error Count
        volatile uint32_t reserved0            :16;
    } __attribute__((packed)) bits;
} usb_xhci_porthlpmc_usb3_t;

_Static_assert(sizeof(usb_xhci_porthlpmc_usb3_t) == 4, "usb_xhci_porthlpmc_usb3_t is not 4 bytes");

typedef union usb_xhci_porthlpmc_usb2_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t hirdm     :2; ///< Host Initiated Resume Duration Mode
        volatile uint32_t l1_timeout:8; ///< L1 Timeout
        volatile uint32_t besld     :4; ///< Best Effort Service Latency Deep
        volatile uint32_t reserved0 :18;
    } __attribute__((packed)) bits;
} usb_xhci_porthlpmc_usb2_t;

_Static_assert(sizeof(usb_xhci_porthlpmc_usb2_t) == 4, "usb_xhci_porthlpmc_usb2_t is not 4 bytes");

typedef union usb_xhci_porthlpmc_t {
    volatile usb_xhci_porthlpmc_usb3_t usb3;
    volatile usb_xhci_porthlpmc_usb2_t usb2;
} usb_xhci_porthlpmc_t;

_Static_assert(sizeof(usb_xhci_porthlpmc_t) == 4, "usb_xhci_porthlpmc_t is not 4 bytes");

typedef struct usb_xhci_trb_t {
    volatile uint64_t parameter; ///< TRB-specific parameter
    volatile uint32_t status; ///< Status field
    volatile uint32_t control; ///< Control field (TRB type, cycle bit)
} __attribute__((packed)) usb_xhci_trb_t;

_Static_assert(sizeof(usb_xhci_trb_t) == 16, "usb_xhci_trb_t is not 16 bytes");

typedef struct usb_xhci_interrupter_t {
    volatile uint32_t iman; ///< Interrupter Management
    volatile uint32_t imod; ///< Interrupter Moderation
    volatile uint32_t erstsz; ///< Event Ring Segment Table Size
    volatile uint32_t reserved;
    volatile uint64_t erstba; ///< Event Ring Segment Table Base Address
    volatile uint64_t erdp; ///< Event Ring Dequeue Pointer
} __attribute__((packed)) usb_xhci_interrupter_t;

_Static_assert(sizeof(usb_xhci_interrupter_t) == 0x20, "usb_xhci_interrupter_t is not 32 bytes");

typedef struct usb_xhci_runtime_registers_t {
    volatile uint32_t               mfindex; ///< Microframe Index
    volatile uint32_t               reserved[7];
    volatile usb_xhci_interrupter_t interrupters[0]; ///< Array of interrupters
} __attribute__((packed)) usb_xhci_runtime_registers_t;

typedef struct usb_xhci_doorbell_t {
    volatile uint32_t db; ///< Doorbell Register
} __attribute__((packed)) usb_xhci_doorbell_t;

_Static_assert(sizeof(usb_xhci_doorbell_t) == 4, "usb_xhci_doorbell_t is not 4 bytes");

typedef struct usb_xhci_stream_context_t {
    volatile uint64_t dequeu_address; ///< Dequeue Pointe
    volatile uint32_t stopped_edtla; ///< Stopped Endpoint Dequeue Logical Address
    volatile uint32_t reserved;
} __attribute__((packed)) usb_xhci_stream_context_t;

_Static_assert(sizeof(usb_xhci_stream_context_t) == 16, "usb_xhci_stream_context_t is not 16 bytes");

typedef enum usb_xhci_trb_type_t {
    USB_XHCI_TRB_TYPE_TRB_RESERVED = 0,
    USB_XHCI_TRB_TYPE_TR_NORMAL,
    USB_XHCI_TRB_TYPE_TR_SETUP,
    USB_XHCI_TRB_TYPE_TR_DATA,
    USB_XHCI_TRB_TYPE_TR_STATUS,
    USB_XHCI_TRB_TYPE_TR_ISOCH, ///< 5
    USB_XHCI_TRB_TYPE_TR_LINK,
    USB_XHCI_TRB_TYPE_TR_EVDATA,
    USB_XHCI_TRB_TYPE_TR_NOOP,
    USB_XHCI_TRB_TYPE_CR_ENABLE_SLOT,
    USB_XHCI_TRB_TYPE_CR_DISABLE_SLOT, ///< 10
    USB_XHCI_TRB_TYPE_CR_ADDRESS_DEVICE,
    USB_XHCI_TRB_TYPE_CR_CONFIGURE_ENDPOINT,
    USB_XHCI_TRB_TYPE_CR_EVALUATE_CONTEXT,
    USB_XHCI_TRB_TYPE_CR_RESET_ENDPOINT,
    USB_XHCI_TRB_TYPE_CR_STOP_ENDPOINT, ///< 15
    USB_XHCI_TRB_TYPE_CR_SET_TR_DEQUEUE,
    USB_XHCI_TRB_TYPE_CR_RESET_DEVICE,
    USB_XHCI_TRB_TYPE_CR_FORCE_EVENT,
    USB_XHCI_TRB_TYPE_CR_NEGOTIATE_BW,
    USB_XHCI_TRB_TYPE_CR_SET_LATENCY_TOLERANCE, ///< 20
    USB_XHCI_TRB_TYPE_CR_GET_PORT_BANDWIDTH,
    USB_XHCI_TRB_TYPE_CR_FORCE_HEADER,
    USB_XHCI_TRB_TYPE_CR_NOOP,
    USB_XHCI_TRB_TYPE_ER_TRANSFER = 32,
    USB_XHCI_TRB_TYPE_ER_COMMAND_COMPLETE,
    USB_XHCI_TRB_TYPE_ER_PORT_STATUS_CHANGE,
    USB_XHCI_TRB_TYPE_ER_BANDWIDTH_REQUEST,
    USB_XHCI_TRB_TYPE_ER_DOORBELL,
    USB_XHCI_TRB_TYPE_ER_HOST_CONTROLLER, ///< 37
    USB_XHCI_TRB_TYPE_ER_DEVICE_NOTIFICATION, ///< 38
    USB_XHCI_TRB_TYPE_ER_MFINDEX_WRAP,
    /* vendor specific bits */
    USB_XHCI_TRB_TYPE_CR_VENDOR_NEC_FIRMWARE_REVISION  = 49,
    USB_XHCI_TRB_TYPE_CR_VENDOR_NEC_CHALLENGE_RESPONSE = 50,
} usb_xhci_trb_type_t;

typedef enum usb_xhci_trb_ccode_t {
    USB_XHCI_TRB_CCODE_CC_INVALID = 0,
    USB_XHCI_TRB_CCODE_CC_SUCCESS,
    USB_XHCI_TRB_CCODE_CC_DATA_BUFFER_ERROR,
    USB_XHCI_TRB_CCODE_CC_BABBLE_DETECTED,
    USB_XHCI_TRB_CCODE_CC_USB_TRANSACTION_ERROR,
    USB_XHCI_TRB_CCODE_CC_TRB_ERROR, ///< 5
    USB_XHCI_TRB_CCODE_CC_STALL_ERROR,
    USB_XHCI_TRB_CCODE_CC_RESOURCE_ERROR,
    USB_XHCI_TRB_CCODE_CC_BANDWIDTH_ERROR,
    USB_XHCI_TRB_CCODE_CC_NO_SLOTS_ERROR,
    USB_XHCI_TRB_CCODE_CC_INVALID_STREAM_TYPE_ERROR, ///< 10
    USB_XHCI_TRB_CCODE_CC_SLOT_NOT_ENABLED_ERROR,
    USB_XHCI_TRB_CCODE_CC_EP_NOT_ENABLED_ERROR,
    USB_XHCI_TRB_CCODE_CC_SHORT_PACKET,
    USB_XHCI_TRB_CCODE_CC_RING_UNDERRUN,
    USB_XHCI_TRB_CCODE_CC_RING_OVERRUN, ///< 15
    USB_XHCI_TRB_CCODE_CC_VF_ER_FULL,
    USB_XHCI_TRB_CCODE_CC_PARAMETER_ERROR,
    USB_XHCI_TRB_CCODE_CC_BANDWIDTH_OVERRUN,
    USB_XHCI_TRB_CCODE_CC_CONTEXT_STATE_ERROR,
    USB_XHCI_TRB_CCODE_CC_NO_PING_RESPONSE_ERROR, ///< 20
    USB_XHCI_TRB_CCODE_CC_EVENT_RING_FULL_ERROR,
    USB_XHCI_TRB_CCODE_CC_INCOMPATIBLE_DEVICE_ERROR,
    USB_XHCI_TRB_CCODE_CC_MISSED_SERVICE_ERROR,
    USB_XHCI_TRB_CCODE_CC_COMMAND_RING_STOPPED,
    USB_XHCI_TRB_CCODE_CC_COMMAND_ABORTED, ///< 25
    USB_XHCI_TRB_CCODE_CC_STOPPED,
    USB_XHCI_TRB_CCODE_CC_STOPPED_LENGTH_INVALID,
    USB_XHCI_TRB_CCODE_CC_MAX_EXIT_LATENCY_TOO_LARGE_ERROR = 29,
    USB_XHCI_TRB_CCODE_CC_ISOCH_BUFFER_OVERRUN = 31,
    USB_XHCI_TRB_CCODE_CC_EVENT_LOST_ERROR,
    USB_XHCI_TRB_CCODE_CC_UNDEFINED_ERROR,
    USB_XHCI_TRB_CCODE_CC_INVALID_STREAM_ID_ERROR,
    USB_XHCI_TRB_CCODE_CC_SECONDARY_BANDWIDTH_ERROR, ///< 35
    USB_XHCI_TRB_CCODE_CC_SPLIT_TRANSACTION_ERROR
} usb_xhci_trb_ccode_t;

#define USB_XHCI_MAX_ENDPOINTS 31

int8_t usb_xhci_init(usb_controller_t* usb_controller);

uint32_t usb_xhci_get_max_psa_size(usb_controller_t* usb_controller);

#ifdef __cplusplus
}
#endif

#endif
