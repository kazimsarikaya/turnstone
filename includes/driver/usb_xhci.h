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
    volatile uint8_t  capability_length;
    volatile uint8_t  reserved;
    volatile uint16_t usb_revision;
    volatile uint32_t hcs_params_1;
    volatile uint32_t hcs_params_2;
    volatile uint32_t hcs_params_3;
    volatile uint32_t hcc_params_1;
    volatile uint32_t dboff;
    volatile uint32_t rtsoff;
    volatile uint32_t hcc_params_2;
} __attribute__((packed)) usb_xhci_capabilities_t;

typedef union usb_xhci_hcs_params_1_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t max_slots_en : 8;
        volatile uint32_t max_intrs    : 11;
        volatile uint32_t reserved     : 5;
        volatile uint32_t max_ports    : 8;
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

int8_t usb_xhci_init(usb_controller_t* usb_controller);

#ifdef __cplusplus
}
#endif

#endif
