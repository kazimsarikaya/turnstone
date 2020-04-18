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
    .stack :
    {
      . = __bss_end +1;
      __stack_bottom = ALIGN(0x100);
      __stack_top = __stack_bottom + 0x1000;
    }
    .heap :
    {
      . = __stack_top +1;
      __kheap_bottom = ALIGN(0x100);
      __kheap_top = __kheap_bottom + 0x8000;
    }
    .pagetable :
    {
      . = __kheap_top +1;
      __kpagetable_p4 = ALIGN(0x1000);
      __kpagetable_p3 = __kpagetable_p4 + 0x1000;
      __kpagetable_p2 = __kpagetable_p3 + 0x1000;
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
