Boot Loader  {#bootloader}
===========
[TOC]

## General Booting

The system can be booted from disk or over network (**PXE**). The booting process will be managed by UEFI BIOS. Disk or network boot has a small difference how to load **Turnstone OS** kernel into memory and execute it. 

If system is booted from disk, disk should have a **GPT** partition map, and a minimum 2 partitions. First partitiion is **ESP** partition which is a **FAT32** formattted. Second partition is a raw partition and its content is only Turnstone OS kernel. For simplicity Turnstone OS has an utility which creates a sparse file with size 1GiB and create gpt partition map and required partitions. Formats first partition as FAT32 and creates required directories and puts **EFI Application** inside it. And also create kernel partition and puts kernel contents into that partition. 

If system is booted from network with pxe, the EFI application is loaded by PXE firmaware. It's the same EFI application as disk boot. 

The EFI application detects how itself loaded into the memory, such as disk or network. Then it loads kernel from disk partition or network via tftp client.

Kernel is compressed with @ref zpack. EFI application decompress kernel and puts kenrel at physical address starting with 2mib. Then relinks kernel for that positition with identity mapping.


## Deep Dive

An EFI application is a PE/COFF+ application with subsystem 10. Yes it is a windows application. For additional information please look @ref efi_internals.

An (U)EFI bios loads EFI applications into the memory and jumps its main method. Let's eximine how UEFI bios finds the EFI application in details.

For boot over network, it is very simple. Network Card's firmware loads EFI application with help of dhcp. Some bioses support HTTP protocol other than TFTP. However most times, only TFTP is used because of availability. For boot over disk it is somehow complicated. The disk should have GPT label, for more specific to EFI internals disk should be EFI table, which is a GPT table, like ACPI tables (in reality every ACPI table is a EFI table). A GPT table can contain 128 partition entries, general all entries created with unused status. And filled some of them. UEFI bios looks for a special partition known as EFI System Partition (ESP), which as a special GUID. Yes EFI loves guids. Every thing is indentified with guids. 

ESP should be formated as FAT, generally FAT32, but it is not neccessary, may be FAT16 or FAT12. Turnstone OS uses FAT32. Because more complexity more problems. For more information about FAT32 you can look @ref fs_fat32. Without other bios configurations, UEFI bios loads efi application at

```
EFI\BOOT\BOOTX64.EFI
```

inside ESP. When EFI application loaded, bios runs it. 

Now, we are at out Tunstone OS EFI application. What will we do? Load kernel and leave EFI application and jump our kernel. Easy? Somehow. Firstly we need to initialize a heap, because we need memory management. Please look @ref memorymanagement_heap for heap details. Then find how EFI application loaded. Because we need to find kernel location. Is it at network or disk? However Mostly bioses loves bad jokes. BootServices' handle protocol loves saying incorrect information. So we need to search how EFI application loaded. The function

```
efi_status_t efi_is_pxe_boot(boolean_t* result);
```

handles this. This function looks EFI global variable BootCurrent. This variables gives the current boot order with format BootXXXX. Then we look for that variable's content. This variable's content is a EFI path. Traverse path and search messaging path type and mac messaging sub type. If we found MAC in the path, that means EFI application booted over network with PXE, otherwise from disk. 

