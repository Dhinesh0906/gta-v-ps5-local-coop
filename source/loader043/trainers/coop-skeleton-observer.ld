ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { *(.entry) *(.text*) }
  ASSERT(. <= 0x1000, "Observer code exceeds reserved space")
  . = 0x1000;
  .data : { *(.observer.data) *(.data*) *(.bss*) }
  ASSERT(. <= 0x2000, "Observer data exceeds reserved space")
  /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
