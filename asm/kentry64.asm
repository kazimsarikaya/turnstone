.code64
.text
.global ___kstart64

___kstart64:
  cli
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
  cli
___kstart64.loop:
  hlt
  jmp ___kstart64.loop
