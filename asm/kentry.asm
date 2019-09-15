.code16
.text
.global mystart
mystart:
  cli
  xor %ax, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov $__stack_top, %esp
  mov %esp,%ebp
  sti
  cld
  call kmain
  cli
.loop:
  hlt
  jmp .loop
.data
