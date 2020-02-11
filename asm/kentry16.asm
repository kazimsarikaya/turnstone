.code16
.text
.global __kstart
.global BOOT_DRIVE
__kstart:
  pop %bx

  cli
  call   enable_A20
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


enable_A20:
  call   enable_A20.a20wait
  mov    $0xad,%al
  out    %al,$0x64

  call   enable_A20.a20wait
  mov    $0xd0,%al
  out    %al,$0x64

  call   enable_A20.a20wait2
  in     $0x60,%al
  push   %eax

  call   enable_A20.a20wait
  mov    $0xd1,%al
  out    %al,$0x64

  call   enable_A20.a20wait
  pop    %eax
  or     $0x2,%al
  out    %al,$0x60

  call   enable_A20.a20wait
  mov    $0xae,%al
  out    %al,$0x64

  call   enable_A20.a20wait
  ret

  enable_A20.a20wait:
  in     $0x64,%al
  test   $0x2,%al
  jne    enable_A20.a20wait
  ret

  enable_A20.a20wait2:
  in     $0x64,%al
  test   $0x1,%al
  je     enable_A20.a20wait2
  ret

.bss
BOOT_DRIVE: .byte 0x00
