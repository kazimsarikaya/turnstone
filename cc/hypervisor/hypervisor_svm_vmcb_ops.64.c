/**
 * @file hypervisor_svm_vmcb_ops.64.c
 * @brief SVM vmcb operations for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_svm_vmcb_ops.h>
#include <hypervisor/hypervisor_svm_macros.h>
#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_ept.h>
#include <memory/paging.h>
#include <cpu.h>
#include <cpu/task.h>
#include <apic.h>
#include <cpu/crx.h>
#include <logging.h>

MODULE("turnstone.hypervisor.svm");

static inline void hypervisor_svm_io_bitmap_set_port(uint8_t * bitmap, uint16_t port) {
    uint16_t byte_index = port >> 3;
    uint8_t bit_index = port & 0x7;
    bitmap[byte_index] |= 1 << bit_index;
}


static int8_t hypervisor_svm_msr_bitmap_set(uint8_t * bitmap, uint32_t msr, boolean_t read) {
    if(msr == APIC_X2APIC_MSR_EOI) { // we don't want to intercept EOI, it is handled by the avic
        return 0;
    }


    uint32_t msr_offset = msr & 0x1FFF;

    if(msr >= 0xC0000000 && msr <= 0xC0001FFF) {
        bitmap += 0x800;
    } else if(msr >= 0xC0010000 && msr <= 0xC0011FFF) {
        bitmap += 0x1000;
    }

    uint32_t byte_index = msr_offset / 4; // each byte represents 4 msrs with 2 bits each

    uint8_t bit_index = msr_offset % 4;
    uint8_t read_bit_index = bit_index * 2;
    uint8_t write_bit_index = bit_index * 2 + 1;

    if(read) {
        bitmap[byte_index] |= 1 << read_bit_index;
    } else {
        bitmap[byte_index] |= 1 << write_bit_index;
    }

    return 0;
}

int8_t hypervisor_svm_vmcb_set_running(hypervisor_vm_t* vm) {
    uint64_t vmcb_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    if(vmcb_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot get vmcb va");
        return -1;
    }

    uint64_t phy_apic_id_table_frame_fa = vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_PHYSICAL_APIC_ID_TABLE].frame_address;
    uint64_t phy_apic_id_table_frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(phy_apic_id_table_frame_fa);

    uint64_t* phy_apic_id_table = (uint64_t*)phy_apic_id_table_frame_va;

    phy_apic_id_table[0] = (3ULL << 62) | vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VAPIC].frame_address | task_get_cpu_id();

    PRINTLOG(HYPERVISOR, LOG_TRACE, "phy apic id table: 0x%llx", phy_apic_id_table[0]);

    return 0;
}

int8_t hypervisor_svm_vmcb_set_stopped(hypervisor_vm_t* vm) {
    uint64_t vmcb_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    if(vmcb_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot get vmcb va");
        return -1;
    }

    uint64_t phy_apic_id_table_frame_fa = vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_PHYSICAL_APIC_ID_TABLE].frame_address;
    uint64_t phy_apic_id_table_frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(phy_apic_id_table_frame_fa);

    uint64_t* phy_apic_id_table = (uint64_t*)phy_apic_id_table_frame_va;

    phy_apic_id_table[0] = (1ULL << 63) | vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VAPIC].frame_address | task_get_cpu_id();

    PRINTLOG(HYPERVISOR, LOG_TRACE, "phy apic id table: 0x%llx", phy_apic_id_table[0]);

    return 0;
}

static void hypervisor_svm_vmcb_set_vapic_defaults(uint64_t vapic_address) {
    uint32_t* vapic = (uint32_t*)vapic_address;

    uint64_t apic_vesion = cpu_read_msr(0x803);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "apic version: 0x%llx", apic_vesion);

    vapic[0x20 / 4] = 0x0; // id
    vapic[0x30 / 4] = apic_vesion; // version
    vapic[0xE0 / 4] = 0xffffffff; // dfr
    vapic[0xF0 / 4] = 0x000000ff; // spurious interrupt vector
    vapic[0x320 / 4] = 0x00010000; // timer local vector table entry
    vapic[0x330 / 4] = 0x00010000; // thermal sensor local vector table entry
    vapic[0x340 / 4] = 0x00010000; // performance counter local vector table entry
    vapic[0x350 / 4] = 0x00010000; // local interrupt 0 local vector table entry
    vapic[0x360 / 4] = 0x00010000; // local interrupt 1 local vector table entry
    vapic[0x370 / 4] = 0x00010000; // error local vector table entry
    vapic[0x400 / 4] = 0x00040007; // extended apic feature register

    for(uint32_t i = 0x480; i <= 0x4F0; i += 0x10) {
        vapic[i / 4] = 0xffffffff; // ier;
    }




}

static int8_t hypervisor_svm_vmcb_prepare_control_area(hypervisor_vm_t* vm) {
    uint64_t vmcb_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    if(vmcb_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot get vmcb va");
        return -1;
    }

    cpu_cpuid_regs_t query = {0};
    cpu_cpuid_regs_t result = {0};

    query.eax = 0x8000000a;

    if(cpu_cpuid(query, &result) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot get avic feature");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "eax: 0x%x, ebx: 0x%x, ecx: 0x%x, edx: 0x%x", result.eax, result.ebx, result.ecx, result.edx);

    boolean_t avic = (result.edx >> 13) & 1;
    boolean_t avic_x2apic = (result.edx >> 18) & 1;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "avic: %i, avic_x2apic: %i", avic, avic_x2apic);

    if(avic == 0 || avic_x2apic == 0) {
        vm->vid_enabled = false;
    }

    svm_vmcb_t* vmcb = (svm_vmcb_t*)vmcb_va;

    vmcb->control_area.intercept_crx.bits = 0xFFFFFFFF;

    vmcb->control_area.intercept_interrupt.bits = 0xFFFFFFFF;

    vmcb->control_area.intercept_control_1.fields.intr = 1;
    vmcb->control_area.intercept_control_1.fields.nmi = 1;
    vmcb->control_area.intercept_control_1.fields.cr0_write = 1;
    vmcb->control_area.intercept_control_1.fields.rdtsc = 1;
    vmcb->control_area.intercept_control_1.fields.rdpmc = 1;
    vmcb->control_area.intercept_control_1.fields.cpuid = 1;
    vmcb->control_area.intercept_control_1.fields.intn = 1;
    vmcb->control_area.intercept_control_1.fields.pause = 1;
    vmcb->control_area.intercept_control_1.fields.hlt = 1;
    vmcb->control_area.intercept_control_1.fields.io = 1;
    vmcb->control_area.intercept_control_1.fields.msr = 1;
    vmcb->control_area.intercept_control_1.fields.shutdown = 1;

    vmcb->control_area.intercept_control_2.fields.vmrun = 1; // amd requires this whenever we dont use nested vm inside vm, else invalid vmexit occurs.
    vmcb->control_area.intercept_control_2.fields.vmmcall = 1;
    vmcb->control_area.intercept_control_2.fields.rdtscp = 1;

    vmcb->control_area.intercept_control_3.fields.idle_hlt = 1;

    vmcb->control_area.guest_asid.fields.asid = 1;
    vmcb->control_area.guest_asid.fields.tlb_control = 0x3;

    vmcb->control_area.nested_page_control.fields.np_enable = 1;

    vmcb->control_area.vint_control.fields.v_gif = 1;
    vmcb->control_area.vint_control.fields.v_gif_enabled = 1;
    vmcb->control_area.vint_control.fields.v_nmi_enabled = 1;
    vmcb->control_area.vint_control.fields.v_intr_mask = 1;
    vmcb->control_area.vint_control.fields.vapic_apic = 1;
    vmcb->control_area.vint_control.fields.vapic_x2apic = 1;

    frame_t* vapic_frame = NULL;

    uint64_t vapic_frame_va = hypervisor_allocate_region(&vapic_frame, FRAME_SIZE);

    if(vapic_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vapic frame");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vapic frame: 0x%llx", vapic_frame->frame_address);

    hypervisor_svm_vmcb_set_vapic_defaults(vapic_frame_va);

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VAPIC] = *vapic_frame;

    vmcb->control_area.avic_apic_backing_page_pointer = vapic_frame->frame_address;
    vmcb->control_area.avic_apic_bar = 0xfee00000;

    frame_t* phy_apic_id_table_frame = NULL;

    uint64_t phy_apic_id_table_frame_va = hypervisor_allocate_region(&phy_apic_id_table_frame, FRAME_SIZE);

    if(phy_apic_id_table_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate phy apic id table frame");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "phy apic id table frame: 0x%llx", phy_apic_id_table_frame->frame_address);

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_PHYSICAL_APIC_ID_TABLE] = *phy_apic_id_table_frame;

    vmcb->control_area.avic_physical_table.fields.max_index = 0; // only one entry
    vmcb->control_area.avic_physical_table.fields.physical_address = phy_apic_id_table_frame->frame_address >> 12;

    uint64_t* phy_apic_id_table = (uint64_t*)phy_apic_id_table_frame_va;

    phy_apic_id_table[0] = (1ULL << 63) | vapic_frame->frame_address;

/*
    frame_t* avic_logical_table_frame = NULL;

    uint64_t avic_logical_table_frame_va = hypervisor_allocate_region(&avic_logical_table_frame, FRAME_SIZE);

    if(avic_logical_table_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate avic logical table frame");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_AVIC_LOGICAL_TABLE] = *avic_logical_table_frame;

    vmcb->control_area.avic_logical_table_pointer = avic_logical_table_frame->frame_address;

    uint32_t* avic_logical_table = (uint32_t*)avic_logical_table_frame_va;

    avic_logical_table[0] = (1 << 31) | 0x0;
 */


    frame_t* io_bitmap_frame = NULL;

    uint64_t io_bitmap_frame_va = hypervisor_allocate_region(&io_bitmap_frame, FRAME_SIZE * 3);

    if(io_bitmap_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate io bitmap frame");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_IO_BITMAP] = *io_bitmap_frame;

    vmcb->control_area.iopm_base_pa = io_bitmap_frame->frame_address;

    uint8_t* io_bitmap_region_va = (uint8_t*)io_bitmap_frame_va;

    // Enable serial ports
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x3f8);
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x3f9);
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x3fa);
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x3fb);
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x3fc);
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x3fd);
    // Enable keyboard ports
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x60);
    hypervisor_svm_io_bitmap_set_port(io_bitmap_region_va, 0x64);

    frame_t* msr_bitmap_frame = NULL;

    uint64_t msr_bitmap_frame_va = hypervisor_allocate_region(&msr_bitmap_frame, FRAME_SIZE);

    if(msr_bitmap_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate msr bitmap frame");
        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_MSR_BITMAP] = *msr_bitmap_frame;

    vmcb->control_area.msrpm_base_pa = msr_bitmap_frame->frame_address;

    uint8_t* msr_bitmap = (uint8_t*)msr_bitmap_frame_va;

    /*
       hypervisor_svm_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_LVT_TIMER, false);
       hypervisor_svm_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_TIMER_DIVIDER, false);
       hypervisor_svm_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_TIMER_INITIAL_VALUE, false);
     */

    if(!vm->vid_enabled) {
        hypervisor_svm_msr_bitmap_set(msr_bitmap, APIC_X2APIC_MSR_EOI, false);
    }

    return 0;
}

static int8_t hypervisor_svm_vmcb_prepare_save_state_area(hypervisor_vm_t* vm) {
    uint64_t vmcb_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    if(vmcb_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot get vmcb va");
        return -1;
    }

    svm_vmcb_t* vmcb = (svm_vmcb_t*)vmcb_va;

    vmcb->save_state_area.es.selector = 0x10;
    vmcb->save_state_area.es.attrib = SVM_DATA_ACCESS_RIGHTS;
    vmcb->save_state_area.es.limit = 0xffffffff;
    vmcb->save_state_area.es.base = 0;

    vmcb->save_state_area.cs.selector = 0x08;
    vmcb->save_state_area.cs.attrib = SVM_CODE_ACCESS_RIGHTS;
    vmcb->save_state_area.cs.limit = 0xffffffff;
    vmcb->save_state_area.cs.base = 0;

    vmcb->save_state_area.ss.selector = 0x10;
    vmcb->save_state_area.ss.attrib = SVM_DATA_ACCESS_RIGHTS;
    vmcb->save_state_area.ss.limit = 0xffffffff;
    vmcb->save_state_area.ss.base = 0;

    vmcb->save_state_area.ds.selector = 0x10;
    vmcb->save_state_area.ds.attrib = SVM_DATA_ACCESS_RIGHTS;
    vmcb->save_state_area.ds.limit = 0xffffffff;
    vmcb->save_state_area.ds.base = 0;

    vmcb->save_state_area.fs.selector = 0x0;
    vmcb->save_state_area.fs.attrib = SVM_DATA_ACCESS_RIGHTS;
    vmcb->save_state_area.fs.limit =  0xffffffff;
    vmcb->save_state_area.fs.base = 0;

    vmcb->save_state_area.gs.selector = 0x0;
    vmcb->save_state_area.gs.attrib = SVM_DATA_ACCESS_RIGHTS;
    vmcb->save_state_area.gs.limit = 0xffffffff;
    vmcb->save_state_area.gs.base = 0;

    vmcb->save_state_area.gdtr.selector = 0;
    vmcb->save_state_area.gdtr.attrib = 0;
    vmcb->save_state_area.gdtr.limit = 0x2f;
    vmcb->save_state_area.gdtr.base = SVM_GUEST_GDTR_BASE_VALUE;

    vmcb->save_state_area.ldtr.selector = 0;
    vmcb->save_state_area.ldtr.attrib = SVM_LDTR_ACCESS_RIGHTS;
    vmcb->save_state_area.ldtr.limit = 0;
    vmcb->save_state_area.ldtr.base = 0;

    vmcb->save_state_area.idtr.selector = 0x0;
    vmcb->save_state_area.idtr.attrib = 0;
    vmcb->save_state_area.idtr.limit = 0xfff;
    vmcb->save_state_area.idtr.base = SVM_GUEST_IDTR_BASE_VALUE;

    vmcb->save_state_area.tr.selector = 0x18;
    vmcb->save_state_area.tr.attrib = SVM_TR_ACCESS_RIGHTS;
    vmcb->save_state_area.tr.limit = 0x67;
    vmcb->save_state_area.tr.base = SVM_GUEST_TR_BASE_VALUE;

    vmcb->save_state_area.cpl = 0;

    vmcb->save_state_area.rflags = SVM_RFLAG_RESERVED | (1 << 9); // reserved bit and interrupt flag

    cpu_reg_cr4_t cr4 = cpu_read_cr4();
    vmcb->save_state_area.cr4 = cr4.bits;
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "cr4: 0x%llx", cr4.bits);

    vmcb->save_state_area.cr3 = SVM_GUEST_CR3_BASE_VALUE;

    cpu_reg_cr0_t cr0 = cpu_read_cr0();
    vmcb->save_state_area.cr0 = cr0.bits;
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "cr0: 0x%llx", cr0.bits);

    uint64_t efer = cpu_read_msr(CPU_MSR_EFER);
    // disable 12. bit (svm) if we disable it, vmrun gives invalid vmexit
    // efer &= ~(1 << 12);
    // disable 0. bit (syscall)
    efer &= ~(1 << 0);
    vmcb->save_state_area.efer = efer;
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "efer: 0x%llx", efer);

    uint64_t pat = cpu_read_msr(CPU_MSR_IA32_PAT);

    vmcb->save_state_area.g_pat = pat;

    return 0;
}

int8_t hypervisor_svm_vmcb_prepare_ept(hypervisor_vm_t* vm) {
    uint64_t ept_pml4_base = hypervisor_ept_setup(vm);

    if (ept_pml4_base == -1ULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "EPT setup failed");
        return -1;
    }

    if(hypervisor_ept_build_tables(vm) == -1) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "EPT build tables failed");
        return -1;
    }

    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    vmcb->control_area.n_cr3 = ept_pml4_base;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "EPT prepared at 0x%llx", ept_pml4_base);

    return 0;
}

int8_t hypervisor_svm_vmcb_prepare(hypervisor_vm_t** vm_out) {
    if(vm_out == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vm out pointer");
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

    frame_t* vmcb_frame = NULL;

    uint64_t vmcb_frame_va = hypervisor_allocate_region(&vmcb_frame, FRAME_SIZE * 2);

    if(vmcb_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vmcb frame");
        return -1;
    }

    vm->vmcb_frame_fa = vmcb_frame->frame_address;

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VMCB] = *vmcb_frame;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "vmcb frame va: 0x%llx", vmcb_frame_va);

    if(hypervisor_svm_vmcb_prepare_control_area(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare host state");
        return -1;
    }

    if(hypervisor_svm_vmcb_prepare_save_state_area(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare guest state");
        return -1;
    }

    *vm_out = vm;

    return 0;
}
