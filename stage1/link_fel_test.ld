OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    ram : ORIGIN = 0x00008000, LENGTH = 0x00002000
}

SECTIONS
{
    .text : ALIGN(4) {
        __image_start = .;
        KEEP(*(.text.start))
        *(.text*)
    } > ram

    .rodata : ALIGN(4) {
        *(.rodata*)
    } > ram

    .data : ALIGN(4) {
        *(.data*)
    } > ram

    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ram

    __image_end = .;

    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.ARM.attributes*)
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }

    ASSERT(__image_end <= 0x00009C00,
           "FEL recovery test leaves insufficient SRAM stack space")
}
