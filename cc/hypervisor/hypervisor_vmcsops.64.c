/**
 * @file hypervisor_vmcsops.64.c
 * @brief Hypervisor VMCS operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_vmcsops.h>
#include <hypervisor/hypervisor_vmxops.h>
#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_macros.h>
#include <hypervisor/hypervisor_ept.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <cpu/task.h>
#include <logging.h>

MODULE("turnstone.hypervisor");

uint32_t hypervisor_vmcs_revision_id(void) {
    return cpu_read_msr(CPU_MSR_IA32_VMX_BASIC) & 0xffffffff;
}

void hypervisor_vmcs_exit_handler_error(void);

void hypervisor_vmcs_exit_handler_error(void) {
    PRINTLOG(HYPERVISOR, LOG_ERROR, "VMExit Handler Error");
}

static __attribute__((naked)) void hypervisor_exit_handler(void) {
    asm volatile (
        "pushq %rbp\n"
        "pushq %rsp\n"
        "pushq %rax\n"
        "pushq %rbx\n"
        "pushq %rcx\n"
        "pushq %rdx\n"
        "pushq %rsi\n"
        "pushq %rdi\n"
        "pushq %r15\n"
        "pushq %r14\n"
        "pushq %r13\n"
        "pushq %r12\n"
        "pushq %r11\n"
        "pushq %r10\n"
        "pushq %r9\n"
        "pushq %r8\n"
        "movq %cr2, %rax\n"
        "pushq % rax\n"
        "pushfq\n"
        "// call the vmexit handler written in C\n"
        "movq %rsp, %rdi\n"
        "lea 0x0(%rip), %rax\n"
        "movabs $_GLOBAL_OFFSET_TABLE_, %r15\n"
        "add %rax, %r15\n"
        "movabsq $hypervisor_vmcs_exit_handler_entry@GOT, %rax\n"
        "call *(%r15, %rax, 1)\n"
        "// Check return value may end vm task\n"
        "// Restore the RSP and guest non-vmcs processor state\n"
        "cmp $-1, %rax\n"
        "je ___vmexit_handler_entry_error\n"
        "movq %rax, %rsp\n"
        "popfq\n"
        "popq %rax\n"
        "movq %rax, %cr2\n"
        "popq %r8\n"
        "popq %r9\n"
        "popq %r10\n"
        "popq %r11\n"
        "popq %r12\n"
        "popq %r13\n"
        "popq %r14\n"
        "popq %r15\n"
        "popq %rdi\n"
        "popq %rsi\n"
        "popq %rdx\n"
        "popq %rcx\n"
        "popq %rbx\n"
        "popq %rax\n"
        "popq %rsp\n"
        "popq %rbp\n"
        "// resume the VM\n"
        "vmresume\n"
        "// FIXME: cleanup task\n"
        "___vmexit_handler_entry_error:\n"
        "lea 0x0(%rip), %rax\n"
        "movabs $_GLOBAL_OFFSET_TABLE_, %r15\n"
        "add %rax, %r15\n"
        "movabsq $hypervisor_vmcs_exit_handler_error@GOT, %rax\n"
        "call *(%r15, %rax, 1)\n"
        "add $0x80, %rsp\n"
        "popq %rsp\n"
        "sti\n"
        "leave\n"
        "ret\n");
}

_Static_assert(sizeof(vmcs_registers_t) == 0x90, "vmcs_registers_t size mismatch. Fix add rsp above");


int8_t hypervisor_vmcs_prepare_host_state(void) {
    uint64_t cr0 = cpu_read_cr0().bits;
    uint64_t cr3 = cpu_read_cr3();
    uint64_t cr4 = cpu_read_cr4().bits;
    uint64_t efer = cpu_read_msr(CPU_MSR_EFER);

    vmx_write(VMX_HOST_CR0, cr0);
    vmx_write(VMX_HOST_CR3, cr3);
    vmx_write(VMX_HOST_CR4, cr4);

    vmx_write(VMX_HOST_ES_SELECTOR, 0);
    vmx_write(VMX_HOST_CS_SELECTOR, KERNEL_CODE_SEG);
    vmx_write(VMX_HOST_SS_SELECTOR, KERNEL_DATA_SEG);
    vmx_write(VMX_HOST_DS_SELECTOR, 0);
    vmx_write(VMX_HOST_FS_SELECTOR, 0);
    vmx_write(VMX_HOST_GS_SELECTOR, 0);
    vmx_write(VMX_HOST_TR_SELECTOR, KERNEL_TSS_SEG);


    vmx_write(VMX_HOST_IA32_SYSENTER_CS, KERNEL_CODE_SEG);
    vmx_write(VMX_HOST_IA32_SYSENTER_ESP, 0x0);
    vmx_write(VMX_HOST_IA32_SYSENTER_EIP, 0x0);

    vmx_write(VMX_HOST_IDTR_BASE, IDT_REGISTER->base);
    vmx_write(VMX_HOST_GDTR_BASE, GDT_REGISTER->base);
    vmx_write(VMX_HOST_FS_BASE, cpu_read_fs_base());
    vmx_write(VMX_HOST_GS_BASE, cpu_read_gs_base());

    descriptor_gdt_t* gdts = (descriptor_gdt_t*)GDT_REGISTER->base;
    descriptor_tss_t* tss = (descriptor_tss_t*)&gdts[KERNEL_TSS_SEG / 8];

    uint64_t tss_base = tss->base_address2;
    tss_base <<= 24;
    tss_base |= tss->base_address1;


    vmx_write(VMX_HOST_TR_BASE, tss_base);


    vmx_write(VMX_HOST_RSP, hypervisor_create_stack(64 << 10));
    vmx_write(VMX_HOST_RIP, (uint64_t)hypervisor_exit_handler);
    vmx_write(VMX_HOST_EFER, efer);

    return 0;
}

int8_t hypervisor_vmcs_prepare_guest_state(void) {
    vmx_write(VMX_GUEST_ES_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_CS_SELECTOR, 0x100);
    vmx_write(VMX_GUEST_DS_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_FS_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_GS_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_SS_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_TR_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_LDTR_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_CS_BASE, 0x0);
    vmx_write(VMX_GUEST_DS_BASE, 0x0);
    vmx_write(VMX_GUEST_ES_BASE, 0x0);
    vmx_write(VMX_GUEST_FS_BASE, 0x0);
    vmx_write(VMX_GUEST_GS_BASE, 0x0);
    vmx_write(VMX_GUEST_SS_BASE, 0x0);
    vmx_write(VMX_GUEST_LDTR_BASE, 0x0);
    vmx_write(VMX_GUEST_IDTR_BASE, 0x0);
    vmx_write(VMX_GUEST_GDTR_BASE, 0x0);
    vmx_write(VMX_GUEST_TR_BASE, 0x0);
    vmx_write(VMX_GUEST_CS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_DS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_ES_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_FS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_GS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_SS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_LDTR_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_TR_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_GDTR_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_IDTR_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_CS_ACCESS_RIGHT, VMX_CODE_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_DS_ACCESS_RIGHT, VMX_DATA_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_ES_ACCESS_RIGHT, VMX_DATA_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_FS_ACCESS_RIGHT, VMX_DATA_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_GS_ACCESS_RIGHT, VMX_DATA_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_SS_ACCESS_RIGHT, VMX_DATA_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_LDTR_ACCESS_RIGHT, VMX_LDTR_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_TR_ACCESS_RIGHT, VMX_TR_ACCESS_RIGHT);
    vmx_write(VMX_GUEST_INTERRUPTIBILITY_STATE, 0x0);
    vmx_write(VMX_GUEST_ACTIVITY_STATE, 0x0);

    uint64_t cr0_fixed = cpu_read_msr(CPU_MSR_IA32_VMX_CR0_FIXED0);
    // cr0_fixed |= cpu_read_msr(CPU_MSR_IA32_VMX_CR0_FIXED1);
    cpu_reg_cr0_t cr0 = { .bits = cr0_fixed };
    cr0.fields.protection_enabled = 0;
    cr0.fields.paging = 0;

    vmx_write(VMX_GUEST_CR0, cr0.bits);

    vmx_write(VMX_GUEST_CR3, 0x0);

    uint64_t cr4_fixed = cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED0);
    // cr4_fixed |= cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED1);
    cpu_reg_cr4_t cr4 = { .bits = cr4_fixed };
    vmx_write(VMX_GUEST_CR4, cr4.bits);

    vmx_write(VMX_GUEST_DR7, 0x0);
    vmx_write(VMX_GUEST_RSP, 0x0);
    vmx_write(VMX_GUEST_RIP, 0x0); // guest cs:ip -> 0x100:0
    vmx_write(VMX_GUEST_RFLAGS, VMX_RFLAG_RESERVED);
    vmx_write(VMX_GUEST_VMCS_LINK_POINTER_LOW, 0xffffffff);
    vmx_write(VMX_GUEST_VMCS_LINK_POINTER_HIGH, 0xffffffff);
    vmx_write(VMX_GUEST_IA32_EFER, 0x0);

    return 0;
}



int8_t hypervisor_vmcs_prepare_pinbased_control(void){
    uint32_t pinbased_msr_eax, pinbased_msr_edx;
    uint64_t pinbased_msr = cpu_read_msr(CPU_MSR_IA32_VMX_PINBASED_CTLS);
    pinbased_msr_eax = pinbased_msr & 0xffffffff;
    pinbased_msr_edx = pinbased_msr >> 32;

    uint32_t pinbased_vm_execution_ctrl = 0;
    // External Interrupt causes a VM EXIT
    pinbased_vm_execution_ctrl |= 1;
    pinbased_vm_execution_ctrl = vmx_fix_reserved_1_bits(pinbased_vm_execution_ctrl,
                                                         pinbased_msr_eax);
    pinbased_vm_execution_ctrl = vmx_fix_reserved_0_bits(pinbased_vm_execution_ctrl,
                                                         pinbased_msr_edx);
    vmx_write(VMX_CTLS_PIN_BASED_VM_EXECUTION, pinbased_vm_execution_ctrl);

    return 0;
}

int8_t hypervisor_vmcs_prepare_procbased_control(void) {
    // uint32_t vpid_and_ept_msr_eax, vpid_and_ept_msr_edx;
    // uint64_t vpid_and_ept_msr = cpu_read_msr(CPU_MSR_IA32_VMX_EPT_VPID_CAP);
    // vpid_and_ept_msr_eax = vpid_and_ept_msr & 0xffffffff;
    // vpid_and_ept_msr_edx = vpid_and_ept_msr >> 32;

    uint32_t pri_procbased_msr_eax, pri_procbased_msr_edx;
    uint64_t pri_procbased_msr = cpu_read_msr(CPU_MSR_IA32_VMX_PRI_PROCBASED_CTLS);
    pri_procbased_msr_eax = pri_procbased_msr & 0xffffffff;
    pri_procbased_msr_edx = pri_procbased_msr >> 32;

    uint32_t sec_procbased_msr_eax, sec_procbased_msr_edx;
    uint64_t sec_procbased_msr = cpu_read_msr(CPU_MSR_IA32_VMX_SEC_PROCBASED_CTLS);
    sec_procbased_msr_eax = sec_procbased_msr & 0xffffffff;
    sec_procbased_msr_edx = sec_procbased_msr >> 32;


    uint32_t pri_procbase_ctls = 0;
    pri_procbase_ctls |= 1 << 2; // enable interrupt-window exit
    pri_procbase_ctls |= 1 << 7; // Hlt causes vm exit
    pri_procbase_ctls |= 1 << 9; // INVLPG causes vm exit
    pri_procbase_ctls |= 1 << 12; // RDTSC exiting
    pri_procbase_ctls |= 1 << 15; // CR3-load causes vm exit
    pri_procbase_ctls |= 1 << 16; // CR3-store causes vm exit
    pri_procbase_ctls |= 1 << 24; // Unconditional IO exiting
    pri_procbase_ctls |= 1 << 25; // Use IO bitmap
    pri_procbase_ctls |= 1 << 30; // PAUSE causes vm exit
    pri_procbase_ctls |= 1 << 31; // activate secondary controls
    pri_procbase_ctls = vmx_fix_reserved_1_bits(pri_procbase_ctls,
                                                pri_procbased_msr_eax);
    pri_procbase_ctls = vmx_fix_reserved_0_bits(pri_procbase_ctls,
                                                pri_procbased_msr_edx);
    vmx_write(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION, pri_procbase_ctls);

    uint32_t sec_procbase_ctls = 0;
    sec_procbase_ctls |= 1 << 1; // use EPT
    sec_procbase_ctls |= 1 << 2; // descriptor-table exiting:GDT/LDT/IDT/TR
    sec_procbase_ctls |= 1 << 5; // enable VPID
    sec_procbase_ctls |= 1 << 7; // unrestricted guest
    sec_procbase_ctls = vmx_fix_reserved_1_bits(sec_procbase_ctls,
                                                sec_procbased_msr_eax);
    sec_procbase_ctls = vmx_fix_reserved_0_bits(sec_procbase_ctls,
                                                sec_procbased_msr_edx);

    vmx_write(VMX_CTLS_SEC_PROC_BASED_VM_EXECUTION, sec_procbase_ctls);

    vmx_write(VMX_CTLS_EXCEPTION_BITMAP, 0);
    vmx_write(VMX_CTLS_CR3_TARGET_COUNT, 0x0);

    return 0;
}

void hypervisor_io_bitmap_set_port(uint8_t * bitmap, uint16_t port) {
    uint16_t byte_index = port >> 3;
    uint8_t bit_index = port & 0x7;
    bitmap[byte_index] |= 1 << bit_index;
}


int8_t hypervisor_vmcs_prepare_io_bitmap(void) {
    frame_t* io_bitmap_frame = NULL;

    uint8_t * io_bitmap_region_va = (uint8_t*)hypervisor_allocate_region(&io_bitmap_frame, 0x2000);
    // Enable serial ports
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x3f8);
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x3f9);
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x3fa);
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x3fb);
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x3fc);
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x3fd);
    // Enable keyboard ports
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x60);
    hypervisor_io_bitmap_set_port(io_bitmap_region_va, 0x64);


    uint64_t io_bitmap_region_pa = io_bitmap_frame->frame_address;

    vmx_write(VMX_CTLS_IO_BITMAP_A, io_bitmap_region_pa);
    vmx_write(VMX_CTLS_IO_BITMAP_B, io_bitmap_region_pa + FRAME_SIZE);

    return 0;
}


int8_t hypervisor_vmcs_prepare_execution_control(void) {
    hypervisor_vmcs_prepare_pinbased_control();
    hypervisor_vmcs_prepare_io_bitmap();
    hypervisor_vmcs_prepare_procbased_control();

    return 0;
}

int8_t hypervisor_vmcs_prepare_vm_exit_and_entry_control(void) {
    uint32_t vm_exit_msr_eax, vm_exit_msr_edx;
    uint64_t vm_exit_msr = cpu_read_msr(CPU_MSR_IA32_VMX_VM_EXIT_CTLS);
    vm_exit_msr_eax = vm_exit_msr & 0xffffffff;
    vm_exit_msr_edx = vm_exit_msr >> 32;

    uint32_t vm_exit_ctls = 0;
    vm_exit_ctls |= 1 << 9; // VM exit to 64-bit long mode.
    vm_exit_ctls |= 1 << 15; // ACK external interrupts.
    vm_exit_ctls |= 1 << 21; // Load IA32_EFER on vm-exit
    vm_exit_ctls = vmx_fix_reserved_1_bits(vm_exit_ctls, vm_exit_msr_eax);
    vm_exit_ctls = vmx_fix_reserved_0_bits(vm_exit_ctls, vm_exit_msr_edx);

    vmx_write(VMX_CTLS_VM_EXIT, vm_exit_ctls);

    uint32_t predefined_msrs[0]; // = {IA32_EFER_MSR};
    uint32_t nr_msrs = sizeof(predefined_msrs) / sizeof(uint32_t);
    uint32_t vm_exit_load_msr_count = nr_msrs;
    uint32_t vm_exit_store_msr_count = nr_msrs;

    frame_t* vm_exit_load_msr_region = NULL;
    frame_t* vm_exit_store_msr_region = NULL;

    uint64_t vm_exit_load_msr_region_va = hypervisor_allocate_region(&vm_exit_load_msr_region, FRAME_SIZE);
    uint64_t vm_exit_store_msr_region_va = hypervisor_allocate_region(&vm_exit_store_msr_region, FRAME_SIZE);

    for (uint32_t index = 0; index < nr_msrs; index++) {
        vmcs_msr_blob_t * vmcs_msr = ((vmcs_msr_blob_t *)vm_exit_store_msr_region_va) + index;
        vmcs_msr->index = predefined_msrs[index];
        vmcs_msr->reserved = 0;
        uint64_t vmcs_msr_value = cpu_read_msr(vmcs_msr->index);
        vmcs_msr->msr_eax = vmcs_msr_value & 0xffffffff;
        vmcs_msr->msr_edx = vmcs_msr_value >> 32;

        vmcs_msr = ((vmcs_msr_blob_t *)vm_exit_load_msr_region_va) + index;
        vmcs_msr->index = predefined_msrs[index];
        vmcs_msr->reserved = 0;
        vmcs_msr_value = cpu_read_msr(vmcs_msr->index);
        vmcs_msr->msr_eax = vmcs_msr_value & 0xffffffff;
        vmcs_msr->msr_edx = vmcs_msr_value >> 32;
    }

    vmx_write(VMX_CTLS_VM_EXIT_MSR_STORE_COUNT, vm_exit_store_msr_count);
    vmx_write(VMX_CTLS_VM_EXIT_MSR_LOAD_COUNT, vm_exit_load_msr_count);
    vmx_write(VMX_CTLS_VM_EXIT_MSR_LOAD, vm_exit_load_msr_region->frame_address);
    vmx_write(VMX_CTLS_VM_EXIT_MSR_STORE, vm_exit_store_msr_region->frame_address);


    uint32_t vm_entry_msr_eax, vm_entry_msr_edx;
    uint64_t vm_entry_msr = cpu_read_msr(CPU_MSR_IA32_VMX_VM_ENTRY_CTLS);
    vm_entry_msr_eax = vm_entry_msr & 0xffffffff;
    vm_entry_msr_edx = vm_entry_msr >> 32;
    uint32_t vm_entry_ctls = 0;

    vm_entry_ctls |= 1 << 15; // load EFER msr on vm-entry
    vm_entry_ctls = vmx_fix_reserved_1_bits(vm_entry_ctls, vm_entry_msr_eax);
    vm_entry_ctls = vmx_fix_reserved_0_bits(vm_entry_ctls, vm_entry_msr_edx);

    vmx_write(VMX_CTLS_VM_ENTRY, vm_entry_ctls);
    vmx_write(VMX_CTLS_VM_ENTRY_MSR_LOAD_COUNT, vm_exit_store_msr_count);
    vmx_write(VMX_CTLS_VM_ENTRY_MSR_LOAD, vm_exit_store_msr_region->frame_address);
    vmx_write(VMX_CTLS_VM_ENTRY_INTERRUPT_INFORMATION_FIELD, 0x0);

    return 0;
}


int8_t hypervisor_vmcs_prepare_ept(void) {
    uint64_t ept_pml4_base = hypervisor_ept_setup(0, 16 << 20);

    uint64_t vpid_cap = cpu_read_msr(CPU_MSR_IA32_VMX_EPT_VPID_CAP);
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "VPID_CAP:0x%llx", vpid_cap);

    uint64_t eptp = ept_pml4_base;

    if (vpid_cap & (1 << 6)) { // bit 6 of the VPID_EPT VMX CAP is for page-walk length
        eptp |= (4 - 1) << 3; // query the bit 6 of the VPID_EPT VMX CAP
    }

    if (vpid_cap & (1 << 14)) { // bit 8 of the VPID_EPT VMX CAP is for write-back memory type
        eptp |= 6; // query the bit 8 of the VPID_EPT VMX CAP
    }

    if (vpid_cap & (1 << 21)) { // bit 21 of the VPID_EPT VMX CAP is for dirty and accessed marking
        eptp |= 1 << 6; // enable acccessed and dirty marking
    }

    vmx_write(VMX_CTLS_EPTP, eptp);
    vmx_write(VMX_CTLS_VPID, 1); // VPID is 1

    uint64_t guest_code = hypervisor_ept_guest_to_host(ept_pml4_base, 0x1000);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "Guest code:0x%llx", guest_code);

    uint64_t guest_code_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(guest_code);

    PRINTLOG(HYPERVISOR, LOG_ERROR, "Guest code VA:0x%llx", guest_code_va);

    uint8_t* guest_code_ptr = (uint8_t*)guest_code_va;

    guest_code_ptr[0] = 0x90; // NOP
    guest_code_ptr[1] = 0xeb; // JMP
    guest_code_ptr[2] = 0xfd; // -1

    return 0;
}

void hypervisor_vmcs_dump(void) {

    PRINTLOG(HYPERVISOR, LOG_ERROR, "VMCS DUMP Host State");
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   RSP:0x%llx",
             vmx_read(VMX_HOST_RSP));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   CR0:0x%llx CR3:0x%llx CR4:0x%llx",
             vmx_read(VMX_HOST_CR0), vmx_read(VMX_HOST_CR3),
             vmx_read(VMX_HOST_CR4));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   RIP:0x%llx EFER:0x%llx",
             vmx_read(VMX_HOST_RIP),
             vmx_read(VMX_HOST_EFER));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   CS  SELECTOR:0x%llx", vmx_read(VMX_HOST_CS_SELECTOR));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   DS  SELECTOR:0x%llx", vmx_read(VMX_HOST_DS_SELECTOR));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   ES  SELECTOR:0x%llx", vmx_read(VMX_HOST_ES_SELECTOR));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   FS  SELECTOR:0x%llx BASE:0x%llx",
             vmx_read(VMX_HOST_FS_SELECTOR), vmx_read(VMX_HOST_FS_BASE));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   GS  SELECTOR:0x%llx BASE:0x%llx",
             vmx_read(VMX_HOST_GS_SELECTOR), vmx_read(VMX_HOST_GS_BASE));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   SS  SELECTOR:0x%llx", vmx_read(VMX_HOST_SS_SELECTOR));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   TR SELECTOR:0x%llx", vmx_read(VMX_HOST_TR_SELECTOR));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   GDTR SELECTOR:NA BASE:0x%llx", vmx_read(VMX_HOST_GDTR_BASE));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   TR SELECTOR:0x%llx BASE:0x%llx",
             vmx_read(VMX_HOST_TR_SELECTOR), vmx_read(VMX_HOST_TR_BASE));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   IDTR SELECTOR:NA base:0x%llx", vmx_read(VMX_HOST_IDTR_BASE));

    PRINTLOG(HYPERVISOR, LOG_ERROR, "VMCS DUMP Guest State");
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   RSP:0x%llx",
             vmx_read(VMX_GUEST_RSP));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   CR0:0x%llx CR3:0x%llx CR4:0x%llx",
             vmx_read(VMX_GUEST_CR0), vmx_read(VMX_GUEST_CR3),
             vmx_read(VMX_GUEST_CR4));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   RIP:0x%llx RFLAGS:0x%llx EFER:0x%llx",
             vmx_read(VMX_GUEST_RIP), vmx_read(VMX_GUEST_RFLAGS),
             vmx_read(VMX_GUEST_IA32_EFER));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   CS  SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx",
             vmx_read(VMX_GUEST_CS_SELECTOR), vmx_read(VMX_GUEST_CS_BASE),
             vmx_read(VMX_GUEST_CS_LIMIT), vmx_read(VMX_GUEST_CS_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   DS  SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx",
             vmx_read(VMX_GUEST_DS_SELECTOR), vmx_read(VMX_GUEST_DS_BASE),
             vmx_read(VMX_GUEST_DS_LIMIT), vmx_read(VMX_GUEST_DS_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   ES  SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx",
             vmx_read(VMX_GUEST_ES_SELECTOR), vmx_read(VMX_GUEST_ES_BASE),
             vmx_read(VMX_GUEST_ES_LIMIT), vmx_read(VMX_GUEST_ES_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   FS  SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx",
             vmx_read(VMX_GUEST_FS_SELECTOR), vmx_read(VMX_GUEST_FS_BASE),
             vmx_read(VMX_GUEST_FS_LIMIT), vmx_read(VMX_GUEST_FS_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   GS  SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx",
             vmx_read(VMX_GUEST_GS_SELECTOR), vmx_read(VMX_GUEST_GS_BASE),
             vmx_read(VMX_GUEST_GS_LIMIT), vmx_read(VMX_GUEST_GS_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   SS  SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx",
             vmx_read(VMX_GUEST_SS_SELECTOR), vmx_read(VMX_GUEST_SS_BASE),
             vmx_read(VMX_GUEST_SS_LIMIT), vmx_read(VMX_GUEST_SS_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   LDTR SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx"
             , vmx_read(VMX_GUEST_LDTR_SELECTOR), vmx_read(VMX_GUEST_LDTR_BASE),
             vmx_read(VMX_GUEST_LDTR_LIMIT), vmx_read(VMX_GUEST_LDTR_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   GDTR SELECTOR:NA BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:NA",
             vmx_read(VMX_GUEST_GDTR_BASE), vmx_read(VMX_GUEST_GDTR_LIMIT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   TR SELECTOR:0x%llx BASE:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:0x%llx",
             vmx_read(VMX_GUEST_TR_SELECTOR), vmx_read(VMX_GUEST_TR_BASE),
             vmx_read(VMX_GUEST_TR_LIMIT), vmx_read(VMX_GUEST_TR_ACCESS_RIGHT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   IDTR SELECTOR:NA base:0x%llx, LIMIT:0x%llx, ACCESS-RIGHT:NA",
             vmx_read(VMX_GUEST_IDTR_BASE), vmx_read(VMX_GUEST_IDTR_LIMIT));

    PRINTLOG(HYPERVISOR, LOG_ERROR, "VMCS DUMP Control");
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   PIN_BASED_VM_EXECUTION:0x%llx",
             vmx_read(VMX_CTLS_PIN_BASED_VM_EXECUTION));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   PROC_BASED_VM_EXECUTION(PRI):0x%llx",
             vmx_read(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   PROC_BASED_VM_EXECUTION(SEC):0x%llx",
             vmx_read(VMX_CTLS_SEC_PROC_BASED_VM_EXECUTION));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   EXCEPTION_BITMAP:0x%llx",
             vmx_read(VMX_CTLS_EXCEPTION_BITMAP));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   CR3_TARGET_COUNT:0x%llx",
             vmx_read(VMX_CTLS_CR3_TARGET_COUNT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_EXIT:0x%llx",
             vmx_read(VMX_CTLS_VM_EXIT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_EXIT_MSR_STORE_COUNT:0x%llx",
             vmx_read(VMX_CTLS_VM_EXIT_MSR_STORE_COUNT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_EXIT_MSR_LOAD_COUNT:0x%llx",
             vmx_read(VMX_CTLS_VM_EXIT_MSR_LOAD_COUNT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_EXIT_MSR_LOAD:0x%llx",
             vmx_read(VMX_CTLS_VM_EXIT_MSR_LOAD));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_EXIT_MSR_STORE:0x%llx",
             vmx_read(VMX_CTLS_VM_EXIT_MSR_STORE));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_ENTRY:0x%llx",
             vmx_read(VMX_CTLS_VM_ENTRY));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_ENTRY_MSR_LOAD_COUNT:0x%llx",
             vmx_read(VMX_CTLS_VM_ENTRY_MSR_LOAD_COUNT));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_ENTRY_MSR_LOAD:0x%llx",
             vmx_read(VMX_CTLS_VM_ENTRY_MSR_LOAD));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VM_ENTRY_INTERRUPT_INFORMATION_FIELD:0x%llx",
             vmx_read(VMX_CTLS_VM_ENTRY_INTERRUPT_INFORMATION_FIELD));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   IO BITMAP A:0x%llx",
             vmx_read(VMX_CTLS_IO_BITMAP_A));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   IO BITMAP B:0x%llx",
             vmx_read(VMX_CTLS_IO_BITMAP_B));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   EPTP:0x%llx",
             vmx_read(VMX_CTLS_EPTP));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "   VPID:0x%llx",
             vmx_read(VMX_CTLS_VPID));

}
