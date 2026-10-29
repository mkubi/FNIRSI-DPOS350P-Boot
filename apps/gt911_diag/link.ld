OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    app : ORIGIN = 0x40000000, LENGTH = 0x01000000
}

SECTIONS
{
    . = ORIGIN(app);
    __image_start = .;

    .text : ALIGN(4) {
        KEEP(*(.text.start))
        *(.text*)
    } > app

    .rodata : ALIGN(4) { *(.rodata*) } > app
    .data : ALIGN(4) { *(.data*) } > app

    __image_end = .;
    __image_size = __image_end - __image_start;

    .bss (NOLOAD) : ALIGN(8) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > app

    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.ARM.attributes*)
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }

    ASSERT(__bss_end <= ORIGIN(app) + LENGTH(app),
           "standalone acquisition test exceeds application region")
}
