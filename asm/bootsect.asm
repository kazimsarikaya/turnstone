.code16
.text
.global __start

__start:
cli
mov    $__stack_top,%bp
mov    %bp,%sp
sti

mov    %dl,BOOT_DRIVE
cli
call   enable_A20
sti
mov    $bootmsg_1,%si
call   print_msg

mov    $load_start_stage2,%si
call   print_msg

call   load_disk

mov    $load_end_stage2,%si
call   print_msg

mov    (BOOT_DRIVE),%al
mov    $0x0, %ah
push   %ax

ljmp   $0x100,$0x0

panic:
hlt
jmp    panic

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

load_disk:
pusha
mov    BOOT_DRIVE,%bx
mov    $0x2,%ah
mov    $0x10,%al
mov    $0x0,%ch
mov    $0x2,%cl
mov    $0x0,%dh
mov    BOOT_DRIVE,%dl
mov    $0x1000,%bx
int    $0x13
jb     load_disk.err
jmp    load_disk.end
load_disk.err:
mov    $errmsg_disk,%si
call   print_msg
jmp    panic
load_disk.end:
popa
ret

print_msg:
  pusha
  mov    $0xe,%ah
print_msg.loop:
  lodsb
  cmp    $0x0,%al
  je     print_msg.end
  int    $0x10
  jmp    print_msg.loop
print_msg.end:
  popa
  ret

print_hex:
  pusha
  mov    %ax,%dx
  mov    HEX_DATA+4,%bx
  mov    $0x4,%cx
print_hex.loop:
  dec    %bx
  and    $0xf,%al
  cmp    $0xa,%al
  jl     print_hex.number
  add    $0x37,%al
  jmp    print_hex.cont
print_hex.number:
  add    $0x30,%al
print_hex.cont:
  mov    %al,(%bx)
  shr    $0x4,%dx
  mov    %dx,%ax
  dec    %cx
  cmp    $0x0,%cx
  jne    print_hex.loop
  mov    HEX_DATA,%bx
  call   0x99
  popa
  ret

.data
HEX_DATA: .byte 0x00,0x00,0x00,0x00,0x0d,0x0a,0x00
BOOT_DRIVE: .byte 0x00
bootmsg_1:
  .asciz "Booting OS...\r\n"
load_start_stage2:
  .asciz "Loading stage 2...\r\n"
load_end_stage2:
  .asciz "Loading ended.\r\n"
errmsg_disk:
  .asciz "Disk error\r\n"

.word 0xaa55
