Disk Slots {#diskslots}
==========

[TOC]

### Slots

Like partitions or more likely solaris slices, the disk divided into slots. A slot may contain executable, data, database or sub-slots. For slot types please refer to @ref disk_slot_type. Each slot is defined by three quad words: type, start and end lba addresses. For slot type please referer to @ref disk_slot.

### Slot Table

From starting the byte **0x110** at the second 512 bytes sector of disk or memory, there is a slot table. It contains **10** entries. The slot table is refered by the struct @ref disk_slot_table. Slot table is used by MBR and PXE boots with assembly, and c code @ref disk_read_slottable.
