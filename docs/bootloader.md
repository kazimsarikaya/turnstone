Boot Loader  {#bootloader}
===========
[TOC]

The system can be booted from disk or network (PXE). The booting process will be different where the system is booted from.

### Disk Boot (MBR)

This boot technic is also known as MBR booting.

The BIOS loads first 512 bytes from disk to start the system. For MBR booting we have to files to generate bootloader code.

* bootsect16.asm
* bootsect.ld

#### bootsect16.asm

The file is the asm code inside MBR. The entry point is **__start**. When bios starts MBR code the ax value is 0xAA55. Fisrt we check this value. If not equals then we jump to **panic** label. It'is a simple code a infinitive loop that halts cpu.

The bios loads code at 0x7C00. And all segments are 0x0 However for addressing data, easily, we load all data segments, ds, es, ss, fs and gs, with 0x7C0. Also we setup a small stack. Stack top address is defined at linker script with **__stack_top**.

We store the hard disk number which is at **dl** register to a memory location labeled with **BOOT_DRIVE**.

We print an information message, Booting OS, with the interrupt **0x10** with **al** value **0x0A**.

Prevent disk errors we check disk number, if it is 0, then we print an error, and panic the system.

We need to check extended disk interrupt is available with interrupt **0x13** and **ax** value with **0x4100**. If **bx** contains the value **0xAA55**, then system supports extended disk interrupts, else we show error message and panic the system.

We build a **[DAP][DAP]** structure to load second sector which has the slot table. Currently we don't have any backup of slot table. We search a slot which has **stage 2** code. The slot's type is 3. For additional information for disks slots please read [disk slots][diskslots].

For slot definition we load **stage 2** into the address **0x1000**. We push the boot drive value to the stack for using at stage 2. Then we far jump stage 2 with changing cs segment.

#### bootsect.ld

The linker script should output in **binary** format. The entry point will be **__start**. We fix data segment addresses hence the alignment of linker script is **0x0** In internet all linker examples are aligned at **0x7C00**. We discard several sections to minimize our code. We insert MBR signature **0xAA55** at location **510**. We also define a stack section with size of 4K.

### Network Boot (PXE)

For booting with network/pxe we do some different things. The pxe loads our kernel some where in memory like bios. And starts the code at **512** aka second sector. So we don't need to read disk only we look slot table which is at same 512 bytes block started at offset **0x110**, then copy stage 2 to the **0x1000**. Finally far jump to the stage 2.

We also have to files for pxe boot:

* slottableprotect16.asm
* slottableprotect.ld

#### slottableprotect16.asm

The code is very simple. Fixes data segments and copies stage 2 code.

#### slottableprotect.ld

Very simple linker script same as **bootsect.ld**. The specific part is putting slot table at offset 0x110.

[DAP]: @ref rwdiskops
[diskslots]: @ref diskslots
