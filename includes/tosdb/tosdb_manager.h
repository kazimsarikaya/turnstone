/**
 * @file tosdb_manager.h
 * @brief TOSDB Manager Program Header
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___TOSDB_MANAGER_H
#define ___TOSDB_MANAGER_H 0

#include <types.h>
#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t tosdb_manager_init(void);

typedef enum tosdb_manager_ipc_type_t {
    TOSDB_MANAGER_IPC_TYPE_NONE = 0,
    TOSDB_MANAGER_IPC_TYPE_CLOSE,
    TOSDB_MANAGER_IPC_TYPE_PROGRAM_LOAD,
    TOSDB_MANAGER_IPC_TYPE_MODULE_LOAD,
} tosdb_manager_ipc_type_t;

typedef struct tosdb_manager_deployed_module_t {
    uint64_t module_handle;
    uint64_t module_physical_address;
    uint64_t module_virtual_address;
    uint64_t module_size;
    uint64_t metadata_physical_address;
    uint64_t metadata_virtual_address;
    uint8_t  metadata_size;
} tosdb_manager_deployed_module_t;

typedef struct tosdb_manager_ipc_t {
    tosdb_manager_ipc_type_t type;
    uint64_t                 sender_task_id;
    buffer_t*                response_buffer;
    boolean_t                is_response_done;
    boolean_t                is_response_success;

    union {
        struct {
            const char_t*                   entry_point_name;
            boolean_t                       for_vm;
            uint64_t                        program_entry_point_virtual_address;
            tosdb_manager_deployed_module_t module;
            uint64_t                        got_physical_address;
            uint64_t                        got_size;
        } program_build;
    };

} tosdb_manager_ipc_t;

int8_t tosdb_manager_close(void);
int8_t tosdb_manager_clear(void);
int8_t tosdb_manager_ipc_send_and_wait(tosdb_manager_ipc_t* ipc);

#ifdef __cplusplus
}
#endif

#endif
