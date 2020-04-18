.code64
.text
.global ___kstart64
.global GDT_REGISTER
.global IDT_REGISTER
.global SYSTEM_INFO

___kstart64:
  cli
  mov %rdx, SYSTEM_INFO
  mov $0x0, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $__stack_top, %esp
  mov %esp,%ebp
  cld
  call kmain64
___kstart64.loop:
  cli
  hlt
  jmp ___kstart64.loop
.bss
.align 8
GDT_REGISTER:
.byte 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.align 8
IDT_REGISTER:
.byte 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
SYSTEM_INFO:
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
