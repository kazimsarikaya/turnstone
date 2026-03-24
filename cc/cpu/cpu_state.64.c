/**
 * @file cpu_state.64.c
 * @brief Local APIC ID
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cpu/cpu_state.h>
#include <types.h>

MODULE("turnstone.kernel.cpu.state");

volatile cpu_state_t __seg_gs * cpu_state = NULL;
