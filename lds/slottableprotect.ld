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
    } =0x0
    .slottable : AT(0x110)
    {
      *(.slottable)
      . = 0xF0;
    } =0x0
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
