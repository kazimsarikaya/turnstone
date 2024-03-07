/**
 * @file tosdb_manager.64.c
 * @brief TOSDB Manager Program
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb_manager.h>
#include <tosdb/tosdb.h>
#include <driver/ahci.h>
#include <driver/nvme.h>
#include <disk.h>
#include <efi.h>
#include <logging.h>
#include <cpu/task.h>
#include <memory.h>

MODULE("turnstone.kernel.programs.tosdb_manager");

int32_t tosdb_manager_main(int32_t argc, char_t** argv);

boolean_t tosdb_manager_is_initialized = false;


int32_t tosdb_manager_main(int32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    if(tosdb_manager_is_initialized) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_main: already initialized");
        return -1;
    }

    memory_heap_t* heap = memory_get_heap(NULL);

    PRINTLOG(KERNEL, LOG_INFO, "Initializing ahci and nvme");
    int8_t sata_port_cnt = ahci_init(heap, PCI_CONTEXT->sata_controllers);
    int8_t nvme_port_cnt = nvme_init(heap, PCI_CONTEXT->nvme_controllers);

    if(sata_port_cnt == -1) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init ahci. Halting...");
        return -1;
    } else {
        PRINTLOG(KERNEL, LOG_INFO, "sata port count is %i", sata_port_cnt);
    }

    if(nvme_port_cnt == -1) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init nvme. Halting...");
        return -1;
    } else {
        PRINTLOG(KERNEL, LOG_INFO, "nvme port count is %i", nvme_port_cnt);
    }

    ahci_sata_disk_t* sd = (ahci_sata_disk_t*)ahci_get_first_inserted_disk();

    if (sd == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "No SATA disk found");
        return -1;
    }


    disk_t* sata0 = gpt_get_or_create_gpt_disk(ahci_disk_impl_open(sd));

    if (sata0 == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "Failed to get or create GPT disk");
        return -1;
    }

    efi_guid_t kernel_guid = EFI_PART_TYPE_TURNSTONE_TOSDB_PART_GUID;

    disk_or_partition_t* tosdb_part = (disk_or_partition_t*)sata0->get_partition_by_type_data(sata0, &kernel_guid);

    if (tosdb_part == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "TOSDB partition not found");
        return -1;
    }

    tosdb_backend_t* tosdb_backend = tosdb_backend_disk_new(tosdb_part);

    if (tosdb_backend == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "Failed to create TOSDB backend");
        return -1;
    }

    tosdb_t* tdb = tosdb_new(tosdb_backend, COMPRESSION_TYPE_NONE);

    if (tdb == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "Failed to create TOSDB instance");
        return -1;
    }

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 2 << 20;
    cc.index_data_size = 4 << 20;
    cc.secondary_index_data_size = 4 << 20;
    cc.valuelog_size = 16 << 20;

    if(!tosdb_cache_config_set(tdb, &cc)) {
        PRINTLOG(TOSDB, LOG_ERROR, "Failed to set cache config");
        return -1;
    }

    tosdb_database_t* db_system = tosdb_database_create_or_open(tdb, "system");

    if(!db_system) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot open system database");

        return -1;
    }

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db_system, "modules", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);


    if(!tbl_sections || !tbl_modules || !tbl_symbols || !tbl_relocations) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot open system tables");

        return -1;
    }

    list_t* mq_list = list_create_queue();

    if(mq_list == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create message queue");
        return -1;
    }

    task_add_message_queue(mq_list);

    tosdb_manager_is_initialized = true;

    boolean_t tosdb_manager_main_close = false;

    while(!tosdb_manager_main_close) {
        if(list_size(mq_list) == 0) {
            PRINTLOG(TOSDB, LOG_INFO, "tosdb_manager_main: waiting for message");
            task_set_message_waiting();
            task_yield();

            continue;
        }

        PRINTLOG(TOSDB, LOG_INFO, "tosdb_manager_main: processing message");

        tosdb_manager_ipc_t* ipc = (tosdb_manager_ipc_t*)list_queue_pop(mq_list);

        if(ipc == NULL) {
            PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_main: cannot pop message");
            continue;
        }

        switch(ipc->type) {
        case TOSDB_MANAGER_IPC_TYPE_CLOSE:
            PRINTLOG(TOSDB, LOG_INFO, "tosdb_manager_main: received close message");
            tosdb_manager_main_close = true;
            break;
        default:
            PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_main: unknown message type");
            break;
        }
    }

    tosdb_close(tdb);

    tosdb_manager_is_initialized = false;

    return 0;
}

uint64_t tosdb_manager_task_id = 0;

int8_t tosdb_manager_init(void) {
    if(tosdb_manager_is_initialized) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_init: already initialized");
        return -1;
    }

    memory_heap_t* heap = memory_get_default_heap();

    tosdb_manager_task_id = task_create_task(heap, 256 << 20, 2 << 20, tosdb_manager_main, 0, NULL, "tosdb_manager");
    return tosdb_manager_task_id == -1ULL ? -1 : 0;
}

const tosdb_manager_ipc_t tosdb_manager_close_ipc = {
    .type = TOSDB_MANAGER_IPC_TYPE_CLOSE
};

int8_t tosdb_manager_close(void) {
    if(!tosdb_manager_is_initialized) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_close: not initialized");
        return -1;
    }

    if(tosdb_manager_task_id == 0) {
        return -1;
    }

    list_t* ipc_mq = task_get_message_queue(tosdb_manager_task_id, 0);

    if(ipc_mq == NULL) {
        return -1;
    }

    list_queue_push(ipc_mq, &tosdb_manager_close_ipc);

    return 0;
}
