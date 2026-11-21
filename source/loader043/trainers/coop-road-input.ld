ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) *(.rodata*) }
 ASSERT(. <= 8192, "Road helper code overflow")
 . = 8192; .data : { *(.road.data) }
 ASSERT(. <= 12288, "Road mailbox overflow")
 . = 12288; .trace (NOLOAD) : { *(.road.trace) }
 ASSERT(. <= 16384, "Road trace overflow")
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
