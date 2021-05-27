OUTPUT_FORMAT(binary)
ENTRY(__start)
SECTIONS
{
    . = 0x0;
    .text :
    {
        *(.text);
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
