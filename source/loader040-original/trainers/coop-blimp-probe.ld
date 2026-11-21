ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) }
 ASSERT(. <= 8192, "Blimp probe code overflow")
 . = 8192; .data : { *(.probe.data) *(.data*) *(.bss*) }
 ASSERT(. <= 12288, "Blimp probe mailbox overflow")
 . = 12288; .constants : { KEEP(*(.probe.layout)) *(.rodata*) }
 ASSERT(. <= 32768, "Blimp probe image overflow")
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
