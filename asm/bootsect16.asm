.code16
.text
.global __start

__start:
mov    %ax, %cx
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

cmp    $0xaa55, %cx
jne    check_pxe_and_load

mov    %dl,BOOT_DRIVE

mov    $bootmsg_1,%si
call   print_msg

cmp    $0x0,%dl
je     load_disk.err

call   check_disk_ext

mov    $load_start_stage2,%si
call   print_msg

mov    $slottableprotect, %ax
push   %ax
mov    $0x7c0, %ax
push   %ax
mov    $0x1, %ax //sector count
push   %ax
mov    $0x1, %ax //lba
push   %ax
call   load_disk

mov    $slottable, %bx
mov    $0x0, %si
next_slot:
mov    (%bx, %si), %al
cmp    $0x3, %al
je     stage2_found
add    $0x18, %si
cmp    $0xF0, %si
je     load_disk.err
jmp    next_slot
stage2_found:

mov    8(%bx, %si), %cx
mov    16(%bx, %si), %dx
sub    %cx, %dx
inc    %dx

xor    %ax, %ax
push   %ax //offset
pushw  $0x100 //segment
push   %dx
push   %cx
call   load_disk

mov    $load_end_stage2,%si
call   print_msg

mov    BOOT_DRIVE, %al
mov    $0x0, %ah
push   %ax
xor    %ax, %ax
jmp    $0x100,$0x0

panic:
hlt
jmp    panic

check_pxe_and_load:
mov   $0x202, %bx // at 0x200 slottable starts first 2 bytes jmp to __realstart then !PXE
mov   (%bx), %eax //load !PXE
cmp   $0x45585021, %eax // compare !PXE
jne   pxe_error
mov   $0x7e0, %ax
mov   %ax, %ds
mov   %ax, %es
jmp   $0x7e0, $0x6

pxe_error:
hlt
jmp pxe_error
mov   $errmsg_disk, %si
call  print_msg
jmp   panic

check_disk_ext:
mov    $0x4100, %ax
xor    %bx,%bx
int    $0x13
cmp    $0xaa55, %bx
jne    load_disk.err
ret

load_disk:
push   %bp
mov    %sp, %bp
mov    $dap, %bx
movw   $0x0010, 0x0(%bx) // header
mov    0x6(%bp), %ax
mov    %ax, 0x2(%bx) // sectors to read
mov    0xa(%bp), %ax
mov    %ax, 0x4(%bx) // offset to put
mov    0x8(%bp), %ax
mov    %ax, 0x6(%bx) // segment to put
mov    0x4(%bp), %ax
mov    %ax, 0x8(%bx) // lba first word
movw   $0x0, 0xa(%bx) // lba second word
movl   $0x0, 0xc(%bx) // lba third & forth word
mov    BOOT_DRIVE, %dl
mov    %bx, %si
mov    $0x4200, %ax
int    $0x13
jc     load_disk.err
mov    %bp, %sp
pop    %bp
ret

load_disk.err:
mov    $errmsg_disk,%si
call   print_msg
jmp    panic

print_msg:
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
  ret

.data
bootmsg_1:
  .asciz "Booting OS"
load_start_stage2:
  .asciz "Loading stage2"
load_end_stage2:
  .asciz "Loading ended"
errmsg_disk:
  .asciz "Disk error"
.bss
BOOT_DRIVE: .byte 0x00
.lcomm dap, 16
.lcomm slottableprotect, 0x110
.lcomm slottable, 0xF0
