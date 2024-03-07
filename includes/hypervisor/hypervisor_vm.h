/**
 * @file hypervisor_vm.h
 * @brief defines hypervisor related Virtual Machine structures.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___HYPERVISOR_VM_H
#define ___HYPERVISOR_VM_H 0

#include <types.h>
#include <list.h>
#include <memory.h>
#include <buffer.h>
#include <map.h>

typedef struct hypervisor_vm_t {
    memory_heap_t* heap;
    uint64_t       task_id;
    list_t*        ipc_queue;
    uint64_t       vmcs_frame_fa;
    buffer_t*      output_buffer;
    struct {
        uint64_t  timer_initial_value;
        uint64_t  timer_current_value;
        uint64_t  timer_divider;
        uint64_t  timer_divider_realvalue;
        uint8_t   timer_vector;
        boolean_t timer_periodic;
        boolean_t timer_masked;
        uint8_t   in_service_vector;
        uint8_t   in_request_vectors[32];
        boolean_t apic_eoi_pending;
    }         lapic;
    uint64_t  last_tsc;
    boolean_t lapic_timer_pending;
    boolean_t lapic_timer_enabled;
    boolean_t need_to_notify;
    boolean_t is_halted;
    boolean_t is_halt_need_next_instruction;
    map_t*    msr_map;
} hypervisor_vm_t;


int8_t hypervisor_vm_init(void);

int8_t hypervisor_vm_create_and_attach_to_task(uint64_t vmcs_frame_fa);
void   hypervisor_vm_destroy(hypervisor_vm_t* vm);
void   hypervisor_vm_notify_timers(void);

#endif
