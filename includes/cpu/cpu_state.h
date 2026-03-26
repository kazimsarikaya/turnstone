/**
 * @file cpu_state.h
 * @brief cpu state stored at each cpu at gs segment
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CPU_STATE_H
/*! prevent duplicate header error macro */
#define ___CPU_STATE_H 0

#include <cpu/task.h>
#include <list.h>
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cpu_state_t {
    uint64_t  local_apic_id; ///< local apic id
    task_t*   current_task; ///< current task
    task_t*   idle_task; ///< idle task
    task_t*   cleaner_task; ///< cleaner task
    boolean_t tasking_enabled; ///< tasking enabled
    boolean_t task_switch_paramters_need_eoi; ///< task switch parameters need eoi
    list_t*   task_queue; ///< task list
    list_t*   task_sleep_queue; ///< task sleep list
    list_t*   task_wait_queue; ///< task wait list
    list_t*   task_cleanup_queue; ///< task cleanup list
    uint64_t  tick_count; ///< tick count
    size_t    gdt_va;
    size_t    gdt_size;
    size_t    tss_va;
    size_t    tss_size;
    uint16_t  tss_selector;
    uint64_t  hypervisor_helper_fa; // intel/amd uses 1 frame for hypervisor init.
    boolean_t parked; // if cpu is parked, it should not be scheduled for tasks and should not be woken up.
    boolean_t in_parked_state; // scheduler now parked.
} cpu_state_t;

extern volatile cpu_state_t __seg_gs * cpu_state;

#ifdef __cplusplus
}
#endif

#endif
