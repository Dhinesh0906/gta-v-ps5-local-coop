ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { KEEP(*(.entry)) KEEP(*(.compact.script)) KEEP(*(.compact.flip)) *(.text*) *(.rodata*) }
 ASSERT(. <= 0x10000, "compact renderer code overflow")
 . = 0x10000;
 .capture (NOLOAD) : { KEEP(*(.capture.data)) }
 ASSERT(. <= 0x11000, "compact capture mailbox overflow")
 . = 0x11000;
 .compact (NOLOAD) : { KEEP(*(.compact.data)) *(.data*) *(.bss*) }
 ASSERT(. <= 0x14000, "compact control overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
