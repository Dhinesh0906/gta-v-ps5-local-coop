ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { *(.entry) *(.text*) }
  . = 0x10000;
  .data : { *(.mailbox) *(.data*) *(.bss*) }
  . = 0x20000;
  .ghost : { *(.ghost.text*) *(.rodata*) }
  /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
