# Linking and Loading Programs

The TurnstoneOS stores all its data at a database called \ref tosdb. Currently it has a one database **system** and several tables as **config, modules, implementations, sections, symbols** and **relocations**. You can look their definitions at \ref utils/generatelinkerdb.c.

Compiling each c file needs several parameters to produce position indepented code with got and plt support. At boot **uefi** bootloader opens database and tables and links kernel entry point and dumps into the memory. You can look it at \ref efi/main.c and \ref cc/lib/linker.64.c.

## PLT and GOT

During linking each module will have its own PLT section for locating functions. For symbol resolving PLT goes to the **GOT** which is at 8TiB (virtual memmory). TOS as its own GOT rows. It is not like linux got. You can look it at \ref includes/linker.h. GOT also contains informations about each symbol. PLT has different PLT0 row for **kernel** and **VM** applications. All symbols are resolved at kernel, hence PLT0 is consist of **NOP**s. However for VM applications PLT0 has a **vmcall** interface and uses **0x1000** for load module. 

Layout of PLT and GOT is diffrenet because both gcc, ld and linux assumes several presumptions and optimizations. Hence **r15** register can be used diffrently, not for GOT. 

## Lazy Loading

When a function acccess performed at code section. It first jumps PLT and PLT checks if symbol resolved at GOT. If it is resolved then jumps to the address at GOT. In VM applications, if symbol is not resolved yet, code jumps into PLT0 and then a **vmcall** occurs. Task sends an ipc request to the **tosdb manager** task to load module. Tosdb Manager loads module into memory with not recursive linking and returns metadata to the VM task. VM task fills EPT and VM's page table. Then continues execution of program at VM. During page table building all sections are marked as readonly. When program tries to access the writable sections **.data, .datareloc** and **.bss** a page fault occurs. Then VM task creates clone of related section with writable properties. Hence each program will access its own writable data. You can look related code at \ref cc/hypervisor/hypervisor_ept.64.c.

Accessing global variables at modules before a function call causes a page fault. Because PLT only handles function (procedure) calls. Hence accessing global variables needs **getter/setter**.
