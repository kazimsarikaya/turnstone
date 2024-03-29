/**
 * @file hypervisor_utils.64.c
 * @brief Hypervisor Utilities
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_macros.h>
#include <hypervisor/hypervisor_vmxops.h>
#include <hypervisor/hypervisor_ept.h>
#include <hypervisor/hypervisor_guestlib.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>
#include <cpu.h>
#include <cpu/task.h>
#include <tosdb/tosdb_manager.h>
#include <linker.h>
#include <pci.h>
#include <apic.h>

MODULE("turnstone.hypervisor");



uint64_t hypervisor_allocate_region(frame_t** frame, uint64_t size) {
    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(),
                                                      size / FRAME_SIZE,
                                                      FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK,
                                                      frame, NULL) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate region frame");
        return 0;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "allocated 0x%llx 0x%llx", (*frame)->frame_address, (*frame)->frame_count);

    uint64_t frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((*frame)->frame_address);

    if(memory_paging_add_va_for_frame(frame_va, *frame, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot map region frame");
        return 0;
    }

    memory_memclean((void*)frame_va, size);

    return frame_va;
}

uint64_t hypervisor_create_stack(hypervisor_vm_t* vm, uint64_t stack_size) {
    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);

        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VMEXIT_STACK] = *stack_frames;

    uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_va, stack_frames->frame_address, stack_frames->frame_count);

        cpu_hlt();
    }

    memory_memclean((void*)stack_va, stack_size);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "stack va 0x%llx[0x%llx]", stack_va, stack_size);

    return stack_va + stack_size - 16;
}

int8_t vmx_validate_capability(uint64_t target, uint32_t allowed0, uint32_t allowed1) {
    int idx = 0;

    for (idx = 0; idx < 32; idx++) {
        uint32_t mask = 1 << idx;
        int target_is_set = !!(target & mask);
        int allowed0_is_set = !!(allowed0 & mask);
        int allowed1_is_set = !!(allowed1 & mask);

        if ((allowed0_is_set && !target_is_set) || (!allowed1_is_set && target_is_set)) {
            return -1;
        }
    }

    return 0;
}

uint32_t vmx_fix_reserved_1_bits(uint32_t target, uint32_t allowed0) {
    int idx = 0;

    for (idx = 0; idx < 32; idx++) {
        uint32_t mask = 1 << idx;
        int target_is_set = !!(target & mask);
        int allowed0_is_set = !!(allowed0 & mask);

        if (allowed0_is_set && !target_is_set) {
            target |= mask;
        }
    }

    return target;
}

uint32_t vmx_fix_reserved_0_bits(uint32_t target, uint32_t allowed1) {
    int idx = 0;

    for (idx = 0; idx < 32; idx++) {
        uint32_t mask = 1 << idx;
        int target_is_set = !!(target & mask);
        int allowed1_is_set = !!(allowed1 & mask);

        if (!allowed1_is_set && target_is_set) {
            target &= ~mask;
        }
    }

    return target;
}

static void hypervisor_cleanup_unused_modules(hypervisor_vm_t * vm, uint64_t got_fa, uint64_t got_size){
    uint64_t got_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(got_fa);
    uint64_t got_entry_count = got_size / sizeof(linker_global_offset_table_entry_t);
    linker_global_offset_table_entry_t* got_entries = (linker_global_offset_table_entry_t*)got_va;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "got 0x%llx 0x%llx", got_fa, got_size);

    for(uint64_t i = 2; i < got_entry_count; i++) {
        linker_global_offset_table_entry_t* got_entry = &got_entries[i];

        if(got_entry->module_id == 0) {
            break;
        }

        PRINTLOG(HYPERVISOR, LOG_TRACE, "got entry 0x%llx 0x%x 0x%x", got_entry->module_id, got_entry->symbol_type, got_entry->resolved);

        if(!got_entry->resolved) {
            PRINTLOG(HYPERVISOR, LOG_TRACE, "unresolved global object 0x%llx 0x%x", got_entry->module_id, got_entry->symbol_type);
        }

        if(hashmap_get(vm->loaded_module_ids, (void*)got_entry->module_id) == NULL) {
            if(got_entry->resolved) {
                PRINTLOG(HYPERVISOR, LOG_TRACE, "cleaning up unused module 0x%llx", got_entry->module_id);
                got_entry->resolved = false;
            }
        }
    }
}

int8_t hypevisor_deploy_program(hypervisor_vm_t* vm, const char_t* entry_point_name) {
    tosdb_manager_ipc_t ipc = {0};

    ipc.type = TOSDB_MANAGER_IPC_TYPE_PROGRAM_LOAD;
    ipc.program_build.entry_point_name = entry_point_name;
    ipc.program_build.for_vm = true;

    if(tosdb_manager_ipc_send_and_wait(&ipc) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot send program build ipc");
        return -1;
    }

    if(!ipc.is_response_done) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response not done");
        return -1;
    }

    if(!ipc.is_response_success) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response failed");
        return -1;
    }

    vm->program_dump_frame_address = ipc.program_build.program_dump_frame_address;
    vm->program_entry_point_virtual_address = ipc.program_build.program_entry_point_virtual_address;
    vm->program_size = ipc.program_build.program_size;
    vm->program_physical_address = ipc.program_build.program_physical_address;
    vm->program_virtual_address = ipc.program_build.program_virtual_address;
    vm->metadata_physical_address = ipc.program_build.metadata_physical_address;
    vm->got_size = ipc.program_build.got_size;
    vm->got_physical_address = ipc.program_build.got_physical_address;

    vm->guest_stack_size = 2ULL << 20; // 2MiB
    vm->guest_heap_size = 16ULL << 20; // 16MiB

    hashmap_put(vm->loaded_module_ids, (void*)ipc.program_build.program_handle, (void*)true);

    hypervisor_cleanup_unused_modules(vm, ipc.program_build.got_physical_address, ipc.program_build.got_size);

    if(hypervisor_vmcs_prepare_ept(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare ept");
        return -1;
    }

    vmx_write(VMX_GUEST_RIP, ipc.program_build.program_entry_point_virtual_address);
    vmx_write(VMX_GUEST_RSP, (VMX_GUEST_STACK_TOP_VALUE) -8); // we subtract 8 because sse needs 16 byte alignment

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "deployed program entry point is at 0x%llx", ipc.program_build.program_entry_point_virtual_address);

    return 0;
}

void hypervisor_vmcs_goto_next_instruction(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t guest_rip = vmx_read(VMX_GUEST_RIP);
    guest_rip += vmexit_info->instruction_length;
    vmx_write(VMX_GUEST_RIP, guest_rip);
}

int8_t   hypervisor_vmcall_load_module(hypervisor_vm_t* vm, vmcs_vmexit_info_t* vmexit_info);
uint64_t hypervisor_vmcall_attach_pci_dev(hypervisor_vm_t* vm, uint32_t pci_address);

uint64_t hypervisor_vmcall_attach_pci_dev(hypervisor_vm_t* vm, uint32_t pci_address) {
    uint8_t group = (pci_address >> 24) & 0xff;
    uint8_t bus = (pci_address >> 16) & 0xff;
    uint8_t device = (pci_address >> 8) & 0xff;
    uint8_t function = pci_address & 0xff;

    const pci_dev_t* pci_dev = pci_find_device_by_address(group, bus, device, function);

    if(pci_dev == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot find pci device 0x%x 0x%x 0x%x 0x%x", group, bus, device, function);
        return -1;
    }

    uint64_t pci_va = hypervisor_ept_map_pci_device(vm, pci_dev);

    if(pci_va != -1ULL){
        list_list_insert(vm->mapped_pci_devices, pci_dev);
    }

    return pci_va;
}

int8_t hypervisor_vmcall_load_module(hypervisor_vm_t* vm, vmcs_vmexit_info_t* vmexit_info) {
    uint64_t got_fa = vm->got_physical_address;
    uint64_t got_size = vm->got_size;
    uint64_t got_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(got_fa);

    if(vmexit_info->registers->r11 > got_size) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "module id 0x%llx is out of got size 0x%llx", vmexit_info->registers->r11, got_size);
        return -1;
    }

    got_va += vmexit_info->registers->r11;

    linker_global_offset_table_entry_t* got_entry = (linker_global_offset_table_entry_t*)got_va;

    uint64_t module_id = got_entry->module_id;

    if(module_id == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "module id 0x%llx is not valid", module_id);
        return -1;
    }

    if(got_entry->resolved) {
        PRINTLOG(HYPERVISOR, LOG_WARNING, "module id 0x%llx is already resolved", module_id);
        return 0;
    }

    tosdb_manager_ipc_t ipc = {0};

    ipc.type = TOSDB_MANAGER_IPC_TYPE_MODULE_LOAD;
    ipc.program_build.program_handle = module_id;
    ipc.program_build.for_vm = true;

    if(tosdb_manager_ipc_send_and_wait(&ipc) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot send program build ipc");
        return -1;
    }

    if(!ipc.is_response_done) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response not done");
        return -1;
    }

    if(!ipc.is_response_success) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response failed");
        return -1;
    }

    hashmap_put(vm->loaded_module_ids, (void*)ipc.program_build.program_handle, (void*)true);

    hypervisor_cleanup_unused_modules(vm, ipc.program_build.got_physical_address, ipc.program_build.got_size);

    hypervisor_vm_module_load_t ml = {0};

    ml.old_got_physical_address = vm->got_physical_address;
    ml.old_got_size = vm->got_size;
    ml.new_got_physical_address = ipc.program_build.got_physical_address;
    ml.new_got_size = ipc.program_build.got_size;
    ml.module_dump_physical_address = ipc.program_build.program_dump_frame_address;
    ml.module_physical_address = ipc.program_build.program_physical_address;
    ml.module_size = ipc.program_build.program_size;
    ml.metadata_physical_address = ipc.program_build.metadata_physical_address;
    ml.metadata_size = ipc.program_build.metadata_size;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "module id 0x%llx loaded", module_id);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "old got 0x%llx 0x%llx", ml.old_got_physical_address, ml.old_got_size);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "new got 0x%llx 0x%llx", ml.new_got_physical_address, ml.new_got_size);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "module 0x%llx 0x%llx", ml.module_physical_address, ml.module_size);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "module dump 0x%llx", ml.module_dump_physical_address);


    if(hypervisor_ept_merge_module(vm, &ml) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot merge module");
        return -1;
    }

    return 0;
}

void video_text_print(const char* str);

list_t* hypervisor_vmcall_interrupt_mapped_vms[256] = {0};

static int8_t hypervisor_vmcall_intterupt_mapped_isr(interrupt_frame_ext_t* frame) {
    uint8_t interrupt_number = frame->interrupt_number;

    list_t* vms = hypervisor_vmcall_interrupt_mapped_vms[interrupt_number];

    if(list_size(vms)) {
        for(size_t i = 0; i < list_size(vms); i++) {
            hypervisor_vm_t* vm = (hypervisor_vm_t*)list_get_data_at_position(vms, i);

            interrupt_frame_ext_t* cloned_frame = memory_malloc_ext(vm->heap, sizeof(interrupt_frame_ext_t), 0);
            memory_memcopy(frame, cloned_frame, sizeof(interrupt_frame_ext_t));

            list_queue_push(vm->interrupt_queue, cloned_frame);
        }

        apic_eoi();

        return 0;
    }

    return -1;
}

static int16_t hypervisor_vmcall_attach_interrupt(hypervisor_vm_t* vm, vmcs_vmexit_info_t* vmexit_info) {
    pci_generic_device_t* pci_dev = (pci_generic_device_t*)vmexit_info->registers->rdi;
    vm_guest_interrupt_type_t interrupt_type = (vm_guest_interrupt_type_t)vmexit_info->registers->rsi;
    uint8_t interrupt_number = (uint8_t)vmexit_info->registers->rdx;



    pci_capability_msi_t* msi_cap = NULL;
    pci_capability_msix_t* msix_cap = NULL;

    if(pci_dev->common_header.status.capabilities_list) {
        pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_dev->capabilities_pointer);


        while(pci_cap->capability_id != 0xFF) {
            if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSI) {
                msi_cap = (pci_capability_msi_t*)pci_cap;
            } else if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSIX) {
                msix_cap = (pci_capability_msix_t*)pci_cap;
            }

            if(pci_cap->next_pointer == NULL) {
                break;
            }

            pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_cap->next_pointer);
        }
    }

    if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSI && !msi_cap) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "pci device does not support msi");
        return -1;
    } else if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSIX && !msix_cap) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "pci device does not support msix");
        return -1;
    }

    uint8_t intnum = 0;

    if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSI) {
        intnum = interrupt_get_next_empty_interrupt();

        uint32_t msg_addr = 0xFEE00000;
        uint32_t apic_id = apic_get_local_apic_id();
        apic_id <<= 12;
        msg_addr |= apic_id;


        if(msi_cap->ma64_support) {
            msi_cap->ma64.message_address = msg_addr; // | (1 << 3) | (0 << 2);
            msi_cap->ma64.message_data = intnum;
        } else {
            msi_cap->ma32.message_address = msg_addr; // | (1 << 3) | (0 << 2);
            msi_cap->ma32.message_data = intnum;
        }

        uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
        interrupt_irq_set_handler(isrnum, &hypervisor_vmcall_intterupt_mapped_isr);

        msi_cap->enable = 1;
    } else if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSIX) {
        intnum = pci_msix_set_isr(pci_dev, msix_cap, interrupt_number, &hypervisor_vmcall_intterupt_mapped_isr);
        intnum += INTERRUPT_IRQ_BASE;
    } else {
        intnum = interrupt_number;
        uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
        interrupt_irq_set_handler(isrnum, &hypervisor_vmcall_intterupt_mapped_isr);
    }

    interrupt_number = intnum;

    if(!hypervisor_vmcall_interrupt_mapped_vms[interrupt_number]) {
        hypervisor_vmcall_interrupt_mapped_vms[interrupt_number] = list_create_list();
    }

    list_list_insert(hypervisor_vmcall_interrupt_mapped_vms[interrupt_number], vm);

    list_list_insert(vm->mapped_interrupts, (void*)(uint64_t)(interrupt_number));

    return interrupt_number;
}

void hypervisor_vmcall_cleanup_mapped_interrupts(hypervisor_vm_t* vm) {
    while(list_size(vm->mapped_interrupts)) {
        uint64_t interrupt_number = (uint64_t)list_queue_pop(vm->mapped_interrupts);
        list_list_delete(hypervisor_vmcall_interrupt_mapped_vms[interrupt_number], vm);

        if(!list_size(hypervisor_vmcall_interrupt_mapped_vms[interrupt_number])) {
            interrupt_irq_remove_handler(interrupt_number - INTERRUPT_IRQ_BASE, &hypervisor_vmcall_intterupt_mapped_isr);
        }
    }

    while(list_size(vm->interrupt_queue)) {
        interrupt_frame_ext_t* frame = (interrupt_frame_ext_t*)list_queue_pop(vm->interrupt_queue);
        memory_free_ext(vm->heap, frame);
    }
}

uint64_t hypervisor_vmcs_vmcalls_handler(vmcs_vmexit_info_t* vmexit_info) {
    hypervisor_vm_t* vm = task_get_vm();
    uint64_t rax = vmexit_info->registers->rax;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcall rax 0x%llx", rax);

    uint64_t ret = -1;

    switch(rax) {
    case HYPERVISOR_VMCALL_NUMBER_EXIT:
        hypervisor_vmcall_cleanup_mapped_interrupts(vm);
        return 0;
        break;
    case HYPERVISOR_VMCALL_NUMBER_GET_HOST_PHYSICAL_ADDRESS:
        ret = hypervisor_ept_guest_virtual_to_host_physical(vm, vmexit_info->registers->rdi);
        break;
    case HYPERVISOR_VMCALL_NUMBER_ATTACH_PCI_DEV:
        ret = hypervisor_vmcall_attach_pci_dev(vm, vmexit_info->registers->rdi);
        break;
    case HYPERVISOR_VMCALL_NUMBER_ATTACH_INTERRUPT:
        ret = hypervisor_vmcall_attach_interrupt(vm, vmexit_info);
        break;
    case HYPERVISOR_VMCALL_NUMBER_LOAD_MODULE:
        ret = hypervisor_vmcall_load_module(vm, vmexit_info);
        break;
    default:
        PRINTLOG(HYPERVISOR, LOG_ERROR, "unknown vmcall rax 0x%llx", rax);
        break;
    }

    vmexit_info->registers->rax = ret;

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}
