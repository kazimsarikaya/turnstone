OUTPUT_FORMAT(binary)
ENTRY(___kstart64)
SECTIONS
{
    . = 0x00100000; /* 64bit kern location */
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
    .stack :
    {
      . = __bss_end;
      __stack_bottom = ALIGN(0x1000);
      __stack_top = __stack_bottom + 0x00100000;
    }
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
