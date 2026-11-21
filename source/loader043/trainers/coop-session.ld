ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { *(.entry) *(.text*) }
  ASSERT(. <= 0x8000, "Co-op session code exceeds reserved space")
  . = 0x8000;
  .data : { *(.coop.data) *(.data*) *(.bss*) }
  ASSERT(. <= 0x8400, "Co-op session mailbox exceeds reserved space")
  . = 0x8400;
  .controls : { *(.coop.controls) }
  ASSERT(. <= 0x8700, "Co-op controls overflow pose probe block")
  . = 0x8700;
  .pose : { *(.coop.pose) }
  ASSERT(. <= 0x8800, "Co-op pose probe overflow toggle block")
  . = 0x8800;
  .toggle : { *(.coop.toggle) }
  ASSERT(. <= 0x8900, "Co-op toggle data exceeds vehicle probe boundary")
  . = 0x8900;
  .vehicle_probe : { *(.coop.vehicle_probe) }
  ASSERT(. <= 0x8a00, "Co-op vehicle probe exceeds hover boundary")
  . = 0x8a00;
  .hover : { *(.coop.hover) }
  ASSERT(. <= 0x8b00, "Co-op hover mailbox exceeds plane boundary")
  . = 0x8a40;
  .oppressor2 : { *(.coop.oppressor2) }
  ASSERT(. <= 0x8b00, "Mk II ownership exceeds plane boundary")
  . = 0x8b00;
  .plane : { *(.coop.plane) }
  ASSERT(. <= 0x8c00, "Co-op plane mailbox exceeds aircraft extras")
  . = 0x8b80;
  .plane_runtime : { *(.coop.plane_runtime) }
  ASSERT(. <= 0x8c00, "Plane runtime exceeds aircraft extras")
  . = 0x8c00;
  .aircraft : { *(.coop.aircraft) }
  ASSERT(. <= 0x8d00, "Co-op aircraft mailbox exceeds plane weapons")
  . = 0x8d00;
  .plane_weapons : { *(.coop.plane_weapons) }
  ASSERT(. <= 0x8e00, "Co-op plane weapons exceed fixture boundary")
  . = 0x8dc0;
  .primary_payload_equipment : { *(.coop.primary_payload_equipment) }
  ASSERT(. <= 0x8e00, "Primary payload equipment exceeds fixture boundary")
  . = 0x8e00;
  .payload_probe : { *(.coop.payload_probe) }
  ASSERT(. <= 0x8f00, "Payload probe exceeds road runtime boundary")
  . = 0x8f00;
  .road : { *(.coop.road) }
  ASSERT(. <= 0x8f40, "Road runtime exceeds assist boundary")
  . = 0x8f40;
  .assists : { *(.coop.assists) }
  ASSERT(. <= 0x9000, "Player assists exceed reference boundary")
  . = 0x9000;
  .vehicle_reference : { *(.coop.vehicle.reference) }
  ASSERT(. <= 0xa000, "Co-op vehicle reference overflow")
  . = 0xa000;
  .constants : { *(.rodata*) }
  ASSERT(. <= 0xc000, "Co-op constants exceed extension boundary")
  . = 0xc000;
  .extensions : { *(.coop.extensions) }
  ASSERT(. <= 0x20000, "Co-op extensions exceed image")
  /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
