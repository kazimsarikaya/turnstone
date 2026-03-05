/**
 * @file syscall.64.c
 * @brief 64-bit system call support.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 **/

#include <apic.h>
#include <cpu.h>
#include <cpu/syscall.h>
#include <cpu/smp.h>
#include <cpu/task.h>
#include <logging.h>
#include <memory.h>
#include <utils.h>

/*! module name */
MODULE("turnstone.kernel.cpu.syscall");

/**
 * @typedef syscall_f
 * @brief System call function type.
 * @param[in] arg1 Argument 1.
 * @param[in] arg2 Argument 2.
 * @param[in] arg3 Argument 3.
 * @param[in] arg4 Argument 4.
 * @param[in] arg5 Argument 5.
 * @param[in] arg6 Argument 6.
 * @return Return value.
 */
typedef int64_t (*syscall_f)(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

/**
 * @brief cpu halt system call handler.
 * @param[in] arg1 Argument 1. (not used)
 * @param[in] arg2 Argument 2. (not used)
 * @param[in] arg3 Argument 3. (not used)
 * @param[in] arg4 Argument 4. (not used)
 * @param[in] arg5 Argument 5. (not used)
 * @param[in] arg6 Argument 6. (not used)
 * @return always 0.
 */
static int64_t syscall_function_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    UNUSED(arg1);
    UNUSED(arg2);
    UNUSED(arg3);
    UNUSED(arg4);
    UNUSED(arg5);
    UNUSED(arg6);

    asm volatile ("hlt\n");
    return 0;
}

/**
 * @brief cpu cli and hlt system call handler. after this system call, cpu is halted forever.
 * @param[in] arg1 Argument 1. (not used)
 * @param[in] arg2 Argument 2. (not used)
 * @param[in] arg3 Argument 3. (not used)
 * @param[in] arg4 Argument 4. (not used)
 * @param[in] arg5 Argument 5. (not used)
 * @param[in] arg6 Argument 6. (not used)
 * @return always 0.
 */
static int64_t syscall_function_cli_and_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    UNUSED(arg1);
    UNUSED(arg2);
    UNUSED(arg3);
    UNUSED(arg4);
    UNUSED(arg5);
    UNUSED(arg6);

    asm volatile ("cli\n"
                  "hlt\n");
    return 0;
}

/**
 * @brief task exit system call handler. this system call will exit current task with given exit code.
 * @param[in] arg1 Argument 1. exit code.
 * @param[in] arg2 Argument 2. (not used)
 * @param[in] arg3 Argument 3. (not used)
 * @param[in] arg4 Argument 4. (not used)
 * @param[in] arg5 Argument 5. (not used)
 * @param[in] arg6 Argument 6. (not used)
 * @return always 0.
 */
static int64_t syscall_function_task_exit(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    UNUSED(arg2);
    UNUSED(arg3);
    UNUSED(arg4);
    UNUSED(arg5);
    UNUSED(arg6);

    int32_t exit_code = (int32_t)arg1;

    task_exit(exit_code);

    return 0;
}

/**
 * @brief task yield system call handler. this system call will yield current task for waiting other tasks.
 * @param[in] arg1 Argument 1. (not used)
 * @param[in] arg2 Argument 2. (not used)
 * @param[in] arg3 Argument 3. (not used)
 * @param[in] arg4 Argument 4. (not used)
 * @param[in] arg5 Argument 5. (not used)
 * @param[in] arg6 Argument 6. (not used)
 * @return always 0.
 */
static int64_t syscall_function_task_yield(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    UNUSED(arg1);
    UNUSED(arg2);
    UNUSED(arg3);
    UNUSED(arg4);
    UNUSED(arg5);
    UNUSED(arg6);

    task_yield();

    return 0;
}

/**
 * @brief system call table.
 */
static const syscall_f SYSCALL_TABLE[] = {
    syscall_function_hlt,
    syscall_function_cli_and_hlt,
    syscall_function_task_exit,
    syscall_function_task_yield,
};

static boolean_t syscall_xsave_mask_memorized = false;
static uint64_t syscall_xsave_mask_lo         = 0;
static uint64_t syscall_xsave_mask_hi         = 0;

static void syscall_save_restore_avx512f(boolean_t save, syscall_frame_t* frame) {
    if(!syscall_xsave_mask_memorized) {
        cpu_cpuid_regs_t query = {0};
        cpu_cpuid_regs_t result;

        query.eax = 0xd;

        cpu_cpuid(query, &result);

        syscall_xsave_mask_lo        = result.eax;
        syscall_xsave_mask_hi        = result.edx;
        syscall_xsave_mask_memorized = true;
    }

    uint64_t frame_base     = (uint64_t)frame;
    uint64_t avx512f_offset = frame_base + offsetof_field(syscall_frame_t, avx512f);
    // align to 0x40
    avx512f_offset = (avx512f_offset + 0x3F) & ~0x3F;

    if(save) {
        memory_memclean((void*)avx512f_offset, 0x2000);

        asm volatile (
            "mov %[avx512f_offset], %%rbx\n"
            "xsave (%%rbx)\n"
            :
            :
            [avx512f_offset] "r" (avx512f_offset),
            "rax" (syscall_xsave_mask_lo),
            "rdx" (syscall_xsave_mask_hi)
            : "rbx"
            );
    } else {
        asm volatile (
            "mov %[avx512f_offset], %%rbx\n"
            "xrstor (%%rbx)\n"
            :
            :
            [avx512f_offset] "r" (avx512f_offset),
            "rax" (syscall_xsave_mask_lo),
            "rdx" (syscall_xsave_mask_hi)
            : "rbx"
            );
    }
}

static int64_t syscall_generic_handler(syscall_frame_t* frame) {
    syscall_save_restore_avx512f(true, frame);

    if(frame->rax >= ARRAY_SIZE(SYSCALL_TABLE)) {
        PRINTLOG(KERNEL, LOG_ERROR, "invalid syscall number %llu", frame->rax);
        syscall_save_restore_avx512f(false, frame);
        return -1;
    }

    syscall_f function = SYSCALL_TABLE[frame->rax];

    if(!function) {
        PRINTLOG(KERNEL, LOG_ERROR, "syscall number %llu not implemented", frame->rax);
        syscall_save_restore_avx512f(false, frame);
        return -1;
    }

    PRINTLOG(KERNEL, LOG_TRACE, "stack is at 0x%llx", frame->rsp);
    PRINTLOG(KERNEL, LOG_TRACE, "syscall (0x%p) number %llu called with args: 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx",
             function,
             frame->rax, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);

    // TODO: initilize stack for syscall handler and switch to it before calling syscall function,
    // and switch back to user stack after syscall function returns. this is required for
    // proper handling of nested syscalls and interrupts during syscall execution.

    int64_t res = function(frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);

    frame->rax = res;

    syscall_save_restore_avx512f(false, frame);

    return res;
}

void syscall_init_generic_handler(void) {
    syscall_handlers_set_generic_handler(syscall_generic_handler);
}

void syscall_init(void) {
    uint64_t msr_efer = cpu_read_msr(CPU_MSR_EFER);
    msr_efer |= 1;
    cpu_write_msr(CPU_MSR_EFER, msr_efer);

    // upper 16 bits (48:63) for sysretq and lower 16 bits (32:47) for syscallq,
    // when syscall is called, cs = syscall_cs, ss = syscall_cs + 0x08.
    // when sysretq is called, cs = sysretq_cs + 0x10, ss = sysretq_cs + 0x08.
    // hence in gdt, user code segment should be after user data segment,
    // kernel code segment should be before kernel data segment.
    // despite documentation says lower 2 bytes of syscall_cs and sysretq_cs are ignored,
    // in reality they are not ignored and sysretq_cs should have 0x3
    // otherwise sysretq will cause general protection fault when returning to user space
    // after int recevied at userspace.
    uint64_t msr_star = 0x00230008ULL << 32;
    cpu_write_msr(CPU_MSR_STAR, msr_star);

    // this handler is defined inside interrupt handlers module, which is coded
    // for shareable between userspace application's page table and kernel's page table.
    // that module has no dependency to other modules.
    uint64_t msr_lstar = (uint64_t)syscall_handler;
    cpu_write_msr(CPU_MSR_LSTAR, msr_lstar);

    // when syscall is called, rflags will be masked with fmask,
    // in first stages of syscall handler code we modify stack,
    // hence there should not be any interrupt during that time,
    // so we mask interrupt flag in rflags.
    uint64_t msr_fmask = 0x200;
    cpu_write_msr(CPU_MSR_FMASK, msr_fmask);
}
