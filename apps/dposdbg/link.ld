OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(dposdbg_screenshot_hook)

MEMORY
{
    plugin : ORIGIN = 0x42E70000, LENGTH = 0x00010000
}

SECTIONS
{
    . = ORIGIN(plugin);
    __image_start = .;

    .api : ALIGN(4) {
        KEEP(*(.api))
    } > plugin

    .text : ALIGN(4) {
        *(.text.hook)
        *(.text*)
    } > plugin

    .rodata : ALIGN(4) { *(.rodata*) } > plugin
    .data : ALIGN(4) { *(.data*) } > plugin
    .trace : ALIGN(32) { KEEP(*(.trace)) } > plugin

    __image_end = .;
    __image_size = __image_end - __image_start;

    .bss (NOLOAD) : ALIGN(4) {
        *(.bss*)
        *(COMMON)
    } > plugin

    __runtime_end = .;

    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.ARM.attributes*)
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }

    ASSERT(__runtime_end <= ORIGIN(plugin) + LENGTH(plugin),
           "DPOSDBG exceeds reserved 64 KiB resident region")
}
