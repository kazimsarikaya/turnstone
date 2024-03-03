/**
 * @file hypervisor.h
 * @brief defines hypervisor related structures and functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_H
#define ___HYPERVISOR_H 0

#include <types.h>

uint32_t hypervisor_vmcs_revision_id(void);

int8_t hypervisor_init(void);
int8_t hypervisor_stop(void);


#endif
