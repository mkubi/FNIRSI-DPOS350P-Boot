ENTRY(_start)
SECTIONS
{
    . = 0x40000000;
    .text : ALIGN(4) {
        KEEP(*(.text.start))
        *(.text*)
    }
    .rodata : ALIGN(4) { *(.rodata*) }
    .data : ALIGN(4) { *(.data*) }
    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    }
    __image_end = .;
    /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.attributes*) }
    ASSERT(__image_end < 0x40100000,
           "SD-NAND mapper exceeds 1 MiB application window")
}
