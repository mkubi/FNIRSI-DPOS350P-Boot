OUTPUT_ARCH(arm)
ENTRY(_start)
MEMORY { lab : ORIGIN = 0x42E70000, LENGTH = 0x00010000 }
SECTIONS {
 . = ORIGIN(lab);
 .text : ALIGN(4) { KEEP(*(.text.start)) *(.text*) } > lab
 .rodata : ALIGN(4) { *(.rodata*) } > lab
 .data : ALIGN(4) { *(.data*) } > lab
 .bss (NOLOAD) : ALIGN(4) { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; } > lab
 __runtime_end = .;
 /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.attributes*) *(.ARM.exidx*) *(.ARM.extab*) }
 ASSERT(__runtime_end <= 0x42E7E000, "DPOSLAB code/data overlaps 8 KiB stack")
}
