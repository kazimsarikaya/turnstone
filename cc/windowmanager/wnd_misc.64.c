/**
 * @file wnd_misc.64.c
 * @brief window miscellaneous utilities/operations implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager/wnd_misc.h>
#include <windowmanager.h>
#include <acpi.h>

MODULE("turnstone.windowmanager");

int8_t wndmgr_reboot(void) {
    windowmanager_set_initialized(false);
    acpi_reset();
    return 0;
}

int8_t wndmgr_power_off(void) {
    windowmanager_set_initialized(false);
    acpi_poweroff();
    return 0;
}
