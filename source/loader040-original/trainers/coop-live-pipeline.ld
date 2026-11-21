ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { KEEP(*(.entry)) KEEP(*(.live.script)) KEEP(*(.live.flip)) *(.flipentry) *(.text*) *(.rodata*) }
  ASSERT(. <= 0x10000, "live pipeline code overflow")
  . = 0x10000;
  .capture (NOLOAD) : { KEEP(*(.capture.data)) }
  ASSERT(. <= 0x11000, "live capture mailbox overflow")
  . = 0x11000;
  .live (NOLOAD) : { KEEP(*(.live.data)) }
  ASSERT(. <= 0x12000, "live ownership mailbox overflow")
  . = 0x12000;
  .split (NOLOAD) : { KEEP(*(.split.data)) *(.data*) *(.bss*) }
  ASSERT(. <= 0x14000, "live output mailbox overflow")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
