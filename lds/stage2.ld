OUTPUT_FORMAT(binary)
ENTRY(mystart)
SECTIONS
{
    . = 0x1000;
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
