/**
 * @file local_apic_id.64.c
 * @brief Local APIC ID
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <types.h>

MODULE("turnstone.kernel.cpu.apic");

boolean_t local_apic_id_is_valid = false;
uint32_t __seg_gs * local_apic_id = 0;
