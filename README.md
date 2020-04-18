*** OS DEV ***

This is a custom os project.
The aim of the project is create a custom hypervisor operating system.

Stages of this project are:

1. ****Stage1****: Custom boot loader:

   Load several sectors to the memory at 0x100 (first free memory), and jump to that program.

2. ****Stage2****: Detect memory, prepare long mode and jump long it.

   Enable A20 line. Memory detection can be performed by bios interrupt, hence real mode operations needed before long mode. Simple memory manager to load 64bit stage3 from disk. PIO disk implentation. Prepare simple GDT empty IDT and 1 Hugepage page table entry (0-2MiB). Then jump long mode. long mode stage3 is at 0x20000

3. ****Stage3****: Minimal 64 bit kernel. Prepares real GDT, IDT etc. Disk access with PIO. Prepares Page tables

4. ****Stage4****: Memory Management

   TODO

5. ****Stage5****: Scheduler

   TODO

6. ****Stage6****: I/O (disk and network)

   TODO

7. ****Stage7****: Hypervisor

   TODO
