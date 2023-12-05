/**
 * @file systeminfo.64.c
 * @brief System information.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <systeminfo.h>

/*! module name */
MODULE("turnstone.lib");

/*! system information global variable. */
system_info_t* SYSTEM_INFO = NULL;
