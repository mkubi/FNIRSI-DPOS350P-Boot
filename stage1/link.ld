ENTRY(_start)

STAGE1_SCRATCHPAD = 0x00007E00;
STAGE1_RESULT     = 0x00007F00;

SECTIONS
{
    . = STAGE1_SCRATCHPAD;
    __image_start = .;

    .text : ALIGN(4) {
        KEEP(*(.text.start))
        *(.text*)
        *(.rodata*)
        *(.data*)
    }

    __image_end = .;

    /DISCARD/ : {
        *(.bss*)
        *(COMMON)
        *(.comment*)
        *(.note*)
        *(.ARM.attributes*)
    }

    ASSERT(__image_end <= STAGE1_RESULT,
           "Stage 1A payload overlaps the result block at 0x00007F00")
}
