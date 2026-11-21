ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) }
 ASSERT(. <= 0x2000, "Camera code overflow")
 . = 0x2000; .data : { *(.hold.data) *(.data*) *(.bss*) }
 ASSERT(. <= 0x3000, "Camera mailbox overflow")
 . = 0x3000; .rodata : { *(.rodata*) }
 ASSERT(. <= 0x4000, "Camera image overflow")
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
