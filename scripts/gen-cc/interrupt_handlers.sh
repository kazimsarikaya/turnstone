#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.


cat <<EOF
/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <utils.h>
#include <cpu/interrupt.h>
#include <cpu/syscall.h>
#include <cpu/cpu_registers.h>

_Static_assert(sizeof_field(cpu_registers_t, avx512f) == 0x2000, "cpu_registers_t.avx512f size must be 0x2000 bytes");
_Static_assert(sizeof_field(interrupt_frame_ext_t, avx512f) == 0x2080, "interrupt_frame_ext_t.avx512f size must be 0x2080");

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

static syscall_generic_handler_f syscall_generic_handler_handle __attribute__((used)) = 0;

void syscall_handlers_set_generic_handler(syscall_generic_handler_f handler) {
    syscall_generic_handler_handle = handler;
}

__attribute__((naked, no_stack_protector))
void syscall_jump_to_userspace(uint64_t cr3, uint64_t rip, uint64_t rsp) {
    UNUSED(cr3);
    UNUSED(rip);
    UNUSED(rsp);
    asm volatile (
        "xor %%rbp, %%rbp\n"
        "mov %%rdx, %%rsp\n"
        "mov %%rsi, %%rcx\n"
        "mov \$0x202, %%r11\n"
        "mov %%rdi, %%cr3\n"
        "swapgs\n"
        "sysretq\n"
        ::: "memory"
        );
}

__attribute__((naked, no_stack_protector))
void syscall_handler(void) {
    asm volatile (
        "swapgs\n"
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
        "mov %rax, %cr3\n"
        "movq syscall_generic_handler_handle(%rip), %rax\n"
        "subq \$8, %rsp\n" // align stack to 16 bytes for call
        "sti\n" // enable interrupts before calling syscall handler to allow nested interrupts during syscalls
        "call *%rax\n"
        "cli\n" // disable interrupts after syscall handler returns
        "add \$8, %rsp\n" // restore stack after call
        "pop %rsp\n"
        "pop %rax\n"
        "mov %rax, %cr3\n"
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
        "add \$0x2080, %rsp\n"
        "swapgs\n"
        "sysretq\n"
        );
}

EOF

for i in 8 $(seq 10 14) 17 21; do

j=`printf "%02x" $i`

cat <<EOF
__attribute__((naked, no_stack_protector))
static void interrupt_naked_handler_int_0x${j}(void) {
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
        "movq interrupt_handlers_kernel_cr3_value(%rip), %rdx\n"
        "cmp %rax, %rdx\n"
        "je 1f\n"
        "mov %rdx, %cr3\n"
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
__attribute__((naked, no_stack_protector))
static void interrupt_naked_handler_int_0x${j}(void) {
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
        "movq interrupt_handlers_kernel_cr3_value(%rip), %rdx\n"
        "cmp %rax, %rdx\n"
        "je 1f\n"
        "mov %rdx, %cr3\n"
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

__attribute__((naked, no_stack_protector))
static void interrupt_dummy_handler(void) {
     while (true) {
        asm volatile ("hlt");
    }
}

void interrupt_inject_dummy_interrupt_handler(uint8_t int_no) {
    descriptor_register_t int_desc;
    asm volatile ("sidt %0" : "=m" (int_desc));
    descriptor_idt_t* idt = (descriptor_idt_t*)int_desc.base;
    uint64_t fa = (uint64_t)interrupt_dummy_handler;
    idt[int_no].ist = 0;
    idt[int_no].offset_1 = (fa & 0xFFFF);
    idt[int_no].offset_2 = ((fa >> 16) & 0xFFFF);
    idt[int_no].offset_3 = ((fa >> 32) & 0xFFFFFFFF);
}

#endif
EOF
