OUTPUT_FORMAT(binary)
ENTRY(___kstart64)
SECTIONS
{
    . = 0x20000; /* 64bit kern location */
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
      __kheap_top = 0;
      __kpagetable_p4 = 0;
      __kpagetable_p3 = 0;
      __kpagetable_p2 = 0;
      __bss_end = .;
    }
    .stack BLOCK(0x1000) :
    {
      __stack_bottom = .;
      . += 0x010000;
      __stack_top = .;
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
