ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { KEEP(*(.entry)) KEEP(*(.flipentry)) *(.text*) *(.rodata*) }
  ASSERT(. <= 0x4000, "split present code overflow")
  . = 0x4000;
  .data (NOLOAD) : { KEEP(*(.split.data)) *(.data*) *(.bss*) }
  ASSERT(. <= 0x8000, "split present mailbox overflow")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
