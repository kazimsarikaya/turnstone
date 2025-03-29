#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.


cat <<EOF
/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <cpu.h>
#include <cpu/descriptor.h>
#include <cpu/interrupt.h>

MODULE("turnstone.kernel.cpu.interrupt");

#ifndef ___DEPEND_ANALYSIS

void interrupt_register_dummy_handlers(descriptor_idt_t* idt);

EOF

for i in 8 $(seq 10 14) 17 21; do

j=`printf "%02x" $i`

cat <<EOF
void interrupt_naked_handler_int_0x${j}(void);
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
        "push %rsp\n"
        "mov %rsp, %rdi\n"
        "sub \$0x8, %rsp\n"
        "lea 0x0(%rip), %rax\n"
        "movabsq \$_GLOBAL_OFFSET_TABLE_, %rbx\n"
        "add %rax, %rbx\n"
        "movabsq \$interrupt_generic_handler@GOT, %rax\n"
        "call *(%rbx, %rax, 1)\n"
        "add \$0x8, %rsp\n"
        "pop %rsp\n"
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
void interrupt_naked_handler_int_0x${j}(void);
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
        "push %rsp\n"
        "mov %rsp, %rdi\n"
        "sub \$0x8, %rsp\n"
        "lea 0x0(%rip), %rax\n"
        "movabsq \$_GLOBAL_OFFSET_TABLE_, %rbx\n"
        "add %rax, %rbx\n"
        "movabsq \$interrupt_generic_handler@GOT, %rax\n"
        "call *(%rbx, %rax, 1)\n"
        "add \$0x8, %rsp\n"
        "pop %rsp\n"
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

const interrupt_dummy_noerrcode_int_ptr interrupt_dummy_noerrcode_list[256] = {
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
