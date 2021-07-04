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
mov    %ax, %gs
mov    %ax, %ss
mov    $0xb800, %ax
mov    %ax, %fs
mov    $__stack_top,%bp
mov    %bp,%sp

cmp    $0xaa55, %cx
jne    check_pxe_and_load

mov    %dl,BOOT_DRIVE

call   clear_screen

mov    $bootmsg_1,%si
call   print_msg

mov    $0, %di
mov    $0x0800, %ax
int    $0x13
test   %ah, %ah
jnz    disk.err
inc    %dh /* TODO: check overflow */
mov    %dh, HEAD_COUNT
push   %cx
and    0x003F, %cx
mov    %cl, SECTOR_COUNT
pop    %cx
shr    $6, %cx
inc    %cx
mov    %cx, CYLINDER_COUNT

mov    $load_start_stage2,%si
call   print_msg

mov    $slottableprotect, %bx
mov    $0x7c0, %ax
mov    %ax, %es
mov    $0x0201, %ax
mov    $0x0002, %cx
mov    $0x00, %dh
mov    BOOT_DRIVE, %dl
int    $0x13
jc     load_disk.err

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

mov    $0x02, %ah
mov    %dl, %al
mov    $0x100, %bx
mov    %bx, %es
xor    %bx, %bx
mov    $0x0003, %cx /* TODO: calculate lba */
mov    $0x00, %dh
mov    BOOT_DRIVE, %dl
int    $0x13
jc     load_disk.err

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
mov   $0x202, %bx /* at 0x200 slottable starts first 2 bytes jmp to __realstart then !PXE */
mov   (%bx), %eax /* load !PXE */
cmp   $0x45585021, %eax /*  compare !PXE */
jne   pxe_error
mov   $0x7e0, %ax
mov   %ax, %ds
mov   %ax, %es
jmp   $0x7e0, $0x6

pxe_error:
mov   $errmsg_pxe, %si
call  print_msg
jmp   panic

disk.err:
mov    $errmsg_disk,%si
call   print_msg
jmp    panic

load_disk.err:
mov    $errmsg_loaddisk,%si
call   print_msg
jmp    panic

load_nodiskext.err:
mov    $errmsg_nodiskext,%si
call   print_msg
jmp    panic

clear_screen:
  mov    $2000, %cx
  mov    $0x0F, %ah
  mov    $0x20, %al
  xor    %bx, %bx
clear_screen.loop:
  mov    %ax, %fs:(%bx)
  add    $2, %bx
  dec    %cx
  test   %cx, %cx
  jnz    clear_screen.loop
  ret

print_msg:
  mov    $0x0F,%ah
  movzb  CURSOR_Y, %cx
  mov    %cx, %di
  imul   $160, %di
  xor    %bx, %bx
print_msg.loop:
  lodsb
  cmp    $0x0,%al
  je     print_msg.end
  mov    %ax, %fs:(%di)
  add    $2, %di
  jmp    print_msg.loop
print_msg.end:
  inc    %cl
  mov    %cl, CURSOR_Y
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
errmsg_pxe:
  .asciz "PXE error"
errmsg_loaddisk:
  .asciz "Load Disk error"
errmsg_nodiskext:
  .asciz "No disk extensions"
.bss
BOOT_DRIVE: .byte 0x00
.lcomm slottableprotect, 0x110
.lcomm slottable, 0xF0
CURSOR_Y: .byte 0x00
HEAD_COUNT: .byte 0x00
SECTOR_COUNT: .byte 0x00
CYLINDER_COUNT: .word 0x0000
