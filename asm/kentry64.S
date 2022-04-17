/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
.code64
.type ___kstart64, @function
.global ___kstart64
.type SYSTEM_INFO, @object
.global SYSTEM_INFO

.section .text.___kstart64
___kstart64:
  cli
  mov $0xff, %al
  out %al, $0xa1
  out %al, $0x21
  nop
  nop
  mov %rcx, SYSTEM_INFO
  mov $0x0, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $__stack_top, %esp
  xor %ebp, %ebp

  /* enable sse */
  mov %cr0, %rax
  and $0xFFFB, %ax		/* clear coprocessor emulation CR0.EM */
  or  $0x02, %ax			/* set coprocessor monitoring  CR0.MP */
  bts $16, %rax       /* set write protect bit */
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

.section .bss.SYSTEM_INFO
.align 8
SYSTEM_INFO:
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
