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
#include <hypervisor/hypervisor_vmcsops.h>
#include <hypervisor/hypervisor_vm.h>

typedef enum hypervisor_ipc_message_type_t {
    HYPERVISOR_IPC_MESSAGE_TYPE_UNKNOWN = 0,
    HYPERVISOR_IPC_MESSAGE_TYPE_DUMP = 1,
    HYPERVISOR_IPC_MESSAGE_TYPE_TIMER_INT = 2,
} hypervisor_ipc_message_type_t;

typedef struct hypervisor_ipc_message_t {
    hypervisor_ipc_message_type_t message_type;
    buffer_t*                     message_data;
    volatile boolean_t            message_data_completed;
} hypervisor_ipc_message_t;

int8_t hypervisor_vmcs_check_ipc(vmcs_vmexit_info_t* vmexit_info);

void hypervisor_ipc_send_timer_interrupt(hypervisor_vm_t* vm);
#endif
