ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) *(.rodata*) }
 ASSERT(. <= 4096, "Bike engine recorder code overflow")
 . = 4096; .data : { *(.flight.data) *(.data*) *(.bss*) }
 ASSERT(. <= 8192, "Bike engine recorder data overflow")
 . = 8192; .trace : { *(.flight.trace) }
 ASSERT(. <= 12288, "Bike engine recorder trace overflow")
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
