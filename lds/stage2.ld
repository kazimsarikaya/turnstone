OUTPUT_FORMAT(binary)
ENTRY(__start)
SECTIONS
{
    . = 0x1000; /* same as last far jump at bootsect.asm */
    .text :
    {
        *(.text);
    }
    .data :
    {
        *(.data)
        *(.rodata)
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
    . = ALIGN(0x1000);
    __stack_bottom = .;
    . = . + 0x1000;
    __stack_top = .;
    /DISCARD/ :
    {
    *(.note*);
    *(.iplt*);
    *(.igot*);
    *(.rel*);
    *(.comment);
    *(.eh_frame);
    }
}
