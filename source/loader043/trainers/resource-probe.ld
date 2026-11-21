ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { *(.entry) *(.text*) *(.rodata*) }
  ASSERT(. <= 0x1000, "Resource probe code exceeds its page")
  . = 0x1000;
  .data : { *(.probe.data) *(.data*) *(.bss*) }
  ASSERT(. <= 0x2000, "Resource probe data exceeds its page")
  /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
