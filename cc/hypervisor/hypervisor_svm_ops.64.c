/**
 * @file hypervisor_svm_ops.64.c
 * @brief SVM operations for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_svm_ops.h>
#include <logging.h>

MODULE("turnstone.hypervisor.svm");

int8_t svm_vmload(uint64_t vmcb_frame_fa) {
    // rax should be vmcb_frame_fa

    asm volatile (
        "movq %0, %%rax\n"
        "vmload\n"
        :
        : "r" (vmcb_frame_fa)
        : "rax"
        );

    return 0;
}

int8_t svm_vmsave(uint64_t vmcb_frame_fa) {
    asm volatile (
        "movq %0, %%rax\n"
        "vmsave\n"
        :
        : "r" (vmcb_frame_fa)
        : "rax"
        );

    return 0;
}

int8_t svm_vmrun(uint64_t vmcb_frame_fa) {
    asm volatile (
        "movq %0, %%rax\n"
        "vmrun\n"
        :
        : "r" (vmcb_frame_fa)
        : "rax"
        );

    return 0;
}
