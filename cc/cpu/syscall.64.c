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
#include <logging.h>
#include <memory.h>

/*! module name */
MODULE("turnstone.kernel.cpu");

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
 * @brief creates stack for system call handler.
 * @param[in] old_stack Old stack pointer.
 * @return New stack pointer.
 */
uint64_t syscall_handler_create_stack(uint64_t old_stack);

/**
 * @brief null handler error. when system call handler is null, this function is called. prints error message about null handler and returns -1.
 * @param[in] handler_no Handler number.
 * @param[in] syscall_handler_offset System call handler offset.
 * @return always -1.
 */
int64_t syscall_handler_null_handler_error(uint64_t handler_no, uint64_t syscall_handler_offset);

/**
 * @brief not found error. when system call handler is not found, this function is called. prints error message about not found handler and returns -1.
 * @param[in] syscall_no System call number.
 * @return always -1.
 */
int64_t syscall_handler_notfound_error(uint64_t syscall_no);

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
int64_t syscall_function_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

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
int64_t syscall_function_cli_and_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

/**
 * @brief system call table.
 */
const syscall_f SYSCALL_TABLE[] = {
    syscall_function_hlt,
    syscall_function_cli_and_hlt,
};

/*! system call table size */
#define SYSCALL_TABLE_SIZE (sizeof(SYSCALL_TABLE) / sizeof(SYSCALL_TABLE[0]))

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
    PRINTLOG(KERNEL, LOG_ERROR, "syscall_handler_null_handler_error: handler_no=%llx, syscall_handler_offset=%llx", handler_no >> 3, syscall_handler_offset);
    PRINTLOG(KERNEL, LOG_ERROR, "syscall_handler_null_handler_error: original syscall table=%p", SYSCALL_TABLE);
    return -1;
}

int64_t  syscall_handler_notfound_error(uint64_t syscall_no) {
    PRINTLOG(KERNEL, LOG_ERROR, "syscall_handler_notfound_error: handler_no=%llx", syscall_no);
    return -1;
}

__attribute__((naked, no_stack_protector)) void syscall_handler(void) {
    asm volatile (
        "push %%r11\n"
        "push %%rbx\n"
        "push %%r12\n"
        "push %%r13\n"
        "push %%r14\n"
        "push %%r15\n"
        "push %%rsp\n"
        "push %%rcx\n"
        "push %%rbp\n"
        "mov %%rsp, %%rbp\n"
        "push %%rax\n"
        "push %%rdi\n"
        "push %%rsi\n"
        "push %%rdx\n"
        "push %%r10\n"
        "push %%r8\n"
        "push %%r9\n"
        "lea 0x0(%%rip), %%rax\n"
        "movabs $_GLOBAL_OFFSET_TABLE_, %%r15\n"
        "add %%rax, %%r15\n"
        "push %%r15\n"
        "mov %%rsp, %%rdi\n"
        "sub $8, %%rsp\n"
        "movabsq $syscall_handler_create_stack@GOT, %%rax\n"
        "call *(%%r15,%%rax,1)\n"
        "cmp $0xFFFFFFFFFFFFFFFF, %%rax\n"
        "je syscall_handler_exit\n"
        "mov %%rax, %%rsp\n"
        "sti\n"
        "pop %%r15\n"
        "pop %%r9\n"
        "pop %%r8\n"
        "pop %%r10\n"
        "pop %%rdx\n"
        "pop %%rsi\n"
        "pop %%rdi\n"
        "pop %%rax\n"
        "cmp %0, %%rax\n"
        "jae syscall_handler_notfound_exit\n"
        "mov %%r10, %%rcx\n"
        "shl $0x3, %%rax\n"
        "movabs $SYSCALL_TABLE@GOT, %%rbx\n"
        "mov (%%r15,%%rbx,1), %%rbx\n"
        "add %%rax, %%rbx\n"
        "cmpq $0x0, (%%rbx)\n"
        "je syscall_handler_null_handler_exit\n"
        "call *(%%rbx)\n"
        "cli\n"
        "syscall_handler_exit:\n"
        "mov %%rbp, %%rsp\n"
        "pop %%rbp\n"
        "pop %%rcx\n"
        "pop %%rsp\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%rbx\n"
        "pop %%r11\n"
        "sysretq\n"
        "syscall_handler_null_handler_exit:\n"
        "cli\n"
        "mov %%rax, %%rdi\n"
        "mov %%rbx, %%rsi\n"
        "movabsq $syscall_handler_null_handler_error@GOT, %%rax\n"
        "call *(%%r15,%%rax,1)\n"
        "jmp syscall_handler_exit\n"
        "syscall_handler_notfound_exit:\n"
        "cli\n"
        "mov %%rax, %%rdi\n"
        "movabsq $syscall_handler_notfound_error@GOT, %%rax\n"
        "call *(%%r15,%%rax,1)\n"
        "jmp syscall_handler_exit\n"
        : : "i" (SYSCALL_TABLE_SIZE)
        );
}


int64_t syscall_function_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    UNUSED(arg1);
    UNUSED(arg2);
    UNUSED(arg3);
    UNUSED(arg4);
    UNUSED(arg5);
    UNUSED(arg6);

    asm volatile ("hlt\n");
    return 0;
}

int64_t syscall_function_cli_and_hlt(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
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
