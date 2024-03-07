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

int8_t tosdb_manager_init(void);

typedef enum tosdb_manager_ipc_type_t
{
    TOSDB_MANAGER_IPC_TYPE_NONE = 0,
    TOSDB_MANAGER_IPC_TYPE_CLOSE,
    TOSDB_MANAGER_IPC_TYPE_PROGRAM_BUILD,
} tosdb_manager_ipc_type_t;

typedef struct tosdb_manager_ipc_t
{
    tosdb_manager_ipc_type_t type;
    buffer_t*                response_buffer;
    boolean_t                is_response_done;
    boolean_t                is_response_success;

    union {
        struct {
            const char_t* entry_point_name;
            boolean_t     for_vm;
            uint64_t      program_handle;
            uint8_t*      program;
        } program_build;
    };

} tosdb_manager_ipc_t;

int8_t tosdb_manager_close(void);

#endif
