.syntax unified
.cpu cortex-a7
.arm

.section .rodata.usb_linux_blob, "a", %progbits
.balign 4
.global usb_linux_blob_start
.global usb_linux_blob_end

usb_linux_blob_start:
    .incbin "build/apps/usb_linux/USB_LINUX.bin"
usb_linux_blob_end:
    .balign 4
