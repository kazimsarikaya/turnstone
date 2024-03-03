/**
 * @file hypervisor_utils.64.c
 * @brief Hypervisor Utilities
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor_utils.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>
#include <cpu.h>

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

uint64_t hypervisor_create_stack(uint64_t stack_size) {
    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);

        return -1;
    }

    uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_va, stack_frames->frame_address, stack_frames->frame_count);

        cpu_hlt();
    }

    memory_memclean((void*)stack_va, stack_size);

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
