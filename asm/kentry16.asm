.code16
.text
.global __kstart
.global GDT_REGISTER
.global IDT_REGISTER
.global SYSTEM_INFO
.global BOOT_DRIVE
.extern __kpagetable_p4
__kstart:
  cli
  mov $0xFF, %al // disable pic
  out %al, $0xA1 // slave
  out %al, $0x21 // master
  nop // wait some seconds
  nop // wait some seconds

  pop %bx
  call   enable_A20
  mov %cs, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  mov %ax, %fs
  mov %ax, %gs
  mov $__stack_top, %esp
  mov %esp, %ebp
  cld
  mov %bl, BOOT_DRIVE
  call kmain16
  cmp $0x00, %al
  jne .loop

  xor %eax, %eax
  mov $IDT_REGISTER, %eax
  lidt (%eax)

  xor %eax, %eax
  mov $GDT_REGISTER, %eax
  lgdt (%eax)

  mov %cr4, %eax
  bts $0x5, %eax //pae
  bts $0x7, %eax //pge
  mov %eax, %cr4

  mov $0xC0000080, %ecx
  rdmsr
  bts $0x8, %eax
  wrmsr

  mov SYSTEM_INFO, %edx
  xor %eax, %eax
  mov %cs, %ax
  shl $0x04, %ax
  mov %ax, %bx
  add $long_mode, %ax // fix long_mode address
  pushw $0x08 // gdt cs segment
  push %ax
  mov %bx, %ax
  add $fix_cs, %ax // fix_cs absolute address
  pushw $0x0
  push %ax
  retf // change cs=0x0, eip=fix_cs
fix_cs:
  mov $0x0, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  mov %cr0, %eax
  bts $0x0, %eax //pe
  bts $0x10, %eax //wp
  bts $0x1F, %eax //pg
  mov %eax, %cr0

  retf // change cs and go long mode

long_mode: // i386 as do not know how to jump so hard coded
.byte 0x48, 0xb8, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 // mov $0x20000,%rax
.byte 0xff, 0xe0 // jmp *%rax

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
BOOT_DRIVE:
.byte 0x00
