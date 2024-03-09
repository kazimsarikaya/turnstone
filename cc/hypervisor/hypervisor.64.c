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
#include <hypervisor/hypervisor_vm.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <cpu/task.h>
#include <cpu/sync.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>
#include <utils.h>
#include <strings.h>

MODULE("turnstone.hypervisor");

uint64_t hypervisor_next_vm_id = 0;
lock_t* hypervisor_vm_lock = NULL;

static int32_t hypervisor_vm_task(uint64_t argc, void** args) {
    if(argc != 2) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid argument count");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "args pointer: 0x%llx", (uint64_t)args);

    uint64_t vmcs_frame_va = (uint64_t)args[0];

    char_t* entry_point_name = args[1];

    if(vmcs_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vmcs frame va");
        return -1;
    }

    if(strlen(entry_point_name) == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid entry point name");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcs frame va: 0x%llx", vmcs_frame_va);

    if(vmcs_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vmcs frame va");
        return -1;
    }

    vmcs_frame_va = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(vmcs_frame_va);


    if(hypervisor_vm_create_and_attach_to_task(vmcs_frame_va) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create vm and attach to task");
        return -1;
    }

    if(vmptrld(vmcs_frame_va) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmptrld failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmptrld success");
    PRINTLOG(HYPERVISOR, LOG_INFO, "vm (0x%llx) starting...", vmcs_frame_va);

    if(hypevisor_deploy_program(entry_point_name) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot deploy program");
        return -1;
    }

    if(vmlaunch() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmxlaunch/vmresume failed");
        hypervisor_vmcs_dump();

        return -1;
    }

    return 0;
}


int8_t hypervisor_init(void) {
    if(hypervisor_vm_lock == NULL) { // thread safe, first creator is main thread, ap threads will soon call
        hypervisor_vm_lock = lock_create();

        if(hypervisor_vm_lock == NULL) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create vm lock");
            return -1;
        }
    }

    cpu_cpuid_regs_t query;
    cpu_cpuid_regs_t result;
    query.eax = 0x1;

    cpu_cpuid(query, &result);

    if(!(result.ecx & (1 << HYPERVISOR_ECX_HYPERVISOR_BIT))) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "Hypervisor supported");

    if(hypervisor_vm_init() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot initialize hypervisor vm");
        return -1;
    }


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

    uint32_t revision_id = hypervisor_vmcs_revision_id();

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "VMCS revision id: 0x%x", revision_id);

    *(uint32_t*)vmxon_frame_va = revision_id;

    uint8_t err = 0;

    err = vmxon(vmxon_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmxon failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmxon success");


    return 0;
}

int8_t hypervisor_vm_create(const char_t* entry_point_name) {
    if(strlen(entry_point_name) == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid entry point name");
        return -1;
    }

    frame_t* vmcs_frame = NULL;

    uint64_t vmcs_frame_va = hypervisor_allocate_region(&vmcs_frame, FRAME_SIZE);

    if(vmcs_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vmcs frame");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcs frame va: 0x%llx", vmcs_frame_va);

    uint32_t revision_id = hypervisor_vmcs_revision_id();

    *(uint32_t*)vmcs_frame_va = revision_id;

    uint8_t err = 0;

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

    if(hypervisor_vmcs_prepare_vmexit_handlers() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare vmexit handlers");
        return -1;
    }

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

    void** args = memory_malloc(sizeof(void*) * 3);

    if(args == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate args");

        return -1;
    }

    args[0] = (void*)vmcs_frame_va;
    args[1] = (void*)entry_point_name;


    char_t* vm_name = sprintf("vm%08llx", ++hypervisor_next_vm_id);

    memory_heap_t* heap = memory_get_default_heap();

    if(task_create_task(heap, 2 << 20, 16 << 10, hypervisor_vm_task, 2, args, vm_name) == -1ULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create vm task");
        memory_free(args);
        memory_free(vm_name);

        return -1;
    }

    memory_free(vm_name);

    return 0;
}

int8_t hypervisor_stop(void) {
    asm volatile ("vmxoff" ::: "cc");
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmxoff success");
    return 0;
}
