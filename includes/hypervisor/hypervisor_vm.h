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
#include <memory/frame.h>
#include <buffer.h>
#include <map.h>
#include <hashmap.h>

typedef enum hypervisor_vm_frame_type_t {
    HYPERVISOR_VM_FRAME_TYPE_SELF,
    HYPERVISOR_VM_FRAME_TYPE_VMCS,
    HYPERVISOR_VM_FRAME_TYPE_VMEXIT_STACK,
    HYPERVISOR_VM_FRAME_TYPE_VAPIC,
    HYPERVISOR_VM_FRAME_TYPE_MSR_BITMAP,
    HYPERVISOR_VM_FRAME_TYPE_IO_BITMAP,
    HYPERVISOR_VM_FRAME_TYPE_VM_EXIT_LOAD_MSR,
    HYPERVISOR_VM_FRAME_TYPE_VM_EXIT_STORE_MSR,
    HYPERVISOR_VM_FRAME_TYPE_NR,
} hypervisor_vm_frame_type_t;

typedef struct hypervisor_vm_t {
    memory_heap_t* heap;
    const char_t*  entry_point_name;
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
        uint64_t  in_request_vectors[4];
        boolean_t apic_eoi_pending;
    }          lapic;
    uint64_t   last_tsc;
    boolean_t  lapic_timer_pending;
    boolean_t  lapic_timer_enabled;
    boolean_t  need_to_notify;
    boolean_t  is_halted;
    boolean_t  is_halt_need_next_instruction;
    map_t*     msr_map;
    frame_t    owned_frames[HYPERVISOR_VM_FRAME_TYPE_NR];
    list_t*    ept_frames;
    hashmap_t* loaded_module_ids;
    list_t*    read_only_frames;
    list_t*    mapped_pci_devices;
    list_t*    mapped_interrupts;
    list_t*    interrupt_queue;
    list_t*    mapped_io_ports;
    uint64_t   program_dump_frame_address;
    uint64_t   program_physical_address;
    uint64_t   program_virtual_address;
    uint64_t   program_size;
    uint64_t   program_entry_point_virtual_address;
    uint64_t   got_physical_address;
    uint64_t   got_size;
    uint64_t   metadata_physical_address;
    uint8_t    metadata_size;
    uint64_t   guest_stack_physical_base;
    uint64_t   guest_stack_size;
    uint64_t   guest_heap_physical_base;
    uint64_t   guest_heap_size;
    uint64_t   ept_pml4_base;
    uint64_t   next_page_address;
} hypervisor_vm_t;


int8_t hypervisor_vm_init(void);

int8_t hypervisor_vm_create_and_attach_to_task(hypervisor_vm_t* vm);
void   hypervisor_vm_destroy(hypervisor_vm_t* vm);
void   hypervisor_vm_notify_timers(void);

#endif
