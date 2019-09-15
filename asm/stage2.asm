[org 0x9000]
[bits 16]
stage2_main:
mov bx,cs
mov ds,bx


mov ah,0x00
mov al,0x03
int 0x10

cli
lgdt [gdt_desc]
mov eax,cr0
or al,0x01
mov cr0,eax

jmp 0x8:pmode_init


gdt_entries:
gdt_null:
dd 0x00000000
dd 0x00000000
gdt_kernel_cs:
dw 0xffff
dw 0x0000
db 0x00
db 0x9a
db 0xcf
db 0x00
gdt_kernel_ds:
dw 0xffff
dw 0x0000
db 0x00
db 0x92
db 0xcf
db 0x00
gdt_user_cs:
dd 0x00000000
dd 0x00000000
gdt_user_ds:
dd 0x00000000
dd 0x00000000
gdt_end:
gdt_desc:
limit: dw gdt_end - gdt_entries - 1
base: dd gdt_entries

[bits 32]
pmode_init:
mov eax,0x10
mov ds,ax
mov es,ax
mov fs,ax
mov gs,ax
mov ss,ax
mov ebp,0x8000
mov esp,0x8000

jmp pmode_main

pmode_main:
call clear_screen


mov al,'.'
mov ah,0x2a
mov ebx,0xb8000
mov [ebx],ax

end:
  hlt
  jmp end



clear_screen:
  pusha
  mov ebx,0xb8000
  mov ah,0x2a
  mov al,' '
  mov cx,0x07D0 ; hex screen buff
clear_screen_loop:
  mov [ebx],ax
  add ebx,2
  dec cx
  cmp cx,0
  jnz clear_screen_loop
  popa
  ret
