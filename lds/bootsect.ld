OUTPUT_FORMAT(binary)
ENTRY(__start)
SECTIONS
{
    . = 0x0;
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
    .sig : AT(0x1FE)
    {
        SHORT(0xaa55);
    }
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
