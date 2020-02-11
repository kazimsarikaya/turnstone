OUTPUT_FORMAT(binary)
ENTRY(__kstart)
SECTIONS
{
    . = 0x0; /* same as last far jump at bootsect.asm */
    .text :
    {
        *(.text);
    }
    .data :
    {
        *(.data)
        *(.rodata)
        . = ALIGN(512);
    } =0x0
    .bss :
    {
      __bss_start = .;
      *(.bss)
      *(COMMON)
      __bss_end = .;
    }
    .stack BLOCK(0x1000) :
    {
      __stack_bottom = .;
      . += 0x1000;
      __stack_top = .;
    }
    .heap :
    {
      . = __stack_top +1;
      __kheap_bottom = ALIGN(0x100);
      __kheap_top = __kheap_bottom + 0x8000;
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
