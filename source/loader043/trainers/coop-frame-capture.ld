ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) }
  ASSERT(. <= 0x2000, "capture code overflow")
  . = 0x2000;
  .data (NOLOAD) : { KEEP(*(.capture.data)) *(.data*) *(.bss*) }
  ASSERT(. <= 0x2400, "capture mailbox overflow")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
