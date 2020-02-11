.code16
.text
.global __kstart
.global BOOT_DRIVE
__kstart:
  cmp    $0x9020, %ax
  jne    __kstart_normal
  mov    $0x0, %dl
  jmp    $0x9000, $0x0

__kstart_normal:
  pop %bx

  cli
  mov $0x100, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $__stack_top, %esp
  mov %esp,%ebp
  sti
  cld
  mov %bl,BOOT_DRIVE
  call kmain16
  cli
.loop:
  hlt
  jmp .loop
.data
BOOT_DRIVE: .byte 0x00
