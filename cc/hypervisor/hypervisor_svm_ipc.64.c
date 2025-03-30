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
#include <logging.h>

MODULE("turnstone.hypervisor.svm");

int8_t hypervisor_svm_ipc_handle_dump(hypervisor_vm_t* vm, hypervisor_ipc_message_t* message) {
    UNUSED(vm);
    UNUSED(message);
    NOTIMPLEMENTEDLOG(HYPERVISOR);
    return -1;
}

int8_t hypervisor_svm_ipc_handle_irq(hypervisor_vm_t* vm, uint8_t vector) {
    UNUSED(vm);
    UNUSED(vector);
    NOTIMPLEMENTEDLOG(HYPERVISOR);
    return -1;
}
