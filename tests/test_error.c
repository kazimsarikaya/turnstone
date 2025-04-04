/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <utils.h>

typedef struct jump_registers_s {
    uint64_t rax; ///< register
    uint64_t rbx; ///< register
    uint64_t rcx; ///< register
    uint64_t rdx; ///< register
    uint64_t r8; ///< register
    uint64_t r9; ///< register
    uint64_t r10; ///< register
    uint64_t r11; ///< register
    uint64_t r12; ///< register
    uint64_t r13; ///< register
    uint64_t r14; ///< register
    uint64_t r15; ///< register
    uint64_t rsi; ///< register
    uint64_t rdi; ///< register
    uint64_t rsp; ///< register
    uint64_t rbp; ///< register
    uint8_t  fx_registers[512]; ///< register
    uint64_t rflags; ///< register 0x280
    uint64_t rip; ///< program counter 0x288
    int64_t  error; ///<error code 0x290
    uint64_t return_rip; ///< return rip 0x298
} __attribute__((aligned(8))) jump_registers_t[1];


__attribute__((naked, no_stack_protector)) void                        jump_save_registers(jump_registers_t regs);
__attribute__((naked, no_stack_protector)) void                        jump_load_registers(jump_registers_t regs);
__attribute__((noinline, noclone, returns_twice, optimize(0))) int64_t jump_set(jump_registers_t jump_regs);
__attribute__((noinline, noclone, optimize(0))) void                   jump_return(jump_registers_t jump_regs, int64_t error);


int64_t jump_set(jump_registers_t jump_regs) {
    jump_save_registers(jump_regs);

    asm volatile (
        "mov 0x8(%rbp), %rax\n"
        "mov %rax, 0x298(%rdi)\n"
        "leaq 7(%rip), %rax\n"
        "mov %rax, 0x288(%rdi)\n"
        "leaq 6(%rip), %rax\n"
        "push %rax\n"
        "jmp jump_load_registers\n"
        "mov 0x290(%rdi), %rax\n"
        "test %rax, %rax\n"
        "jz to_exit\n"
        "mov 0x298(%rdi), %rax\n"
        "mov %rax, 0x8(%rbp)\n"
        "to_exit: movq 0x290(%rdi), %rax\n"
        "mov -0x8(%rbp),%rbx\n"
        "leave\n"
        "ret\n"
        );

    return 0;
}

void jump_return(jump_registers_t jump_regs, int64_t error) {
    UNUSED(error);
    UNUSED(jump_regs);
    asm volatile (
        "mov %rsi, %rax\n"
        "mov %rax, 0x290(%rdi)\n"
        "mov 0x288(%rdi), %rax\n"
        "mov 0x70(%rdi), %rsp\n"
        "mov 0x78(%rdi), %rbp\n"
        "jmp *%rax\n"
        );
}


__attribute__((naked, no_stack_protector)) void jump_save_registers(jump_registers_t regs) {
    __asm__ __volatile__ (
        "mov %%rax, %0\n"
        "mov %%rbx, %1\n"
        "mov %%rcx, %2\n"
        "mov %%rdx, %3\n"
        "mov %%r8,  %4\n"
        "mov %%r9,  %5\n"
        "mov %%r10, %6\n"
        "mov %%r11, %7\n"
        "mov %%r12, %8\n"
        "mov %%r13, %9\n"
        "mov %%r14, %10\n"
        "mov %%r15, %11\n"
        "mov %%rdi, %12\n"
        "mov %%rsi, %13\n"
        "mov %%rbp, %14\n"
        "fxsave %15\n"
        "push %%rax\n"
        "pushfq\n"
        "mov (%%rsp), %%rax\n"
        "mov %%rax, %16\n"
        "popfq\n"
        "pop %%rax\n"
        "mov %%rsp, %17\n"
        "retq\n"
        : :
        "m" (regs->rax),
        "m" (regs->rbx),
        "m" (regs->rcx),
        "m" (regs->rdx),
        "m" (regs->r8),
        "m" (regs->r9),
        "m" (regs->r10),
        "m" (regs->r11),
        "m" (regs->r12),
        "m" (regs->r13),
        "m" (regs->r14),
        "m" (regs->r15),
        "m" (regs->rdi),
        "m" (regs->rsi),
        "m" (regs->rbp),
        "m" (regs->fx_registers),
        "m" (regs->rflags),
        "m" (regs->rsp)
        );
}

__attribute__((naked, no_stack_protector)) void jump_load_registers(jump_registers_t regs) {
    __asm__ __volatile__ (
        "mov %0,  %%rax\n"
        "mov %1,  %%rbx\n"
        "mov %2,  %%rcx\n"
        "mov %3,  %%rdx\n"
        "mov %4,  %%r8\n"
        "mov %5,  %%r9\n"
        "mov %6,  %%r10\n"
        "mov %7,  %%r11\n"
        "mov %8,  %%r12\n"
        "mov %9,  %%r13\n"
        "mov %10, %%r14\n"
        "mov %11, %%r15\n"
        "mov %13, %%rsi\n"
        "mov %14, %%rbp\n"
        "fxrstor %15\n"
        "push %%rax\n"
        "mov %16, %%rax\n"
        "push %%rax\n"
        "popfq\n"
        "pop %%rax\n"
        "mov %17, %%rsp\n"
        "mov %12, %%rdi\n"
        "retq\n"
        : :
        "m" (regs->rax),
        "m" (regs->rbx),
        "m" (regs->rcx),
        "m" (regs->rdx),
        "m" (regs->r8),
        "m" (regs->r9),
        "m" (regs->r10),
        "m" (regs->r11),
        "m" (regs->r12),
        "m" (regs->r13),
        "m" (regs->r14),
        "m" (regs->r15),
        "m" (regs->rdi),
        "m" (regs->rsi),
        "m" (regs->rbp),
        "m" (regs->fx_registers),
        "m" (regs->rflags),
        "m" (regs->rsp)
        );
}



int32_t main(uint32_t argc, char_t** argv);

#define TRY do{ jump_registers_t ___jr; memory_memclean(___jr, sizeof(jump_registers_t)); switch( jump_set(___jr) ) { case 0: while(1) {
#define CATCH(x) break; case x:
#define FINALLY break; } default:
#define ETRY } } while(0)
#define THROW(x) jump_return(___jr, x)

#define FOO_EXCEPTION (1)
#define BAR_EXCEPTION (2)
#define BAZ_EXCEPTION (3)


int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    TRY {
        printf("In Try Statement\n");
        THROW( BAR_EXCEPTION );
        printf("I do not appear\n");
    } CATCH( FOO_EXCEPTION ) {
        printf("Got Foo!\n");
    } CATCH( BAR_EXCEPTION ) {
        printf("Got Bar!\n");
    } CATCH( BAZ_EXCEPTION ) {
        printf("Got Baz!\n");
    } FINALLY {
        printf("...et in arcadia Ego\n");
    } ETRY;

    print_success("TESTS PASSED");

    return 0;
}
