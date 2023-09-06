/**
 * @file usb_ehci.h
 * @brief USB EHCI driver header file
 */

#ifndef ___USB_EHCI_H
#define ___USB_EHCI_H



#include <types.h>
#include <driver/usb.h>
#include <pci.h>
#include <utils.h>


typedef struct usb_ehci_controller_capabilties_t {
    volatile uint32_t caplength  : 8;
    volatile uint32_t reserved   : 8;
    volatile uint32_t hciversion : 16;
    volatile uint32_t hcsparams;
    volatile uint32_t hccparams;
    volatile uint32_t hcsp_portroute;
} __attribute__((packed)) usb_ehci_controller_capabilties_t;

typedef union usb_ehci_hcsparams_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t n_ports            : 4;
        volatile uint32_t port_power_control : 1;
        volatile uint32_t reserved1          : 2;
        volatile uint32_t port_routing_rules : 1;
        volatile uint32_t n_per_cc           : 4;
        volatile uint32_t n_cc               : 4;
        volatile uint32_t port_indicators    : 1;
        volatile uint32_t reserved2          : 3;
        volatile uint32_t debug_port         : 4;
        volatile uint32_t reserved3          : 8;
    } __attribute__((packed)) bits;
} usb_ehci_hcsparams_t;

_Static_assert(sizeof(usb_ehci_hcsparams_t) == sizeof(uint32_t), "usb_ehci_hcsparams_t is not 32 bits");

typedef union usb_ehci_hccparams_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t addr64              : 1;
        volatile uint32_t prog_frame_list     : 1;
        volatile uint32_t async_park          : 1;
        volatile uint32_t reserved1           : 1;
        volatile uint32_t iso_sched_threshold : 4;
        volatile uint32_t eecp                : 8;
        volatile uint32_t reserved2           : 16;
    } __attribute__((packed)) bits;
} usb_ehci_hccparams_t;

_Static_assert(sizeof(usb_ehci_hccparams_t) == sizeof(uint32_t), "usb_ehci_hccparams_t is not 32 bits");

typedef struct usb_ehci_op_regs_t {
    volatile uint32_t usb_cmd;
    volatile uint32_t usb_sts;
    volatile uint32_t usb_intr;
    volatile uint32_t frame_index;
    volatile uint32_t ctrl_ds_segment;
    volatile uint32_t periodic_list_base;
    volatile uint32_t async_list_addr;
    volatile uint32_t reserved1[9];
    volatile uint32_t config_flag;
    volatile uint32_t port_status_and_control[];
} __attribute__((packed)) usb_ehci_op_regs_t;

typedef union usb_ehci_cmd_reg_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t run_stop                          : 1;
        volatile uint32_t host_controller_reset             : 1;
        volatile uint32_t frame_list_size                   : 2;
        volatile uint32_t periodic_schedule_enable          : 1;
        volatile uint32_t async_schedule_enable             : 1;
        volatile uint32_t interrupt_on_async_advance_enable : 1;
        volatile uint32_t light_host_controller_reset       : 1;
        volatile uint32_t async_sched_park_mode_count       : 2;
        volatile uint32_t reserved1                         : 1;
        volatile uint32_t async_sched_park_mode_enable      : 1;
        volatile uint32_t reserved2                         : 4;
        volatile uint32_t interrupt_threshold_control       : 8;
        volatile uint32_t reserved3                         : 8;
    } __attribute__((packed)) bits;
} usb_ehci_cmd_reg_t;

_Static_assert(sizeof(usb_ehci_cmd_reg_t) == sizeof(uint32_t), "usb_ehci_cmd_reg_t is not 32 bits");

typedef union usb_ehci_sts_reg_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t usb_interrupt              : 1;
        volatile uint32_t usb_error                  : 1;
        volatile uint32_t port_change                : 1;
        volatile uint32_t frame_list_rollover        : 1;
        volatile uint32_t host_system_error          : 1;
        volatile uint32_t interrupt_on_async_advance : 1;
        volatile uint32_t reserved1                  : 6;
        volatile uint32_t host_cotroller_halted      : 1;
        volatile uint32_t reclaimation               : 1;
        volatile uint32_t periodic_sched_status      : 1;
        volatile uint32_t async_sched_status         : 1;
        volatile uint32_t reserved2                  : 16;
    } __attribute__((packed)) bits;
} usb_ehci_sts_reg_t;

_Static_assert(sizeof(usb_ehci_sts_reg_t) == sizeof(uint32_t), "usb_ehci_sts_reg_t is not 32 bits");

typedef union usb_ehci_int_enable_reg_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t usb_interrupt_enable              : 1;
        volatile uint32_t usb_error_enable                  : 1;
        volatile uint32_t port_change_enable                : 1;
        volatile uint32_t frame_list_rollover_enable        : 1;
        volatile uint32_t host_system_error_enable          : 1;
        volatile uint32_t interrupt_on_async_advance_enable : 1;
        volatile uint32_t reserved1                         : 26;
    } __attribute__((packed)) bits;
} usb_ehci_int_enable_reg_t;

_Static_assert(sizeof(usb_ehci_int_enable_reg_t) == sizeof(uint32_t), "usb_ehci_int_enable_reg_t is not 32 bits");

typedef union usb_ehci_port_sts_and_ctrl_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t current_connect_status      : 1;
        volatile uint32_t connect_status_change       : 1;
        volatile uint32_t port_enabled                : 1;
        volatile uint32_t enable_status_change        : 1;
        volatile uint32_t port_over_current           : 1;
        volatile uint32_t port_over_current_change    : 1;
        volatile uint32_t force_port_resume           : 1;
        volatile uint32_t suspend                     : 1;
        volatile uint32_t reset                       : 1; // bit 8
        volatile uint32_t reserved1                   : 1;
        volatile uint32_t line_status                 : 2;
        volatile uint32_t port_power                  : 1;
        volatile uint32_t port_owner                  : 1;
        volatile uint32_t port_indicator_control      : 2;
        volatile uint32_t port_test_control           : 4;
        volatile uint32_t wake_on_connect_enable      : 1;
        volatile uint32_t wake_on_disconnect_enable   : 1;
        volatile uint32_t wake_on_over_current_enable : 1;
        volatile uint32_t reserved2                   : 9;
    } __attribute__((packed)) bits;
} usb_ehci_port_sts_and_ctrl_t;

_Static_assert(sizeof(usb_ehci_port_sts_and_ctrl_t) == sizeof(uint32_t), "usb_ehci_port_sts_and_ctrl_t is not 32 bits");

typedef enum usb_ehci_pid_t {
    USB_EHCI_PID_OUT = 0,
    USB_EHCI_PID_IN  = 1,
    USB_EHCI_PID_SETUP = 2,
} usb_ehci_pid_t;

typedef enum usb_ehci_qtd_type_t {
    USB_EHCI_QTD_TYPE_ITD = 0,
    USB_EHCI_QTD_TYPE_QH  = 1,
    USB_EHCI_QTD_TYPE_SITD = 2,
    USB_EHCI_QTD_TYPE_FSTN = 3,
} usb_ehci_qtd_type_t;

typedef union usb_ehci_framelist_item_t {
    struct {
        volatile uint32_t terminate_bit                     : 1;
        volatile uint32_t type                              : 2;
        volatile uint32_t reserved1                         : 2;
        volatile uint32_t qh_or_itd_or_sitd_or_fstn_pointer : 27;
    } __attribute__((packed)) bits;
    volatile uint32_t raw;
}__attribute__((packed)) usb_ehci_framelist_item_t;

_Static_assert(sizeof(usb_ehci_framelist_item_t) == sizeof(uint32_t), "usb_ehci_framelist_item_t is not 32 bits");

typedef struct usb_ehci_itd_t {
    union {
        struct {
            volatile uint32_t terminate_bit     : 1;
            volatile uint32_t type              : 2;
            volatile uint32_t reserved1         : 2;
            volatile uint32_t next_link_pointer : 27;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } next_link_pointer;

    union {
        struct {
            volatile uint32_t offset          : 12;
            volatile uint32_t page_select     : 3;
            volatile uint32_t ioc             : 1;
            volatile uint32_t length          : 12;
            volatile uint32_t xact_err        : 1;
            volatile uint32_t babble_err      : 1;
            volatile uint32_t data_buffer_err : 1;
            volatile uint32_t active          : 1;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } transactions[8];

    union {
        struct {
            volatile uint32_t device_address : 7;
            volatile uint32_t reserved1      : 1;
            volatile uint32_t endpoint       : 4;
            volatile uint32_t buffer_address : 20;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } buffer_page_0;

    union {
        struct {
            volatile uint32_t max_packet_length : 11;
            volatile uint32_t direction         : 1;
            volatile uint32_t buffer_address    : 20;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } buffer_page_1;

    union {
        struct {
            volatile uint32_t multi_count    : 2;
            volatile uint32_t reserved1      : 10;
            volatile uint32_t buffer_address : 20;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } buffer_page_2;

    union {
        struct {
            volatile uint32_t reserved1      : 12;
            volatile uint32_t buffer_address : 20;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } buffer_pages_cont[4];

    volatile uint32_t ext_buffer_pages[6];

    uint8_t           padding[4];
    volatile uint32_t this_raw;
} __attribute__((packed)) usb_ehci_itd_t;

_Static_assert((sizeof(usb_ehci_itd_t) % 32) == 0, "usb_ehci_itq_t is not 32 bytes boundary aligned");

typedef struct usb_ehci_sitd_t {
    union {
        struct {
            volatile uint32_t terminate_bit     : 1;
            volatile uint32_t type              : 2;
            volatile uint32_t reserved1         : 2;
            volatile uint32_t next_link_pointer : 27;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } next_link_pointer;

    union {
        struct {
            volatile uint32_t device_address : 7;
            volatile uint32_t reserved1      : 1;
            volatile uint32_t endpoint       : 4;
            volatile uint32_t reserved2      : 4;
            volatile uint32_t hub_address    : 7;
            volatile uint32_t reserved3      : 1;
            volatile uint32_t port_number    : 7;
            volatile uint32_t direction      : 1;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } capabilities;

    union {
        struct {
            volatile uint32_t start_mask    : 8;
            volatile uint32_t complete_mask : 8;
            volatile uint32_t reserved1     : 16;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    }schedule_control;

    union {
        struct {
            volatile uint32_t reserved1       : 1;
            volatile uint32_t split_xstate    : 1;
            volatile uint32_t missed_uframe   : 1;
            volatile uint32_t xact_err        : 1;
            volatile uint32_t babble_err      : 1;
            volatile uint32_t data_buffer_err : 1;
            volatile uint32_t err             : 1;
            volatile uint32_t active          : 1;
            volatile uint32_t c_prog_mask     : 8;
            volatile uint32_t total_bytes     : 10;
            volatile uint32_t reserved2       : 4;
            volatile uint32_t page_select     : 1;
            volatile uint32_t ioc             : 1;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } transfer_state;

    union {
        struct {
            volatile uint32_t offset         : 12;
            volatile uint32_t buffer_address : 20;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } buffer_page_0;

    union {
        struct {
            volatile uint32_t t_count        : 3;
            volatile uint32_t t_position     : 2;
            volatile uint32_t reserved1      : 7;
            volatile uint32_t buffer_address : 20;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } buffer_page_1;

    union {
        struct {
            volatile uint32_t terminate_bit     : 1;
            volatile uint32_t reserved1         : 4;
            volatile uint32_t next_link_pointer : 27;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } back_link_pointer;

    volatile uint32_t ext_buffer_pages[2];

    uint8_t           padding[24];
    volatile uint32_t this_raw;
} __attribute__((packed)) usb_ehci_sitd_t;

_Static_assert((sizeof(usb_ehci_sitd_t) % 32) == 0, "usb_ehci_sitq_t is not 32 bytes boundary aligned");


typedef union usb_ehci_qtd_pointer_t {
    struct {
        volatile uint32_t next_qtd_terminate : 1;
        volatile uint32_t reserved3          : 4;
        volatile uint32_t next_qtd_pointer   : 27;
    } __attribute__((packed)) bits;
    volatile uint32_t raw;
} __attribute__((packed)) usb_ehci_qtd_pointer_t;

_Static_assert(sizeof(usb_ehci_qtd_pointer_t) == sizeof(uint32_t), "usb_ehci_qtd_pointer_t is not 32 bits");

typedef union usb_ehci_qtd_token_t {
    struct {
        volatile uint32_t ping_state              : 1;
        volatile uint32_t split_transaction_state : 1;
        volatile uint32_t missed_microframe       : 1;
        volatile uint32_t xact_err                : 1;
        volatile uint32_t babble_err              : 1;
        volatile uint32_t data_buffer_err         : 1;
        volatile uint32_t halted                  : 1;
        volatile uint32_t active                  : 1;
        volatile uint32_t pid_code                : 2;
        volatile uint32_t error_counter           : 2;
        volatile uint32_t current_page            : 3;
        volatile uint32_t interrupt_on_complete   : 1;
        volatile uint32_t bytes_to_transfer       : 15;
        volatile uint32_t data_toggle             : 1;
    } __attribute__((packed)) bits;
    volatile uint32_t raw;
} __attribute__((packed)) usb_ehci_qtd_token_t;

_Static_assert(sizeof(usb_ehci_qtd_token_t) == sizeof(uint32_t), "usb_ehci_qtd_token_t is not 32 bits");

typedef struct usb_ehci_qtd_t {
    usb_ehci_qtd_pointer_t next_qtd_pointer;
    usb_ehci_qtd_pointer_t alternate_next_qtd_pointer;
    usb_ehci_qtd_token_t   token;
    volatile uint32_t      buffer_page_pointer[5];
    volatile uint32_t      ext_buffer_page_pointer[5];
    uint8_t                padding[3];
    volatile uint32_t      this_raw;
    volatile uint32_t      prev_raw;
    volatile uint8_t       id;
}__attribute__((packed)) usb_ehci_qtd_t;

_Static_assert((sizeof(usb_ehci_qtd_t) % 32) == 0, "usb_ehci_qtd_t is not 32 bit aligned");

typedef struct usb_ehci_qh_t {
    union {
        struct {
            volatile uint32_t terminate               : 1;
            volatile uint32_t qtd_type                : 2;
            volatile uint32_t reserved1               : 2;
            volatile uint32_t horizontal_link_pointer : 27;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    }__attribute__((packed)) horizontal_link_pointer;

    union {
        struct {
            volatile uint32_t device_address                : 7;
            volatile uint32_t inactive_on_next_tr           : 1;
            volatile uint32_t endpoint_number               : 4;
            volatile uint32_t endpoint_speed                : 2;
            volatile uint32_t data_toggle_control           : 1;
            volatile uint32_t head_of_reclamation_list_flag : 1;
            volatile uint32_t max_packet_length             : 11;
            volatile uint32_t control_endpoint_flag         : 1;
            volatile uint32_t nak_count_reload              : 4;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } __attribute__((packed)) endpoint_characteristics;

    union {
        struct {
            volatile uint32_t interrupt_schedule_mask : 8;
            volatile uint32_t split_completion_mask   : 8;
            volatile uint32_t hub_address             : 7;
            volatile uint32_t port_number             : 7;
            volatile uint32_t mult                    : 2;
        } __attribute__((packed)) bits;
        volatile uint32_t raw;
    } __attribute__((packed)) endpoint_capabilities;

    usb_ehci_qtd_pointer_t current_qtd_pointer;
    usb_ehci_qtd_pointer_t next_qtd_pointer;
    usb_ehci_qtd_pointer_t alternate_qtd_pointer;

    usb_ehci_qtd_token_t token;

    volatile uint32_t buffer_page_pointer[5];
    volatile uint32_t ext_buffer_page_pointer[5];

    uint8_t           padding[3];
    volatile uint32_t this_raw;
    volatile uint32_t prev_raw;
    volatile uint8_t  id;
    usb_ehci_qtd_t*   qtd_head;
    usb_ehci_qtd_t*   qtd_tail;
} __attribute__((packed)) usb_ehci_qh_t;

_Static_assert((sizeof(usb_ehci_qh_t) % 32) == 0, "usb_ehci_qh_t is not 32 bytes aligned");

int8_t usb_ehci_init(usb_controller_t * usb_controller);

#endif
