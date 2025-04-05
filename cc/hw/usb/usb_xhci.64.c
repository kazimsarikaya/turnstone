/**
 * @file usb_xhci.64.c
 * @brief USB XHCI driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <driver/usb_xhci.h>
#include <pci.h>
#include <logging.h>
#include <memory/paging.h>

MODULE("turnstone.kernel.hw.usb");



int8_t usb_xhci_init(usb_controller_t* usb_controller) {
    const pci_dev_t* pci_dev = usb_controller->pci_dev;

    PRINTLOG(USB, LOG_INFO, "XHCI controller found: %02x:%02x:%02x:%02x",
             pci_dev->group_number, pci_dev->bus_number, pci_dev->device_number, pci_dev->function_number);

    uint64_t bar_fa = pci_get_bar_address((pci_generic_device_t*)pci_dev->pci_header, 0);
    PRINTLOG(USB, LOG_INFO, "XHCI BAR address: 0x%016llx", bar_fa);

    uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);
    PRINTLOG(USB, LOG_INFO, "XHCI BAR virtual address: 0x%016llx", bar_va);

    usb_xhci_capabilities_t* xhci_cap = (usb_xhci_capabilities_t*)bar_va;

    usb_xhci_caplen_rev_t caplen_rev  = (usb_xhci_caplen_rev_t)xhci_cap->caplength_and_revision;

    PRINTLOG(USB, LOG_INFO, "XHCI capability length: %d", caplen_rev.bits.capability_length);
    PRINTLOG(USB, LOG_INFO, "XHCI revision: 0x%04x", caplen_rev.bits.usb_revision);

    usb_xhci_hcs_params_1_t hcs_params1 = (usb_xhci_hcs_params_1_t)xhci_cap->hcs_params_1;

    PRINTLOG(USB, LOG_INFO, "XHCI number of device slots: %d", hcs_params1.bits.max_slots);
    PRINTLOG(USB, LOG_INFO, "XHCI number of interrupter: %d", hcs_params1.bits.max_interrupts);
    PRINTLOG(USB, LOG_INFO, "XHCI number of ports: %d", hcs_params1.bits.max_ports);

    usb_xhci_hcs_params_2_t hcs_params2 = (usb_xhci_hcs_params_2_t)xhci_cap->hcs_params_2;

    PRINTLOG(USB, LOG_INFO, "XHCI max erst size: %d", hcs_params2.bits.erst_max);

    usb_xhci_hcc_params_1_t hcc_params1 = (usb_xhci_hcc_params_1_t)xhci_cap->hcc_params_1;

    PRINTLOG(USB, LOG_INFO, "XHCI ac64: %d", hcc_params1.bits.ac64);
    PRINTLOG(USB, LOG_INFO, "XHCI context size: %d", hcc_params1.bits.csz);
    uint64_t extended_caps_offset = hcc_params1.bits.xecp << 2;
    PRINTLOG(USB, LOG_INFO, "XHCI extended capabilities pointer: 0x%016llx", extended_caps_offset);

    PRINTLOG(USB, LOG_INFO, "XHCI doorbell offset: 0x%08x", xhci_cap->dboff);
    PRINTLOG(USB, LOG_INFO, "XHCI runtime register space offset: 0x%08x", xhci_cap->rtsoff);

    uint64_t opregs_va = bar_va + caplen_rev.bits.capability_length;

    usb_xhci_operational_registers_t* opregs = (usb_xhci_operational_registers_t*)opregs_va;

    usb_xhci_usbsts_t usbsts = (usb_xhci_usbsts_t)opregs->usbsts;

    PRINTLOG(USB, LOG_INFO, "XHCI usb status is halted: %d", usbsts.bits.hch);

    return 0;
}
