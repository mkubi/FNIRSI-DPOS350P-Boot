/*
 * Phase 0: linked at 0x41000000 (FEL, above original app at 0x40000000)
 * Phase 5: will need relinking for SRAM entry + DRAM handoff
 */
ENTRY(_start)

SECTIONS
{
    . = 0x41000000;

    .text : {
        arch/r11/start.o(.text.start)
        *(.text*)
    }

    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
}
