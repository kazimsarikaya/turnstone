.code64
.type ___kstart64, @function
.global ___kstart64
.type GDT_REGISTER, @object
.global GDT_REGISTER
.type IDT_REGISTER, @object
.global IDT_REGISTER
.type SYSTEM_INFO, @object
.global SYSTEM_INFO

.section .text.___kstart64
___kstart64:
  cli
  mov %rcx, SYSTEM_INFO
  mov $0x0, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $__stack_top, %esp
  mov %esp, %ebp

  xor %rax, %rax
  mov $IDT_REGISTER, %rax
  sidt (%rax)

  xor %rax,%rax
  mov $GDT_REGISTER, %rax
  sgdt (%rax)

  /* enable sse */
  mov %cr0, %rax
  and $0xFFFB, %ax		/* clear coprocessor emulation CR0.EM */
  or  $0x02, %ax			/* set coprocessor monitoring  CR0.MP */
  mov %rax, %cr0
  mov %cr4, %rax
  or  $0x600, %ax		/* set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time */
  mov %rax, %cr4
  /* end enable sse */

  cld
  mov  $___kstart64, %edi

#ifndef ___TESTMODE
  call kmain64
#else
  call kmain_test
#endif

  test %al, %al
  jne ___kstart64.errloop
___kstart64.loop:
  hlt
  jmp ___kstart64.loop
___kstart64.errloop:
  cli
  hlt
  jmp ___kstart64.errloop

.section .bss.GDT_REGISTER
.align 8
GDT_REGISTER:
.byte 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

.section .bss.IDT_REGISTER
.align 8
IDT_REGISTER:
.byte 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

.section .bss.SYSTEM_INFO
.align 8
SYSTEM_INFO:
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
