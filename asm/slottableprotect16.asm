.code16
.text
.global __start

// FIXME: read slot type 3 and calculate stage2 length.
__start:
cli
mov    $0x800, %ecx
push   %ds
add    $0x20, %ax
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
mov    $0x0, %ax
push   %eax
jmp    $0x100, $0x0



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
