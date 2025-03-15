/**
 * @file hypervisor_vmcs_ops.64.c
 * @brief Hypervisor VMCS operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_vmx_vmcs_ops.h>
#include <hypervisor/hypervisor_vmx_ops.h>
#include <hypervisor/hypervisor_vmx_utils.h>
#include <hypervisor/hypervisor_vmx_macros.h>
#include <hypervisor/hypervisor_ept.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <cpu/task.h>
#include <apic.h>
#include <logging.h>

MODULE("turnstone.hypervisor");

uint32_t hypervisor_vmx_vmcs_revision_id(void) {
    return cpu_read_msr(CPU_MSR_IA32_VMX_BASIC) & 0xffffffff;
}

#if 0
static int8_t vmx_validate_capability(uint64_t target, uint32_t allowed0, uint32_t allowed1) {
    int idx = 0;

    for (idx = 0; idx < 32; idx++) {
        uint32_t mask = 1 << idx;
        int target_is_set = !!(target & mask);
        int allowed0_is_set = !!(allowed0 & mask);
        int allowed1_is_set = !!(allowed1 & mask);

        if ((allowed0_is_set && !target_is_set) || (!allowed1_is_set && target_is_set)) {
            return -1;
        }
    }

    return 0;
}
#endif

static uint32_t vmx_fix_reserved_1_bits(uint32_t target, uint32_t allowed0) {
    int idx = 0;

    for (idx = 0; idx < 32; idx++) {
        uint32_t mask = 1 << idx;
        int target_is_set = !!(target & mask);
        int allowed0_is_set = !!(allowed0 & mask);

        if (allowed0_is_set && !target_is_set) {
            target |= mask;
        }
    }

    return target;
}

static uint32_t vmx_fix_reserved_0_bits(uint32_t target, uint32_t allowed1) {
    int idx = 0;

    for (idx = 0; idx < 32; idx++) {
        uint32_t mask = 1 << idx;
        int target_is_set = !!(target & mask);
        int allowed1_is_set = !!(allowed1 & mask);

        if (!allowed1_is_set && target_is_set) {
            target &= ~mask;
        }
    }

    return target;
}

static int8_t hypervisor_vmx_vmcs_prepare_host_state(hypervisor_vm_t* vm) {
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


    vmx_write(VMX_HOST_RSP, hypervisor_create_stack(vm, 64 << 10));
    vmx_write(VMX_HOST_EFER, efer);

    return 0;
}

static int8_t hypervisor_vmx_vmcs_prepare_guest_state(void) {
    // prepare guest state at long mode
    vmx_write(VMX_GUEST_ES_SELECTOR, 0x10);
    vmx_write(VMX_GUEST_CS_SELECTOR, 0x08);
    vmx_write(VMX_GUEST_DS_SELECTOR, 0x10);
    vmx_write(VMX_GUEST_FS_SELECTOR, 0x10);
    vmx_write(VMX_GUEST_GS_SELECTOR, 0x10);
    vmx_write(VMX_GUEST_SS_SELECTOR, 0x10);
    vmx_write(VMX_GUEST_TR_SELECTOR, 0x18);
    vmx_write(VMX_GUEST_LDTR_SELECTOR, 0x0);
    vmx_write(VMX_GUEST_CS_BASE, 0x0);
    vmx_write(VMX_GUEST_DS_BASE, 0x0);
    vmx_write(VMX_GUEST_ES_BASE, 0x0);
    vmx_write(VMX_GUEST_FS_BASE, 0x0);
    vmx_write(VMX_GUEST_GS_BASE, 0x0);
    vmx_write(VMX_GUEST_SS_BASE, 0x0);
    vmx_write(VMX_GUEST_LDTR_BASE, 0x0);
    vmx_write(VMX_GUEST_IDTR_BASE, VMX_GUEST_IDTR_BASE_VALUE);
    vmx_write(VMX_GUEST_GDTR_BASE, VMX_GUEST_GDTR_BASE_VALUE);
    vmx_write(VMX_GUEST_TR_BASE, VMX_GUEST_TR_BASE_VALUE);
    vmx_write(VMX_GUEST_CS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_DS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_ES_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_FS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_GS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_SS_LIMIT, 0xffff);
    vmx_write(VMX_GUEST_LDTR_LIMIT, 0x0);
    vmx_write(VMX_GUEST_TR_LIMIT, 0x67);
    vmx_write(VMX_GUEST_GDTR_LIMIT, 0x2f);
    vmx_write(VMX_GUEST_IDTR_LIMIT, 0xfff);
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
    cr0.fields.protection_enabled = 1;
    cr0.fields.monitor_coprocessor = 1;
    cr0.fields.emulation = 0;
    cr0.fields.task_switched = 0;
    cr0.fields.numeric_error = 1;
    cr0.fields.write_protect = 1;
    cr0.fields.paging = 1;

    vmx_write(VMX_GUEST_CR0, cr0.bits);

    vmx_write(VMX_GUEST_CR3, VMX_GUEST_CR3_BASE_VALUE); // cr3 is set to 16kib

    uint64_t cr4_fixed = cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED0);
    // cr4_fixed |= cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED1);
    cpu_reg_cr4_t cr4 = { .bits = cr4_fixed };

    cr4.fields.physical_address_extension = 1;
    cr4.fields.os_fx_support = 1;
    cr4.fields.os_unmasked_exception_support = 1;
    cr4.fields.page_global_enable = 1;

    vmx_write(VMX_GUEST_CR4, cr4.bits);

    vmx_write(VMX_GUEST_DR7, 0x0);
    vmx_write(VMX_GUEST_RFLAGS, VMX_RFLAG_RESERVED);
    vmx_write(VMX_GUEST_VMCS_LINK_POINTER_LOW, 0xffffffff);
    vmx_write(VMX_GUEST_VMCS_LINK_POINTER_HIGH, 0xffffffff);
    vmx_write(VMX_GUEST_IA32_EFER, 0xD00); // enable long mode LME/LMA bit and NXE bit

    return 0;
}



static int8_t hypervisor_vmx_vmcs_prepare_pinbased_control(void){
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

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pinbased_vm_execution_ctrl: 0x%08x resv 1: 0x%08x resv 0: 0x%08x",
             pinbased_vm_execution_ctrl, pinbased_msr_eax, pinbased_msr_edx);

    vmx_write(VMX_CTLS_PIN_BASED_VM_EXECUTION, pinbased_vm_execution_ctrl);

    return 0;
}

static int8_t hypervisor_vmx_msr_bitmap_set(uint8_t * bitmap, uint32_t msr, boolean_t read) {
    if(read) {
        if(msr >= 0xC0000000) {
            bitmap += 1024;
        }
    } else {
        bitmap += 2048;

        if(msr >= 0xC0000000) {
            bitmap += 1024;
        }
    }


    uint32_t byte_index = msr / 8;
    uint8_t bit_index = msr % 8;
    bitmap[byte_index] |= 1 << bit_index;
    return 0;
}

static int8_t hypervisor_vmx_vmcs_prepare_procbased_control(hypervisor_vm_t* vm) {
    // uint32_t vpid_and_ept_msr_eax, vpid_and_ept_msr_edx;
    // uint64_t vpid_and_ept_msr = cpu_read_msr(CPU_MSR_IA32_VMX_EPT_VPID_CAP);
    // vpid_and_ept_msr_eax = vpid_and_ept_msr & 0xffffffff;
    // vpid_and_ept_msr_edx = vpid_and_ept_msr >> 32;

    uint32_t pri_procbased_msr_eax, pri_procbased_msr_edx;
    uint64_t pri_procbased_msr = cpu_read_msr(CPU_MSR_IA32_VMX_PRI_PROCBASED_CTLS);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "pri_procbased_msr:0x%016llx", pri_procbased_msr);
    pri_procbased_msr_eax = pri_procbased_msr & 0xffffffff;
    pri_procbased_msr_edx = pri_procbased_msr >> 32;

    uint32_t sec_procbased_msr_eax, sec_procbased_msr_edx;
    uint64_t sec_procbased_msr = cpu_read_msr(CPU_MSR_IA32_VMX_SEC_PROCBASED_CTLS);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "sec_procbased_msr:0x%016llx", sec_procbased_msr);
    sec_procbased_msr_eax = sec_procbased_msr & 0xffffffff;
    sec_procbased_msr_edx = sec_procbased_msr >> 32;


    uint32_t pri_procbase_ctls = 0;
    pri_procbase_ctls |= 1 << 2; // enable interrupt-window exit
    pri_procbase_ctls |= 1 << 7; // Hlt causes vm exit
    pri_procbase_ctls |= 1 << 9; // INVLPG causes vm exit
    pri_procbase_ctls |= 1 << 12; // RDTSC exiting
    pri_procbase_ctls |= 1 << 15; // CR3-load causes vm exit
    pri_procbase_ctls |= 1 << 16; // CR3-store causes vm exit
    pri_procbase_ctls |= 1 << 19; // CR8-load causes vm exit
    pri_procbase_ctls |= 1 << 20; // CR8-store causes vm exit
    pri_procbase_ctls |= 1 << 21; // Use TPR shadow
    pri_procbase_ctls |= 1 << 24; // Unconditional IO exiting
    pri_procbase_ctls |= 1 << 25; // Use IO bitmap
    pri_procbase_ctls |= 1 << 28; // Use MSR bitmap
    pri_procbase_ctls |= 1 << 30; // PAUSE causes vm exit
    pri_procbase_ctls |= 1 << 31; // activate secondary controls

    pri_procbase_ctls = vmx_fix_reserved_1_bits(pri_procbase_ctls, pri_procbased_msr_eax);
    pri_procbase_ctls = vmx_fix_reserved_0_bits(pri_procbase_ctls, pri_procbased_msr_edx);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pri_procbase_ctls: 0x%08x resv 1: 0x%08x resv 0: 0x%08x",
             pri_procbase_ctls, pri_procbased_msr_eax, pri_procbased_msr_edx);

    vmx_write(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION, pri_procbase_ctls);

    uint32_t sec_procbase_ctls = 0;
    // sec_procbase_ctls |= 1 << 0; // virtualize APIC access cannot be enabled when x2APIC mode is enabled
    sec_procbase_ctls |= 1 << 1; // use EPT
    sec_procbase_ctls |= 1 << 2; // descriptor-table exiting:GDT/LDT/IDT/TR
    sec_procbase_ctls |= 1 << 3; // enable RDTSCP
    sec_procbase_ctls |= 1 << 4; // enable virtualize APIC access x2APIC mode
    sec_procbase_ctls |= 1 << 5; // enable VPID
    sec_procbase_ctls |= 1 << 7; // unrestricted guest
    sec_procbase_ctls |= 1 << 8; // enable virtualize APIC register access
    sec_procbase_ctls |= 1 << 9; // virtual-interrupt delivery

    sec_procbase_ctls = vmx_fix_reserved_1_bits(sec_procbase_ctls, sec_procbased_msr_eax);
    sec_procbase_ctls = vmx_fix_reserved_0_bits(sec_procbase_ctls, sec_procbased_msr_edx);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "sec_procbase_ctls: 0x%08x resv 1: 0x%08x resv 0: 0x%08x",
             sec_procbase_ctls, sec_procbased_msr_eax, sec_procbased_msr_edx);

    vmx_write(VMX_CTLS_SEC_PROC_BASED_VM_EXECUTION, sec_procbase_ctls);

    vmx_write(VMX_CTLS_EXCEPTION_BITMAP, 0xFFFFFFFF);
    vmx_write(VMX_CTLS_CR3_TARGET_COUNT, 0x0);

    frame_t* vapic_frame = NULL;

    uint64_t vapic_region_va = hypervisor_allocate_region(&vapic_frame, 0x1000);

    if (vapic_region_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate VAPIC region");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VAPIC] = *vapic_frame;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vapic_region_va:0x%llx", vapic_region_va);

    uint64_t vapic_region_pa = vapic_frame->frame_address;

    vmx_write(VMX_CTLS_VIRTUAL_APIC_PAGE_ADDR, vapic_region_pa);
    vmx_write(VMX_CTLS_APIC_ACCESS_ADDR, 0xfee00000);

    frame_t* msr_bitmap_frame = NULL;

    uint64_t msr_bitmap_region_va = hypervisor_allocate_region(&msr_bitmap_frame, 0x2000);

    if (msr_bitmap_region_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate MSR bitmap region");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_MSR_BITMAP] = *msr_bitmap_frame;

    uint8_t * msr_bitmap = (uint8_t*)msr_bitmap_region_va;

#if 0
    if(!(sec_procbase_ctls & (1 << 8))) { // if vapic register access is not enabled ensure vapic register access is intercepted
        hypervisor_vmx_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_LVT_TIMER, false);
        hypervisor_vmx_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_TIMER_DIVIDER, false);
        hypervisor_vmx_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_TIMER_INITIAL_VALUE, false);
        vm->vapic_register_access_enabled = false;
        PRINTLOG(HYPERVISOR, LOG_DEBUG, "vapic register access intercepted, no vapic register access.");
    } else {
        vm->vapic_register_access_enabled = true;
        PRINTLOG(HYPERVISOR, LOG_DEBUG, "vapic register access not intercepted, vapic register access enabled.");
    }
#else // do we need always watching these registers for write access?
    hypervisor_vmx_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_LVT_TIMER, false);
    hypervisor_vmx_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_TIMER_DIVIDER, false);
    hypervisor_vmx_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_TIMER_INITIAL_VALUE, false);
#endif

    if(!(sec_procbase_ctls & (1 << 9))) { // if virtual-interrupt delivery is not enabled ensure EOI MSR is intercepted
        hypervisor_vmx_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_EOI, false);
        vm->vid_enabled = false;
        PRINTLOG(HYPERVISOR, LOG_DEBUG, "EOI MSR intercepted, no VID support.");
    } else {
        vm->vid_enabled = true;
        PRINTLOG(HYPERVISOR, LOG_DEBUG, "EOI MSR not intercepted, VID enabled.");
    }

    uint64_t msr_bitmap_region_pa = msr_bitmap_frame->frame_address;

    vmx_write(VMX_CTLS_MSR_BITMAP, msr_bitmap_region_pa);

    return 0;
}

static void hypervisor_vmx_io_bitmap_set_port(uint8_t * bitmap, uint16_t port) {
    uint16_t byte_index = port >> 3;
    uint8_t bit_index = port & 0x7;
    bitmap[byte_index] |= 1 << bit_index;
}


static int8_t hypervisor_vmx_vmcs_prepare_io_bitmap(hypervisor_vm_t* vm) {
    frame_t* io_bitmap_frame = NULL;

    uint8_t * io_bitmap_region_va = (uint8_t*)hypervisor_allocate_region(&io_bitmap_frame, 0x2000);

    if (io_bitmap_region_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate IO bitmap region");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_IO_BITMAP] = *io_bitmap_frame;

    // Enable serial ports
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x3f8);
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x3f9);
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x3fa);
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x3fb);
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x3fc);
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x3fd);
    // Enable keyboard ports
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x60);
    hypervisor_vmx_io_bitmap_set_port(io_bitmap_region_va, 0x64);


    uint64_t io_bitmap_region_pa = io_bitmap_frame->frame_address;

    vmx_write(VMX_CTLS_IO_BITMAP_A, io_bitmap_region_pa);
    vmx_write(VMX_CTLS_IO_BITMAP_B, io_bitmap_region_pa + FRAME_SIZE);

    return 0;
}


static int8_t hypervisor_vmx_vmcs_prepare_execution_control(hypervisor_vm_t* vm) {
    hypervisor_vmx_vmcs_prepare_pinbased_control();
    hypervisor_vmx_vmcs_prepare_io_bitmap(vm);
    hypervisor_vmx_vmcs_prepare_procbased_control(vm);

    return 0;
}

static int8_t hypervisor_vmx_vmcs_prepare_vm_exit_and_entry_control(hypervisor_vm_t* vm) {
    uint32_t vm_exit_msr_eax, vm_exit_msr_edx;
    uint64_t vm_exit_msr = cpu_read_msr(CPU_MSR_IA32_VMX_VM_EXIT_CTLS);
    vm_exit_msr_eax = vm_exit_msr & 0xffffffff;
    vm_exit_msr_edx = vm_exit_msr >> 32;

    uint32_t vm_exit_ctls = 0;
    vm_exit_ctls |= 1 << 9; // VM exit to 64-bit long mode.
    vm_exit_ctls |= 1 << 15; // ACK external interrupts.
    vm_exit_ctls |= 1 << 20; // Save IA32_EFER on vm-exit
    vm_exit_ctls |= 1 << 21; // Load IA32_EFER on vm-exit
    vm_exit_ctls = vmx_fix_reserved_1_bits(vm_exit_ctls, vm_exit_msr_eax);
    vm_exit_ctls = vmx_fix_reserved_0_bits(vm_exit_ctls, vm_exit_msr_edx);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vm_exit_ctls:0x%08x resv 1:0x%08x resv 0:0x%08x",
             vm_exit_ctls, vm_exit_msr_eax, vm_exit_msr_edx);

    vmx_write(VMX_CTLS_VM_EXIT, vm_exit_ctls);

    uint32_t predefined_msrs[0]; // = {IA32_EFER_MSR};
    uint32_t nr_msrs = sizeof(predefined_msrs) / sizeof(uint32_t);
    uint32_t vm_exit_load_msr_count = nr_msrs;
    uint32_t vm_exit_store_msr_count = nr_msrs;

    frame_t* vm_exit_load_msr_region = NULL;
    frame_t* vm_exit_store_msr_region = NULL;

    uint64_t vm_exit_load_msr_region_va = hypervisor_allocate_region(&vm_exit_load_msr_region, FRAME_SIZE);

    if(vm_exit_load_msr_region_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate vm_exit_load_msr_region");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VM_EXIT_LOAD_MSR] = *vm_exit_load_msr_region;

    uint64_t vm_exit_store_msr_region_va = hypervisor_allocate_region(&vm_exit_store_msr_region, FRAME_SIZE);

    if(vm_exit_store_msr_region_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate vm_exit_store_msr_region");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VM_EXIT_STORE_MSR] = *vm_exit_store_msr_region;

    for (uint32_t index = 0; index < nr_msrs; index++) {
        vmx_vmcs_msr_blob_t * vmcs_msr = ((vmx_vmcs_msr_blob_t *)vm_exit_store_msr_region_va) + index;
        vmcs_msr->index = predefined_msrs[index];
        vmcs_msr->reserved = 0;
        uint64_t vmcs_msr_value = cpu_read_msr(vmcs_msr->index);
        vmcs_msr->msr_eax = vmcs_msr_value & 0xffffffff;
        vmcs_msr->msr_edx = vmcs_msr_value >> 32;

        vmcs_msr = ((vmx_vmcs_msr_blob_t *)vm_exit_load_msr_region_va) + index;
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

    vm_entry_ctls |= 1 << 9; // VM entry to 64-bit long mode.
    vm_entry_ctls |= 1 << 15; // load EFER msr on vm-entry
    vm_entry_ctls = vmx_fix_reserved_1_bits(vm_entry_ctls, vm_entry_msr_eax);
    vm_entry_ctls = vmx_fix_reserved_0_bits(vm_entry_ctls, vm_entry_msr_edx);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vm_entry_ctls:0x%08x resv 1:0x%08x resv 0:0x%08x",
             vm_entry_ctls, vm_entry_msr_eax, vm_entry_msr_edx);

    vmx_write(VMX_CTLS_VM_ENTRY, vm_entry_ctls);
    vmx_write(VMX_CTLS_VM_ENTRY_MSR_LOAD_COUNT, vm_exit_store_msr_count);
    vmx_write(VMX_CTLS_VM_ENTRY_MSR_LOAD, vm_exit_store_msr_region->frame_address);
    vmx_write(VMX_CTLS_VM_ENTRY_INTERRUPT_INFORMATION_FIELD, 0x0);

    return 0;
}

int8_t hypervisor_vmx_vmcs_prepare_ept(hypervisor_vm_t* vm) {
    uint64_t ept_pml4_base = hypervisor_ept_setup(vm);

    if (ept_pml4_base == -1ULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "EPT setup failed");
        return -1;
    }

    if(hypervisor_ept_build_tables(vm) == -1) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "EPT build tables failed");
        return -1;
    }

    uint64_t vpid_cap = cpu_read_msr(CPU_MSR_IA32_VMX_EPT_VPID_CAP);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "VPID_CAP:0x%llx", vpid_cap);

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

    return 0;
}

void hypervisor_vmx_vmcs_dump(void) {

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

int8_t hypervisor_vmx_vmcs_prepare(hypervisor_vm_t** vm_out) {
    if(!vm_out) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vm_out is NULL");
        return -1;
    }

    frame_t* vm_frame = NULL;

    uint64_t vm_frame_va = hypervisor_allocate_region(&vm_frame, FRAME_SIZE);

    if(vm_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vm frame");
        return -1;
    }

    hypervisor_vm_t* vm = (hypervisor_vm_t*)vm_frame_va;

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_SELF] = *vm_frame;

    frame_t* vmcs_frame = NULL;

    uint64_t vmcs_frame_va = hypervisor_allocate_region(&vmcs_frame, FRAME_SIZE);

    if(vmcs_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vmcs frame");
        return -1;
    }

    vm->vmcs_frame_fa = vmcs_frame->frame_address;
    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VMCS] = *vmcs_frame;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vmcs frame va: 0x%llx", vmcs_frame_va);

    uint32_t revision_id = hypervisor_vmx_vmcs_revision_id();

    *(uint32_t*)vmcs_frame_va = revision_id;

    uint8_t err = 0;

    err = vmclear(vmcs_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmclear failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vmclear success");

    err = vmptrld(vmcs_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmptrld failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vmptrld success");

    if(hypervisor_vmx_vmcs_prepare_host_state(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare host state");
        return -1;
    }

    if(hypervisor_vmx_vmcs_prepare_guest_state() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare guest state");
        return -1;
    }

    if(hypervisor_vmx_vmcs_prepare_execution_control(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare execution control");
        return -1;
    }

    if(hypervisor_vmx_vmcs_prepare_vm_exit_and_entry_control(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare vm exit control");
        return -1;
    }

    if(hypervisor_vmx_vmcs_prepare_vmexit_handlers() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare vmexit handlers");
        return -1;
    }

    err = vmclear(vmcs_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmclear failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vmclear success");


    *vm_out = vm;

    return 0;
}

