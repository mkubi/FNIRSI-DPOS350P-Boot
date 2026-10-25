ENTRY(portuguese_mmu_probe)

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
