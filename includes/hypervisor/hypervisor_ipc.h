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

typedef enum hypervisor_ipc_message_type_t {
    HYPERVISOR_IPC_MESSAGE_TYPE_UNKNOWN = 0,
    HYPERVISOR_IPC_MESSAGE_TYPE_VM_CALL = 1,
} hypervisor_ipc_message_type_t;

typedef struct hypervisor_ipc_message_t {
    hypervisor_ipc_message_type_t message_type;
    buffer_t*                     message_data;
    volatile boolean_t            message_data_completed;
} hypervisor_ipc_message_t;

#endif
