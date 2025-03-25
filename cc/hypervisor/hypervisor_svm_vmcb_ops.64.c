/**
 * @file hypervisor_svm_vmcb_ops.64.c
 * @brief SVM vmcb operations for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_svm_vmcb_ops.h>
#include <logging.h>

MODULE("turnstone.hypervisor.svm");


int8_t hypervisor_svm_vmcb_prepare(hypervisor_vm_t** vm_out) {
    if(vm_out == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vm out pointer");
        return -1;
    }

    hypervisor_vm_t* vm = (hypervisor_vm_t*)memory_malloc(sizeof(hypervisor_vm_t));

    if(vm == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vm");
        return -1;
    }



    *vm_out = vm;

    return 0;
}
