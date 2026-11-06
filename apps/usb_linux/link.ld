OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

USB_LINUX_BASE = 0x42E70000;
USB_LINUX_SIZE = 0x00010000;
USB_LINUX_END  = USB_LINUX_BASE + USB_LINUX_SIZE;

MEMORY
{
    ram (rwx) : ORIGIN = USB_LINUX_BASE, LENGTH = USB_LINUX_SIZE
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

    ASSERT(__image_start == USB_LINUX_BASE,
           "USB Linux image must start at 0x42E70000")

    ASSERT(__image_end <= USB_LINUX_END,
           "USB Linux resident image exceeds 64 KiB window")
}
