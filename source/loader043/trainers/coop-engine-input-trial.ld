ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) }
 ASSERT(. <= 0x2000, "input trial code overflow")
 . = 0x2000;
 .data (NOLOAD) : { KEEP(*(.probe.data)) *(.data*) *(.bss*) }
 ASSERT(. <= 0x4000, "input trial mailbox overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
