OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

BT0_MAX_SIZE = 0x00006800;
STAGE1_RESULT_ADDR = 0x00009F70;
STAGE1_RESULT_SIZE = 0x00000090;

MEMORY
{
    sram : ORIGIN = 0x00000000, LENGTH = BT0_MAX_SIZE
}

SECTIONS
{
    .text : ALIGN(4) {
        __image_start = .;
        KEEP(*(.text.bt0_header))
        *(.text*)
    } > sram

    .rodata : ALIGN(4) { *(.rodata*) } > sram
    .data   : ALIGN(4) { *(.data*) } > sram

    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss*) *(.sbss*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > sram

    __image_end = .;

    /DISCARD/ : {
        *(.comment*) *(.note*) *(.ARM.attributes*)
        *(.ARM.exidx*) *(.ARM.extab*)
    }

    ASSERT(__image_start == 0x00000000,
           "BT0 image must start at SRAM address zero")
    ASSERT(__image_end <= BT0_MAX_SIZE,
           "Stage 1C exceeds original 0x6800-byte BT0 region")
    ASSERT((STAGE1_RESULT_ADDR + STAGE1_RESULT_SIZE) <= 0x0000A000,
           "Stage 1 result block exceeds SRAM diagnostic window")
}
