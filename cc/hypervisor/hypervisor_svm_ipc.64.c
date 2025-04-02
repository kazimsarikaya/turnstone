/**
 * @file hypervisor_svm_ipc.64.c
 * @brief Hypervisor SVM IPC handler
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_ipc.h>
#include <hypervisor/hypervisor_svm_vmcb_ops.h>
#include <hypervisor/hypervisor_svm_macros.h>
#include <hypervisor/hypervisor_ept.h>
#include <logging.h>
#include <apic.h>
#include <cpu/task.h>
#include <cpu.h>

MODULE("turnstone.hypervisor.svm");

int8_t hypervisor_svm_ipc_handle_dump(hypervisor_vm_t* vm, hypervisor_ipc_message_t* message) {
    UNUSED(vm);
    UNUSED(message);
    NOTIMPLEMENTEDLOG(HYPERVISOR);
    return -1;
}

int8_t hypervisor_svm_ipc_handle_irq(hypervisor_vm_t* vm, uint8_t vector) {
    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    list_t* mq = vm->interrupt_queue;
    list_queue_pop(mq);

    if(vm->vid_enabled) {
        uint64_t apic_bp_fa = vmcb->control_area.avic_apic_backing_page_pointer;
        uint64_t apic_bp_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(apic_bp_fa);

        int32_t irr_index = vector / 32; // 32 bits per ISR register
        int32_t irr_bit = vector % 32;

        irr_index *= 0x10; // irr step is 0x10

        uint32_t* irr = (uint32_t*)(apic_bp_va + APIC_REGISTER_OFFSET_IRR0 + irr_index);

        *irr |= 1 << irr_bit;

#if 0
        cpu_write_msr(CPU_MSR_SVM_APIC_DOORBELL, 1); // not working linux sucks.
#endif

    } else {
        // vm->lapic.in_service_vector = vector;
        vmcb->control_area.vint_control.fields.v_irq = 1;
        vmcb->control_area.vint_control.fields.v_ign_tpr = 1;
        vmcb->control_area.vint_control.fields.v_intr_vector = vector;
        vmcb->control_area.clean_bits.fields.tpr = 1;
    }

    if(vm->is_halted) {
        vm->is_halt_need_next_instruction = true;
    }

    return 0;
}
