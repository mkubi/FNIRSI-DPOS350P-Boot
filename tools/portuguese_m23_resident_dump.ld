ENTRY(portuguese_m23_resident_dump)

SECTIONS
{
    . = 0x40055AD4;

    .text : ALIGN(4)
    {
        *(.text*)
    }

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
        *(.ARM.attributes*)
    }
}
