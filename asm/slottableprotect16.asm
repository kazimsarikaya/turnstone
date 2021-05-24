.code16
.text
.global __start
.asciz "!PXE"

// FIXME: read slot type 3 and calculate stage2 length.
__start:
cli
mov    $slot_stage2.end, %eax
mov    $slot_stage2.start, %ebx
sub    %ebx, %eax
inc    %eax
mov    $0x200, %ebx
imul   %ebx, %eax // eax contains byte count of stage2
mov    %eax, %ecx // loop count
push   %ds
mov    $0x800, %ax // stage2 at 0x8000
mov    %ax, %ds
xor    %ax, %ax
mov    %ax, %si // source index 0
mov    $0x100, %ax // destination stage2 will be at 0x1000
push   %es
mov    %ax, %es
xor    %ax, %ax
mov    %ax, %di // destination index 0

cld
rep    movsb

pop    %es
pop    %ds

mov    $0x0, %ax // BOOT_DRIVE will be 0x0 for pxe boot
push   %ax // send BOOT_DRIVE to stack
xor    %eax, %eax
jmp    $0x100, $0x0 // jmp to stage2



.section .slottable
slot_mbr:
slot_mbr.type:
.byte 1
slot_mbr.unused:
.byte 0, 0, 0, 0, 0, 0, 0
slot_mbr.start:
.byte 0, 0, 0, 0, 0, 0, 0, 0
slot_mbr.end:
.byte 0, 0, 0, 0, 0, 0, 0, 0
slot_slottable:
slot_slottable.type:
.byte 2
slot_slottable.unused:
.byte 0, 0, 0, 0, 0, 0, 0
slot_slottable.start:
.byte 1, 0, 0, 0, 0, 0, 0, 0
slot_slottable.end:
.byte 1, 0, 0, 0, 0, 0, 0, 0
slot_stage2:
slot_stage2.type:
.byte 3
slot_stage2.unused:
.byte 0, 0, 0, 0, 0, 0, 0
slot_stage2.start:
.byte 0, 0, 0, 0, 0, 0, 0, 0
slot_stage2.end:
.byte 0, 0, 0, 0, 0, 0, 0, 0
