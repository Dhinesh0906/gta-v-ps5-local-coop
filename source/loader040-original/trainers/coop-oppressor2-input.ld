ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) }
 ASSERT(. <= 4096, "Oppressor input code overflow")
 . = 4096; .data : { *(.flight.data) *(.data*) *(.bss*) }
 ASSERT(. <= 8192, "Oppressor input data overflow")
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
