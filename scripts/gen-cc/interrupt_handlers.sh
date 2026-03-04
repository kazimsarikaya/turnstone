#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.


cat <<EOF
/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <cpu/interrupt.h>

MODULE("turnstone.kernel.cpu.interrupt.handlers");

#ifndef ___DEPEND_ANALYSIS

static uint64_t interrupt_handlers_kernel_cr3_value __attribute__((used)) = 0;

void interrupt_handlers_set_kernel_cr3_value(uint64_t cr3_value) {
    interrupt_handlers_kernel_cr3_value = cr3_value;
}

static interrupt_generic_handler_f interrupt_generic_handler_handle __attribute__((used)) = 0;

void interrupt_handlers_set_generic_handler(interrupt_generic_handler_f handler) {
    interrupt_generic_handler_handle = handler;
}

EOF

for i in 8 $(seq 10 14) 17 21; do

j=`printf "%02x" $i`

cat <<EOF
static void interrupt_naked_handler_int_0x${j}(void);
__attribute__((naked, no_stack_protector)) void interrupt_naked_handler_int_0x${j}(void) {
    asm volatile (
        "push \$${i}\n" // push interrupt number
        "subq \$0x2080, %rsp\n"
        "push %r15\n"
        "push %r14\n"
        "push %r13\n"
        "push %r12\n"
        "push %r11\n"
        "push %r10\n"
        "push %r9\n"
        "push %r8\n"
        "push %rbp\n"
        "push %rdi\n"
        "push %rsi\n"
        "push %rdx\n"
        "push %rcx\n"
        "push %rbx\n"
        "push %rax\n"
        "mov %cr3, %rax\n"
        "push %rax\n"
        "push %rsp\n"
        "mov %rsp, %rdi\n"
        "movq interrupt_handlers_kernel_cr3_value(%rip), %rax\n"
        "mov %cr3, %rdx\n"
        "cmp %rax, %rdx\n"
        "je 1f\n"
        "mov %rax, %cr3\n"
        "1:\n"
        "movq interrupt_generic_handler_handle(%rip), %rax\n"
        "call *%rax\n"
        "pop %rsp\n"
        "pop %rax\n"
        "mov %cr3, %rbx\n"
        "cmp %rax, %rbx\n"
        "jne 1f\n"
        "mov %rax, %cr3\n"
        "1:\n"
        "pop %rax\n"
        "pop %rbx\n"
        "pop %rcx\n"
        "pop %rdx\n"
        "pop %rsi\n"
        "pop %rdi\n"
        "pop %rbp\n"
        "pop %r8\n"
        "pop %r9\n"
        "pop %r10\n"
        "pop %r11\n"
        "pop %r12\n"
        "pop %r13\n"
        "pop %r14\n"
        "pop %r15\n"
        "add \$0x2088, %rsp\n"
        "iretq\n"
        );
}
EOF

done

for i in $(seq 0 7) 9 15 16 18 19 20 $(seq 22 31) $(seq 32 255); do

j=`printf "%02x" $i`

cat <<EOF
static void interrupt_naked_handler_int_0x${j}(void);
__attribute__((naked, no_stack_protector)) void interrupt_naked_handler_int_0x${j}(void) {
    asm volatile (
        "push \$0\n" // push error code
        "push \$${i}\n" // push interrupt number
        "subq \$0x2080, %rsp\n"
        "push %r15\n"
        "push %r14\n"
        "push %r13\n"
        "push %r12\n"
        "push %r11\n"
        "push %r10\n"
        "push %r9\n"
        "push %r8\n"
        "push %rbp\n"
        "push %rdi\n"
        "push %rsi\n"
        "push %rdx\n"
        "push %rcx\n"
        "push %rbx\n"
        "push %rax\n"
        "mov %cr3, %rax\n"
        "push %rax\n"
        "push %rsp\n"
        "mov %rsp, %rdi\n"
        "movq interrupt_handlers_kernel_cr3_value(%rip), %rax\n"
        "mov %cr3, %rdx\n"
        "cmp %rax, %rdx\n"
        "je 1f\n"
        "mov %rax, %cr3\n"
        "1:\n"
        "movq interrupt_generic_handler_handle(%rip), %rax\n"
        "call *%rax\n"
        "pop %rsp\n"
        "pop %rax\n"
        "mov %cr3, %rbx\n"
        "cmp %rax, %rbx\n"
        "je 1f\n"
        "mov %rax, %cr3\n"
        "1:\n"
        "pop %rax\n"
        "pop %rbx\n"
        "pop %rcx\n"
        "pop %rdx\n"
        "pop %rsi\n"
        "pop %rdi\n"
        "pop %rbp\n"
        "pop %r8\n"
        "pop %r9\n"
        "pop %r10\n"
        "pop %r11\n"
        "pop %r12\n"
        "pop %r13\n"
        "pop %r14\n"
        "pop %r15\n"
        "add \$0x2090, %rsp\n"
        "iretq\n"
        );
}
EOF

done

cat <<EOF
typedef void (*interrupt_dummy_noerrcode_int_ptr)(void);

static const interrupt_dummy_noerrcode_int_ptr interrupt_dummy_noerrcode_list[256] = {
EOF

for i in $(seq 0 255); do

j=`printf "%02x" $i`

cat <<EOF
&interrupt_naked_handler_int_0x${j},
EOF
done

cat <<EOF
};

void interrupt_register_dummy_handlers(descriptor_idt_t* idt) {
  uint64_t fa;
  for(uint32_t i=0;i<256;i++){
    fa=(uint64_t)interrupt_dummy_noerrcode_list[i];
    DESCRIPTOR_BUILD_IDT_SEG(idt[i], fa, KERNEL_CODE_SEG, 0, DPL_KERNEL)
  }
}

#endif
EOF
