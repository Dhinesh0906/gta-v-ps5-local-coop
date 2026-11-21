ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { *(.entry) *(.text*) *(.rodata*) }
 ASSERT(. <= 0x1000,"Flight adapter code overflow")
 . = 0x1000;
 .data : { *(.flight.data) }
 ASSERT(. <= 0x2000,"Flight adapter mailbox overflow")
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
