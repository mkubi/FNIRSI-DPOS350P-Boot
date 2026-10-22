OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

STAGE1_RESULT_ADDR = 0x00009F80;
STAGE1_RESULT_SIZE = 0x0000007C;

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

    .rodata : ALIGN(4) { *(.rodata*) } > ram
    .data : ALIGN(4) { *(.data*) } > ram

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

    ASSERT(__image_end <= STAGE1_RESULT_ADDR,
           "Stage 1B.1 payload overlaps the result block")
    ASSERT((STAGE1_RESULT_ADDR + STAGE1_RESULT_SIZE) <= 0x0000A000,
           "Stage 1B.1 result block exceeds the XFEL SRAM window")
}
