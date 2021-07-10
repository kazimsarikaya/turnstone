OUTPUT_FORMAT(binary)
ENTRY(___kstart64)
SECTIONS
{
    . = 0x20000; /* 64bit kern location */

    .text :
    {
        LONG(0x0FBE9);
        FILL(0x0);
        . = 0x100;
        *(.text*);
    }

    .data :
    {
        *(.data*)
        *(.rodata*)
    }

    .padding :
    {
      BYTE(0xFF);
      . = ALIGN(0x200);
    } =0xFF

    .bss :
    {
      *(.bss*)
      *(COMMON)
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
