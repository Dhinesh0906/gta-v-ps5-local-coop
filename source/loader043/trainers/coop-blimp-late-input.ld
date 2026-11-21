ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) *(.rodata*) }
 ASSERT(. <= 4096, "Blimp input code overflow")
 . = 4096; .data : { *(.blimp.data) }
 ASSERT(. <= 8192, "Blimp input state overflow")
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
