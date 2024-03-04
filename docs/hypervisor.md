# Hypervisor

Turnstone OS has internal hypervisor which uses [VMX].

**VMX** requires two main structures which 4096 bytes. One is for **VMON** and other is for **VMCS**. VMON structure intialized for every cpu core. For each vm we should initilize VMCS structure. This structure only can be editted or read by 

```
vmwrite
``` 

and 

```
vmread
```

Each field of VMCS has key. You can find keys at \ref hypervisor_macros.h

## Lifecycle of a VM

VM lifecycle is like that:
1. Create VMCS 
2. Load VMCS 
3. VMLAUNCH 
4. VM exits with some reason 
5. VMRESUME or VMLAUNCH
6. Repeats with step 4

Firstly step 5 is tricky. *vmlaunch* and *vmresume* performs same event with a little difference. If **VMCS** structure is dirty, we need *vmresume*, otherwise we do *vmlaunch* At step 5 we firstly perform vmresume, if it fails we check the reason. If reason code is **5** which means *vmlaunch* required, then we perform a vmlaunch. If vmlaunch is also failed, we kill vm task. 

Let's return step 1. The most difficult operation and most important one. Creating and filling VMCS structure. This structure has 3 main parts.

## VMCS Structure

1. Host state 
2. Guest state 
3. Control values

Host state is the most simple one. When vmexit occurs, it is about how will be cpu's several registers. We fill control registers, segment values, gdt, idt and tss values. Also when vmexit occurs, a new code will be called with given address and stack pointer. vmlaunch and vmresume cancels this method's execution and if a new vm exit occurs, the method called as new (rip and rsp always same as vmcs structure.). 

Guest state is very interesting than host state. We can do two diffrent things during filling. We can create a new booted machine, or we can run a program inside vm. It will be detailed at control values however we can give some details. VMX uses [EPT] which is likely a page table with small diffrences. With EPT, we map a physical memory into VMCS. So this area can be all or partial physical memory with identical mapping and guest cr3 register can be same as physical (real) cr3 value. Also if we build EPT cleverly and create page tables, we can also directly start our VM in long mode with empty memory.

The main difficulty of system emulations depends on virtualization of hardware. When a vm starts also there is no bios (legacy or uefi). We should write virtual hardware code and PIO and MMIO emulations. It is not difficult. We can create EPT misconfigurations (which will be explained detailed) and on vmexit we can handle this misconfigurations for emulating virtual hardwares. What we can do is releated only with our imaginations. 

Now let's talk about control values. Generally this values about how vmexit occurs, and memory. We fill io bitmap, which events creates vmexit, create and set EPT and also set VCPU id which is really helpfull for TLB. Also control values needs several 4096 bytes areas. 2 area for io bitmap and each for vm entry and vm exit msrs.

We can fill this values with help of physical cpu's properties. We can not set or clean each field's bits. There are several msrs to verify values. If we don't obey them, vmlauch/vmresume/vmptrld will failed with error code 7. 

## EPT 

Like a page table structure, but maps vm memory regions into physical ram. It can have holes or misconfigurated regions for MMIO. Also EPT can be lazy created. All about imaginations. Espacially about not consume physical memory. If the code which is running inside VM never access a memory region, why we should allocate it on physical ram? There is two error of EPT misconfiguration and violation. Some situations they are real problems. However we can do this errors intentionally and we can handle and resume our vm. 

However playing over EPT should be performed carefully. We can expose a physical memory which is used by something can be corrupted by vm.

## IPC between host and guest or PIO/MMIO emulation

Virtual hardware is a kind of IPC between host and guest. Let's go over an example.

### VM's video output

In our operating system we write into GPU's framebuffer or use FIFO queue. However vm does not have real gpu. We can easly support a special memory region as a virtual framebuffer. We can create an hole at EPT of vm for this region. When the code inside vm access this memory (mov or movsx instructions), at vm exit with EPT misconfiguration or violation we examine this instruction (some deassembly but it is easy). We handle mov/movsx instruction that should it do (read/write). Framebuffer's generally does writes. We gather the value of mov instruction (may be from register or may be memory of vm (look ept and find real physical address)), and do whatever with this value (may send real gpu to draw at screen). After that set vm's rip value (reading and writnig guest rip value at vmcs).

### PIO

Not only memory can be intercepted. With help of io bitmap, we can access port reads and writes. Especially com (serial port), pit, pic and keyboard emulation can be done easily with this method. PIO creates a IO instruction reason for vmexits.

### Host2Guest

We can easly perform a vmwrite into VMCS that we have interrupt for vm. Let's assume we have a virtual keyboard attached to our vm. It is just a uint8_t array. Don't think so much. We perform a vmwrite to **interrupt info** field for a keyboard interrupt, or choose what number you wanna, your code will handle it, be relax. However checking vm's rflags or other interupt related things like virtual apic is good thing. However you can assume all your interrupts are NMI regardless of it interrupt number. Let's continue. When vm gets an interrupt it will do some PIO or MMIO to get data about that interrupt. Remember we discussed above. So it is easy. 

## Real OS emulation 

Oh my holly...

Now it is very difficult. If we want to run a linux or an other os inside our VM, we really need to implement several mandotary virtual hardware and software. Firstly we need a legacy or uefi bios. Don't worry, it is only a software do what you do during developing your operating system. But from otherside. Firstly two things are mandotary:

1. Reading from virtual disk
2. Memory map 

Think what you do at bootloading your os: read some data from your disk and ask memory map from bios. Then you jumpped your little kernel. So now you should provides these two. 

Now what your kernel needs? Access hardware. Everyone loves PCI, don't we? However before that we need to learn our computer which is provided by ACPI. Put ACPI tables, and some DSDT and SSDT into memory. Don't forget that their locations are predefined. Maybe you think the memory of VM is small, but don't forget you can do tricks with EPT. UEFI and ACPI is end of 4gib memory. But you can provide them with EPT while you have only small memory. MMIO areas are not real physical memories. Don't you release during handling PCI bars? After ACPI, DSDT and SSDT are completed, do PCI MMIO areas. Maybe you can do it PIO, however choose MMIO it is more easy. Don't forget PCI means hardware emulation. So you should write several emulated hardware which guest os needs. Most easy ones may be virtio devices. And then probably if you code very well you can run a real operating system on your hypervisor. 

## From TurnstoneOS Perspective

Our aim is not runnig an operating system inside VM. We focues on run TurnstoneOS's several parts inside a restricted area. The hypervisor will provide data exchange between host and guest.


[VMX]: https://en.wikipedia.org/wiki/X86_virtualization
[EPT]: https://en.wikipedia.org/wiki/Second_Level_Address_Translation#Extended_Page_Tables
