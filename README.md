*** OS DEV ***

This is a custom os project.
The aim of the project is create a custom hypervisor operating system.

Stages of this project are:

1. ****Stage1****: Custom boot loader:

   Enable A20 line. Load several sectors to the memory at 0x100 (first free memory), and jump to that program.

2. ****Stage2****: Detect memory, prepare long mode and jump long it.

   Memory detection can be performed by bios interrupt, hence real mode operations needed before long mode. Prepare ISR, IRQ and long mode page tables. Then jump long mode.

3. ****Stage3****: Memory Management

   TODO

4. ****Stage4****: Scheduler

   TODO

4. ****Stage5****: I/O (disk and network)

   TODO

5. ****Stage6****: Hypervisor

   TODO
