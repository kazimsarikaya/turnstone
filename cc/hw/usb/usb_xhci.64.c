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

MODULE("turnstone.kernel.hw.usb");



int8_t usb_xhci_init(usb_controller_t* usb_controller) {
    const pci_dev_t* pci_dev = usb_controller->pci_dev;

    PRINTLOG(USB, LOG_INFO, "XHCI controller found: %02x:%02x:%02x:%02x",
             pci_dev->group_number, pci_dev->bus_number, pci_dev->device_number, pci_dev->function_number);

    return 0;
}
