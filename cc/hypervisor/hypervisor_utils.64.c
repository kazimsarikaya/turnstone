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
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>
#include <cpu.h>
#include <tosdb/tosdb_manager.h>

MODULE("turnstone.hypervisor");



uint64_t hypervisor_allocate_region(frame_t** frame, uint64_t size) {
    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
                                                       size / FRAME_SIZE,
                                                       FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK,
                                                       frame, NULL) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate region frame");
        return 0;
    }

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

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
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

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "stack va 0x%llx[0x%llx]", stack_va, stack_size);

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

int8_t hypevisor_deploy_program(hypervisor_vm_t* vm, const char_t* entry_point_name) {
    tosdb_manager_ipc_t ipc = {0};

    ipc.type = TOSDB_MANAGER_IPC_TYPE_PROGRAM_BUILD;
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

    if(hypervisor_vmcs_prepare_ept(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare ept");
        return -1;
    }

    vmx_write(VMX_GUEST_RIP, ipc.program_build.program_entry_point_virtual_address);
    vmx_write(VMX_GUEST_RSP, (256ULL << 30) - 8); // we subtract 8 because sse needs 16 byte alignment

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "deployed program entry point is at 0x%llx", ipc.program_build.program_entry_point_virtual_address);

    return 0;
}

void hypervisor_vmcs_goto_next_instruction(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t guest_rip = vmx_read(VMX_GUEST_RIP);
    guest_rip += vmexit_info->instruction_length;
    vmx_write(VMX_GUEST_RIP, guest_rip);
}

uint64_t hypervisor_vmcs_vmcalls_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t rax = vmexit_info->registers->rax;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcall rax 0x%llx", rax);

    vmexit_info->registers->rax = -1;

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}
