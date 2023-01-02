Boot Loader  {#bootloader}
===========
[TOC]

The system can be booted from disk or over network (**PXE**). The booting process will be managed by UEFI BIOS. Disk or network boot has a small difference how to load **Turnstone OS** kernel into memory and execute it. 

If system is booted from disk, disk should have a **GPT** partition map, and a minimum 2 partitions. First partitiion is **ESP** partition which is a **FAT32** formattted. Second partition is a raw partition and its content is only Turnstone OS kernel. For simplicity Turnstone OS has an utility which creates a sparse file with size 1GiB and create gpt partition map and required partitions. Formats first partition as FAT32 and creates required directories and puts **EFI Application** inside it. And also create kernel partition and puts kernel contents into that partition. 

If system is booted from network with pxe, the EFI application is loaded by PXE firmaware. It's the same EFI application as disk boot. 

The EFI application detects how itself loaded into the memory, such as disk or network. Then it loads kernel from disk partition or network via tftp client.

Kernel is compressed with \@ref zpack. EFI application decompress kernel and puts kenrel at physical address starting with 2mib. Then relinks kernel for that positition with identity mapping.
