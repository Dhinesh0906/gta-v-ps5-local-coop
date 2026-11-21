ENTRY(_start)
SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) *(.rodata*) }
 ASSERT(. <= 2048, "Late plane callback overflow")
 . = 2048; .engine : { *(.engine_entry) }
 ASSERT(. <= 4096, "Late plane engine entry overflow")
 . = 4096; .data : { *(.plane.data) }
 . = 4352; .pedals : { *(.plane.pedals) }
 ASSERT(. <= 8192,"Plane pedal lease overflow")
 . = 8192; .trace (NOLOAD) : { *(.plane.trace) }
 ASSERT(. <= 16384, "Late plane trace overflow")
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
