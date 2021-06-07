.code16
.text
.global __start

__start:
jmp __realstart
.asciz "!PXE"
__realstart:
cli
mov    %cs, %ax
mov    %ax, %ds
mov    $0x100, %ax
mov    %ax, %es /*  destination stage will be at 0x1000 */
mov    slot_stage2.end, %eax
mov    slot_stage2.start, %ebx
call    move_stage

mov    %cs, %ax
mov    %ax, %ds
mov    $0x2000, %ax
mov    %ax, %es /*  destination stage will be at 0x20000 */
mov    slot_stage3.end, %eax
mov    slot_stage3.start, %ebx
call    move_stage

jmp    start_stage2

move_stage:
push   %ds /* store destination segment */
sub    %ebx, %eax
inc    %eax
mov    %eax, %esi /* our sector counter */

dec    %ebx /*  dec dx to relative this code segment */
mov    $0x20, %edx /* sector size as segment */
imul   %ebx, %edx
mov    %cs, %ax
add    %dx, %ax /*  add offset as segment */
mov    %ax, %ds /* ds is source start in terms of segment */

mov    $0x20, %dx /* sector size as segment */

.sectorloop: /* loops sector count tested with %esi */
mov    $0x0, %bx
mov    $0x200, %ecx /* sector byte count */
.sectorbytes: /* loop for each byte of sector tested with %cx */
mov    %ds:(%bx), %al /* get the byte */
mov    %al, %es:(%bx)
inc    %bx
dec    %cx
test   %cx, %cx
jnz    .sectorbytes /* loop until sector finished */
dec    %esi
test   %esi, %esi
jz     .endofmove /* no remaining sector then end loop */
mov    %ds, %ax
add    %dx, %ax /* if segment incremented by 0x20 it means 200 bytes */
mov    %ax, %ds
mov    %es, %ax
add    %dx, %ax /* if segment incremented by 0x20 it means 200 bytes */
mov    %ax, %es
jmp    .sectorloop /* loop until all sectors completed */

.endofmove:
pop    %ds /* restore destination segment */
ret

start_stage2:
mov    $0x0, %ax /*  BOOT_DRIVE will be 0x0 for pxe boot */
push   %ax /*  send BOOT_DRIVE to stack */
xor    %eax, %eax
jmp    $0x100, $0x0 /*  jmp to stage2 */


/*  align slor_mbr to 0x110 with correct ld addressing */
slot_mbr_header:
.balign 0x100, 0x00
.quad 0,0
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
slot_stage3:
slot_stage3.type:
.byte 4
slot_stage3.unused:
.byte 0, 0, 0, 0, 0, 0, 0
slot_stage3.start:
.byte 0, 0, 0, 0, 0, 0, 0, 0
slot_stage3.end:
.byte 0, 0, 0, 0, 0, 0, 0, 0
slot_mbr_end:
.balign 0x200, 0x00
