.code16
.text
.global __kstart
.global BOOT_DRIVE
__kstart:
  pop %ax
  mov %al,BOOT_DRIVE

  cli
  xor %ax, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov $__stack_top, %esp
  mov %esp,%ebp
  sti
  cld
  call kmain16
  cli
.loop:
  hlt
  jmp .loop
.data
BOOT_DRIVE: .byte 0x00
