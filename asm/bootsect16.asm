.code16
.text
.global __start

__start:
cmp    $0xaa55, %ax
je    __start_normal
mov    $load_from_network,%si
call   print_msg
cli
mov    $0x800, %ecx
push   %ds
mov    %ax, %ds
xor    %ax, %ax
mov    %ax, %si
mov    $0x100, %ax
push   %es
mov    %ax, %es
xor    %ax, %ax
mov    %ax, %di
cld
rep    movsb
pop    %es
pop    %ds
sti
jmp    __start_network
__start_normal:
cli
mov    $0x7c0, %ax
mov    %ax, %ds
mov    %ax, %es
mov    %ax, %fs
mov    %ax, %gs
mov    %ax, %ss
mov    $__stack_top,%bp
mov    %bp,%sp
sti

__start_network:
mov    %dl,BOOT_DRIVE
cli
call   enable_A20
sti
mov    $bootmsg_1,%si
call   print_msg

cmp    $0x0,%dl
je     __skip_disk_load

mov    $load_start_stage2,%si
call   print_msg

call   load_disk

mov    $load_end_stage2,%si
call   print_msg

__skip_disk_load:
mov    BOOT_DRIVE, %al
mov    $0x0, %ah
push   %ax
xor    %ax, %ax
jmp    $0x100,$0x0

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
push   %es
mov    $0x100,%ax
mov    %ax, %es
mov    $0x2,%ah
mov    $0x10,%al
mov    $0x0,%ch
mov    $0x2,%cl
mov    $0x0,%dh
mov    BOOT_DRIVE,%dl
xor    %bx,%bx
int    $0x13
jb     load_disk.err
jmp    load_disk.end
load_disk.err:
mov    $errmsg_disk,%si
call   print_msg
jmp    panic
load_disk.end:
pop    %es
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
  mov    $0x0d, %al
  int    $0x10
  mov    $0x0A, %al
  int    $0x10
  popa
  ret

.data
BOOT_DRIVE: .byte 0x00
bootmsg_1:
  .asciz "Booting OS"
load_start_stage2:
  .asciz "Loading stage2"
load_end_stage2:
  .asciz "Loading ended"
errmsg_disk:
  .asciz "Disk error"
load_from_network:
  .asciz "Diskless boot"
