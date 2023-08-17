/**
 * @file syscall.64.c
 * @brief 64-bit system call support.
 **/

#include <apic.h>
#include <cpu.h>
#include <cpu/syscall.h>
#include <cpu/smp.h>
#include <video.h>
#include <memory.h>

typedef int64_t (*syscall_f)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

uint64_t syscall_handler_create_stack(uint64_t old_stack);
int64_t  syscall_handler_null_handler_error(uint64_t handler_no, uint64_t syscall_handler_offset);

int64_t syscall_function_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

const syscall_f SYSCALL_TABLE[] = {
    syscall_function_hlt,
};

uint64_t syscall_handler_create_stack(uint64_t old_stack) {
    smp_data_t* smp_data = (smp_data_t*)0x9000;
    uint64_t ap_id = apic_get_local_apic_id();

    uint64_t new_stack_begin = smp_data->stack_base + (ap_id - 1) * smp_data->stack_size;
    uint8_t* new_stack = (uint8_t*)new_stack_begin;
    memory_memclean(new_stack, smp_data->stack_size);

    new_stack += smp_data->stack_size - 0x10;

    uint64_t* new_stack64 = (uint64_t*)new_stack;

    new_stack64[0] = old_stack;
    new_stack64--;

    new_stack = (uint8_t*)new_stack64;

    uint64_t old_stack_data_count = 17 * 8;

    new_stack -= old_stack_data_count;

    memory_memcopy((uint8_t*)old_stack, new_stack, old_stack_data_count);

    return (uint64_t)new_stack;
}

int64_t syscall_handler_null_handler_error(uint64_t handler_no, uint64_t syscall_handler_offset) {
    PRINTLOG(KERNEL, LOG_ERROR, "syscall_handler_null_handler_error: handler_no=%llx, syscall_handler_offset=%llx\n", handler_no >> 3, syscall_handler_offset);
    PRINTLOG(KERNEL, LOG_ERROR, "syscall_handler_null_handler_error: original syscall table=%p\n", SYSCALL_TABLE);
    return -1;
}

__attribute__((naked, no_stack_protector)) void syscall_handler(void) {
    asm volatile (
        "push %rcx\n"
        "push %r11\n"
        "push %rbp\n"
        "push %rbx\n"
        "push %r12\n"
        "push %r13\n"
        "push %r14\n"
        "push %r15\n"
        "push %rsp\n"
        "mov %rsp, %rbp\n"
        "push %rax\n"
        "push %rdi\n"
        "push %rsi\n"
        "push %rdx\n"
        "push %r10\n"
        "push %r8\n"
        "push %r9\n"
        "lea 0x0(%rip), %rax\n"
        "movabs $_GLOBAL_OFFSET_TABLE_, %r15\n"
        "add %rax, %r15\n"
        "push %r15\n"
        "mov %rsp, %rdi\n"
        "sub $8, %rsp\n"
        "movabsq $syscall_handler_create_stack@GOT, %rax\n"
        "call *(%r15,%rax,1)\n"
        "cmp $0xFFFFFFFFFFFFFFFF, %rax\n"
        "je syscall_handler_exit\n"
        "mov %rax, %rsp\n"
        "sti\n"
        "pop %r15\n"
        "pop %r9\n"
        "pop %r8\n"
        "pop %r10\n"
        "pop %rdx\n"
        "pop %rsi\n"
        "pop %rdi\n"
        "pop %rax\n"
        "mov %r10, %rcx\n"
        "shl $0x3, %rax\n"
        "movabs $SYSCALL_TABLE@GOT, %rbx\n"
        "add %rax, %rbx\n"
        "mov (%r15,%rbx,1), %rbx\n"
        "cmp $0x0, %rbx\n"
        "je syscall_handler_null_handler_exit\n"
        "call *(%rbx)\n"
        "cli\n"
        "syscall_handler_exit:\n"
        "mov %rbp, %rsp\n"
        "pop %rsp\n"
        "pop %r15\n"
        "pop %r14\n"
        "pop %r13\n"
        "pop %r12\n"
        "pop %rbx\n"
        "pop %rbp\n"
        "pop %r11\n"
        "pop %rcx\n"
        "sysretq\n"
        "syscall_handler_null_handler_exit:\n"
        "cli\n"
        "mov %rax, %rdi\n"
        "mov %rbx, %rsi\n"
        "movabsq $syscall_handler_null_handler_error@GOT, %rax\n"
        "call *(%r15,%rax,1)\n"
        "jmp syscall_handler_exit\n"
        );
}


int64_t syscall_function_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    UNUSED(arg1);
    UNUSED(arg2);
    UNUSED(arg3);
    UNUSED(arg4);
    UNUSED(arg5);
    UNUSED(arg6);

    asm volatile ("hlt");
    return 0;
}
