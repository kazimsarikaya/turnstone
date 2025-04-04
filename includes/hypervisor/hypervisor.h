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

#ifdef __cplusplus
extern "C" {
#endif

int8_t hypervisor_init(void);
int8_t hypervisor_stop(void);

int8_t hypervisor_vm_create(const char_t* entry_point_name,
                            uint64_t      heap_size,
                            uint64_t      stack_size);

#ifdef __cplusplus
}
#endif

#endif
