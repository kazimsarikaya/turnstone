/**
 * @file hypervisor_ipc.h
 * @brief defines hypervisor related structures and functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_IPC_H
#define ___HYPERVISOR_IPC_H 0

#include <types.h>
#include <buffer.h>
#include <hypervisor/hypervisor_vm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hypervisor_ipc_message_type_t {
    HYPERVISOR_IPC_MESSAGE_TYPE_UNKNOWN = 0,
    HYPERVISOR_IPC_MESSAGE_TYPE_DUMP = 1,
    HYPERVISOR_IPC_MESSAGE_TYPE_TIMER_INT = 2,
    HYPERVISOR_IPC_MESSAGE_TYPE_CLOSE = 3,
} hypervisor_ipc_message_type_t;

typedef struct hypervisor_ipc_message_t {
    hypervisor_ipc_message_type_t message_type;
    buffer_t*                     message_data;
    volatile boolean_t            message_data_completed;
} hypervisor_ipc_message_t;

int8_t hypervisor_check_ipc(hypervisor_vm_t* vm);

void   hypervisor_ipc_send_timer_interrupt(hypervisor_vm_t* vm);
int8_t hypervisor_ipc_send_close(uint64_t vm_id);

int8_t hypervisor_vmx_ipc_handle_dump(hypervisor_vm_t* vm, hypervisor_ipc_message_t* message);
int8_t hypervisor_vmx_ipc_handle_irq(hypervisor_vm_t* vm, uint8_t vector);


int8_t hypervisor_svm_ipc_handle_dump(hypervisor_vm_t* vm, hypervisor_ipc_message_t* message);
int8_t hypervisor_svm_ipc_handle_irq(hypervisor_vm_t* vm, uint8_t vector);

#ifdef __cplusplus
}
#endif

#endif
