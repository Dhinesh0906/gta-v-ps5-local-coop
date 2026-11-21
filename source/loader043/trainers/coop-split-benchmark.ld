ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) }
  ASSERT(. <= 0x1000, "split benchmark code exceeds one page")
  . = 0x1000;
  .data (NOLOAD) : { KEEP(*(.probe.data)) *(.data*) *(.bss*) }
  ASSERT(. <= 0x2000, "split benchmark mailbox overflow")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
