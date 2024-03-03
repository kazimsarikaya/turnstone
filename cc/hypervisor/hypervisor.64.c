/**
 * @file hypervisor.64.c
 * @brief Hypervisor for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor.h>
#include <hypervisor/hypervisor_macros.h>
#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_vmcsops.h>
#include <hypervisor/hypervisor_vmxops.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <cpu/task.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>

MODULE("turnstone.hypervisor");


uint32_t hypervisor_vmcs_revision_id(void) {
    return cpu_read_msr(CPU_MSR_IA32_VMX_BASIC) & 0xffffffff;
}



int8_t hypervisor_init(void) {
    cpu_cpuid_regs_t query;
    cpu_cpuid_regs_t result;
    query.eax = 0x1;

    cpu_cpuid(query, &result);

    if(!(result.ecx & (1 << HYPERVISOR_ECX_HYPERVISOR_BIT))) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "Hypervisor supported");


    cpu_reg_cr4_t cr4 = cpu_read_cr4();

    cr4.fields.vmx_enable = 1;

    cpu_write_cr4(cr4);

    uint64_t feature_control = cpu_read_msr(CPU_MSR_IA32_FEATURE_CONTROL);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "Feature control: 0x%llx", feature_control);

    uint64_t required = FEATURE_CONTROL_LOCKED | FEATURE_CONTROL_VMXON_OUTSIDE_SMX;

    if((feature_control & required) != required) {
        feature_control |= required;
        cpu_write_msr(CPU_MSR_IA32_FEATURE_CONTROL, feature_control);
    }

    cpu_reg_cr0_t cr0 = cpu_read_cr0();

    cr0.bits &= cpu_read_msr(CPU_MSR_IA32_VMX_CR0_FIXED1);
    cr0.bits |= cpu_read_msr(CPU_MSR_IA32_VMX_CR0_FIXED0);

    cpu_write_cr0(cr0);

    cr4 = cpu_read_cr4();

    cr4.bits &= cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED0);
    cr4.bits |= cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED1);

    cpu_write_cr4(cr4);

    frame_t* vmxon_frame = NULL;

    uint64_t vmxon_frame_va = hypervisor_allocate_region(&vmxon_frame, FRAME_SIZE);

    if(vmxon_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vmxon frame");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmxon frame va: 0x%llx", vmxon_frame_va);

    frame_t* vmcs_frame = NULL;

    uint64_t vmcs_frame_va = hypervisor_allocate_region(&vmcs_frame, FRAME_SIZE);

    if(vmcs_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vmcs frame");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcs frame va: 0x%llx", vmcs_frame_va);


    uint32_t vmcs_revision_id = hypervisor_vmcs_revision_id();

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "VMCS revision id: 0x%x", vmcs_revision_id);

    *(uint32_t*)vmcs_frame_va = vmcs_revision_id;
    *(uint32_t*)vmxon_frame_va = vmcs_revision_id;

    uint8_t err = 0;


    err = vmxon(vmxon_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmxon failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmxon success");

    err = vmclear(vmcs_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmclear failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmclear success");

    err = vmptrld(vmcs_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmptrld failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmptrld success");

    if(hypervisor_vmcs_prepare_host_state() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare host state");
        return -1;
    }

    if(hypervisor_vmcs_prepare_guest_state() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare guest state");
        return -1;
    }

    if(hypervisor_vmcs_prepare_execution_control() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare execution control");
        return -1;
    }

    if(hypervisor_vmcs_prepare_vm_exit_and_entry_control() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare vm exit control");
        return -1;
    }

    if(hypervisor_vmcs_prepare_ept() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare ept");
        return -1;
    }

    if(vmlaunch() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmxlaunch failed");
        hypervisor_vmcs_dump();
        return -1;
    }

    return 0;
}

int8_t hypervisor_stop(void) {
    asm volatile ("vmxoff" ::: "cc");
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmxoff success");
    return 0;
}
