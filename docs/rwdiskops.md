Reading/Writing to the Disk {#rwdiskops}
===========================

[TOC]

Reading/Writing to the disk performed differently at real and long mode. At real mode it uses BIOS interrupts, and long mode it uses pci hba access.

### BIOS interrupts

BIOS can support extended disk read and write with the interrupt **0x13**. For checking this support we use **ax** value **0x4100**. When the BIOS support extended r/w, then the **bx** value will be **0xAA55**. After that For reading we use **ax** value **0x4200** and for writing **0x4300**.

The interrupt uses a structure called as [DAP][DAP]. DAP address is located at **DS:SI**. And hard disk number is located at **dl**. If there is and error, **CF** is set, and **ah** contains error code.

### Over PCI

At long mode we cannot use BIOS interrupts. Hence we should scan [PCI][PCI] and find disk controller aka [HBA][HBA]. Then we send HBA commands to work with disks. PCI access in not implemented yet.

[DAP]: @ref disk_bios_dap
[PCI]: @ref pciops
[HBA]: @ref hbaops 
