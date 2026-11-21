/* Preserve the existing mailbox ABI, place contextual code afterward. (AI-assisted) */
ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) }
 ASSERT(. <= 0x2000, "Camera entry code overflow")
 . = 0x2000; .data : { *(.hold.data) *(.data*) *(.bss*) }
 ASSERT(. <= 0x3000, "Camera mailbox overflow")
 . = 0x3000; .rodata : { *(.rodata*) }
 ASSERT(. <= 0x3f00, "Camera constants overflow")
 . = 0x3f00; .layout : { KEEP(*(.aware.layout)) }
 ASSERT(. <= 0x4000, "Camera layout overflow")
 . = 0x4000; .awaretext : { *(.aware.text*) }
 ASSERT(. <= 0x8000, "Context camera code overflow")
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
