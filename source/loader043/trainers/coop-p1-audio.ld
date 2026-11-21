ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) *(.rodata*) }
 ASSERT(. <= 8192, "Audio code overflow")
 . = 8192; .data : { *(.audio.data) }
 ASSERT(. <= 12288, "Audio mailbox overflow")
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
