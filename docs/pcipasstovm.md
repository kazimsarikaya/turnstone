# Passing PCI(e) cards to VM

As mentioned at @ref hypervisor, we can do whatever we wanna do with vms. It is all about imagination.

For PCI(e) access by VMs, several things should be performed:

1. Find about PCI(e) cards memory. Like its own memory and BAR's memory and ports. 
2. Map PCI(e) related memories into EPT with 4K frames with no caching. Also update VMCS's IO BITMAP A and B.
3. Create page table entries at VM.
4. Rediret interrupts.

After we created and start our vm, vm makes a **vmcall** for pci(e) mapping. **vmcall** causes a **vmexit** and a vm root-operations starts. The vm call passes address of pci(e) card as 32-bit integer as GGBBDDFF. GG is group number, BB is bus number, DD is device number and FF is function number. 

Host software searches given card. After find it, host software creates entries at ept and page tables for the vm. For details you can look **hypervisor_ept_map_pci_device procedure** at \ref cc/hypervisor/hypervisor_ept.64.c. Procedure also updates IO bitmaps at VMCS. After mapping an ept invalidation is required. When mappings are done, host software resumes (also returns vmcall status) the VM. The main important part is the mappings should be in 4Ks, otherwise it is an undefined stiuation (bad things can be happen).

After **vmcall** returns pci(e) cards guest virtual address to the guest. Guest can access pci(e) card with mmio.

Now the other big question: what about interrupts?

If VM configures and enables interrupts on mapped pci(e), all interrupts will be gone to host software, not VM. So VM should make a new **vmcall** for interrupt redirection. There is three types of interrups, **legacy (pin) based, msi** and **msix**. VM chooses one of this interrupt types and maybe a interupt number and makes the related vmcall. 

When host software receives vmcall, it should configure pci(e) card for interrupt. The job is very generic. For legacy interrupts, it is about pci(e) card's pin number and io-apic mapping. For msi and msix configuration should be done at pci capabilities. Host software may not have knowledge about device. It is all about pci structure. Host software maps interrupt into a generic isr. 

When interupt arives at generic isr, generic isr should search vm to find mapped vms with that interrupt. When one or more vms found, inform vm with a **interupt info** inside **interrupt window** vmexit. 

Interrupts on host causes vmexits at VMs, hence inside vmexit of **external interrupt** we can check host software pushes an interrupt for VM. If there is we should enable **interrupt window** for vm. When interrupt window enabled at VMCS, vmexit occurs with this type. Now we can create and set interrupt info at VMCS.

When an interrupt info setted at VMCS an interrupt triggered at VM. Hence vm created an entry at its own IDT before interrupt redirection vmcall, VM runs that isr on VM. VM should perform pci(e) interrupt related operations and makes a fake **apic eoi**. In Turnstone OS, we configure a x2apic emulation with msr writings. So a vmexit occures with msr writing. Host software checks there is any interupt waiting for VM. If there is re-enables interrupt window at VMCS, otherwise resumes VM.

After all this operations, The redirected interrupt consumed by the VM.

For code details there is a sample pci card known as **edu**, and the all pci(e) mapping stuff tested with it. The example vm is at \ref cc/programs/vmedu.64.c.
