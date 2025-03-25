/**
 * @file hypervisor_svm_ops.h
 * @brief defines hypervisor svm (amd) related operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_SVM_OPS_H
#define ___HYPERVISOR_SVM_OPS_H 0

#include <types.h>
#include <utils.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t svm_vmload(uint64_t vmcb_frame_fa);
int8_t svm_vmsave(uint64_t vmcb_frame_fa);
int8_t svm_vmrun(uint64_t vmcb_frame_fa);

#ifdef __cplusplus
}
#endif

#endif
