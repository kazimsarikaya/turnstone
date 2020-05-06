Roadmap {#roadmap}
=======

[TOC]

This project has this roadmap:

###1. Booting Stage 2
* Booting from MBR <span style="color:green; font-weight:bold">COMPLETED</span>
* Booting from PXE <span style="color:orange; font-weight:bold">PARTIALLY COMPLETED</span>

For additional information please refer @ref bootloader

###2. Stage 2 and Booting Stage 3
* Enable **A20** line. <span style="color:green; font-weight:bold">COMPLETED</span>

  For additional information please refer @ref memorymanagement

* Write video output driver. <span style="color:green; font-weight:bold">COMPLETED</span>

  For additional information please refer @ref video.h

* Write main string functions. <span style="color:green; font-weight:bold">COMPLETED</span>

  For additional information please refer @ref strings.h

* Write simple heap implentation. <span style="color:green; font-weight:bold">COMPLETED</span>

  For additional information please refer @ref memory.h and @ref memorymanagement

* Write BIOS memory map detection code. <span style="color:green; font-weight:bold">COMPLETED</span>

  For additional information please refer @ref memory.h and @ref memorymanagement

* Write disk read methods to read slot table and stage 3 to the memory. <span style="color:green; font-weight:bold">COMPLETED</span>

  For additional information please refer @ref rwdiskops, @ref diskio.h and @ref diskio.16.c

* Write functions creating segments such as idt and gdt. <span style="color:orange; font-weight:bold">PARTIALLY COMPLETED</span>

  For additional information please refer @ref segments and @ref descriptor.h.

* Write functions for creating page table and loading them. <span style="color:orange; font-weight:bold">PARTIALLY COMPLETED</span>

  For additional information please refer @ref memorymanagement, @ref memory.h and @ref memory.xx.c.

* Switch to long mode and start 64 bit stage 3. <span style="color:green; font-weight:bold">COMPLETED</span>

###3. Stage 3 and Starting Kernel Tasks
* Write libraries for data types. <span style="color:orange; font-weight:bold">PARTIALLY COMPLETED</span>

  For additional information please refer @ref datatypes.

* Frame Allocation.  <span style="color:gray; font-weight:bold">NOT STARTED</span>

  For additional information please refer @ref memorymanagement.

* Parse ACPI tables, and get system configuration. <span style="color:red; font-weight:bold">NOT COMPLETED</span>
* Scan PCI hardware. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Write disk and network driver. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Write file system-a-like libraries. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Write linker for loading objects, create executables and run them. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Configure multitasking. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Create kernel tasks. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Setup and start Application Processors. <span style="color:gray; font-weight:bold">NOT STARTED</span>

###4. System Message Queue Server
* Implement MQS. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Disk I/O over MQS. <span style="color:gray; font-weight:bold">NOT STARTED</span>
* Network I/O over MQS. <span style="color:gray; font-weight:bold">NOT STARTED</span>
