OUTPUT_FORMAT(binary)
ENTRY(__kstart)
SECTIONS
{
    . = 0x0; /* same as last far jump at bootsect.asm */

    .text :
    {
        *(.text*);
    }

    .data :
    {
        *(.data*)
        *(.rodata*)
    } =0x0

    .padding :
    {
      BYTE(0xFF);
      . = ALIGN(0x200);
    } =0xFF

    .bss :
    {
      __bss_start = .;
      *(.bss*)
      *(COMMON)
      __bss_end = .;
    }

    .stack :
    {
      __stack_bottom = ALIGN(0x100);
      __stack_top = __stack_bottom + 0x1000;
    }

    .heap :
    {
      . = __stack_top +1;
      __kheap_bottom = ALIGN(0x100);
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
