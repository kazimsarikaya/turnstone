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
#include <linker.h>

MODULE("turnstone.kernel.programs.tosdb_manager");

int32_t tosdb_manager_main(int32_t argc, char_t** argv);

static boolean_t tosdb_manager_is_initialized = false;

static hashmap_t* tosdb_manager_deployed_modules = NULL;

static buffer_t* tosdb_manager_global_offset_table_buffer = NULL;
static hashmap_t* tosdb_manager_got_symbol_index_map = NULL;

static uint64_t tosdb_manager_clone_global_offset_table(uint64_t* got_return_size) {
    uint64_t got_buffer_size = buffer_get_length(tosdb_manager_global_offset_table_buffer);
    uint64_t got_size = got_buffer_size;

    if(got_size % FRAME_SIZE != 0) {
        got_size += FRAME_SIZE - (got_size % FRAME_SIZE);
    }

    frame_t* got_dump_frame = NULL;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
                                                       got_size / FRAME_SIZE,
                                                       FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK,
                                                       &got_dump_frame, NULL) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate region frame");

        return -1;
    }

    uint64_t got_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(got_dump_frame->frame_address);

    if(memory_paging_add_va_for_frame(got_va, got_dump_frame, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot map region frame");

        return -1;
    }

    memory_memclean((void*)got_va, got_size);

    uint8_t* got = buffer_get_view_at_position(tosdb_manager_global_offset_table_buffer, 0, got_buffer_size);

    memory_memcopy(got, (void*)got_va, got_buffer_size);

    uint64_t got_physical_address = got_dump_frame->frame_address;

    *got_return_size = got_size;

    return got_physical_address;
}

static void tosdb_manger_build_module(tosdb_t* tdb, tosdb_manager_ipc_t* ipc, uint64_t mod_id, uint64_t sym_id) {
    int8_t exit_code = 0;

    tosdb_database_t* db_system = tosdb_database_create_or_open(tdb, "system");

    if(!db_system) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open system database");

        exit_code = -1;
        goto exit;
    }

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db_system, "modules", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);


    if(!tbl_sections || !tbl_modules || !tbl_symbols || !tbl_relocations) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open system tables");

        exit_code = -1;
        goto exit;
    }



    PRINTLOG(LINKER, LOG_DEBUG, "module id: 0x%llx", mod_id);

    tosdb_manager_deployed_module_t* deployed_module = (tosdb_manager_deployed_module_t*)hashmap_get(tosdb_manager_deployed_modules, (void*)mod_id);

    if(deployed_module) {
        uint64_t got_size = 0;
        uint64_t got_physical_address = tosdb_manager_clone_global_offset_table(&got_size);

        if(got_physical_address == -1ULL) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot clone got");

            exit_code = -1;
            goto exit;
        }

        ipc->program_build.program_handle = mod_id;
        ipc->program_build.program_dump_frame_address = deployed_module->program_dump_frame_address;
        ipc->program_build.program_size = deployed_module->program_size;
        ipc->program_build.program_physical_address = deployed_module->program_physical_address;
        ipc->program_build.program_virtual_address = deployed_module->program_virtual_address;
        ipc->program_build.program_entry_point_virtual_address = deployed_module->program_entry_point_virtual_address;
        ipc->program_build.got_physical_address = got_physical_address;
        ipc->program_build.got_size = got_size;
        ipc->program_build.metadata_physical_address = deployed_module->metadata_physical_address;
        ipc->program_build.metadata_size = deployed_module->metadata_size;

        ipc->is_response_success = (exit_code == 0);
        ipc->is_response_done = true;
        task_set_interrupt_received(ipc->sender_task_id);

        return;
    }

    linker_context_t* ctx = memory_malloc(sizeof(linker_context_t));

    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate linker context");

        exit_code = -1;
        goto exit;
    }

    ctx->for_hypervisor_application = ipc->program_build.for_vm;
    ctx->entrypoint_symbol_id = sym_id;
    ctx->tdb = tdb;
    ctx->modules = hashmap_integer(16);
    ctx->got_table_buffer = tosdb_manager_global_offset_table_buffer;
    ctx->got_symbol_index_map = tosdb_manager_got_symbol_index_map;

    if(!tosdb_manager_global_offset_table_buffer) {
        tosdb_manager_got_symbol_index_map = hashmap_integer(1024);

        if(!tosdb_manager_got_symbol_index_map) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot allocate got symbol index map");

            exit_code = -1;
            goto exit_with_destroy_context;
        }

        ctx->got_symbol_index_map = tosdb_manager_got_symbol_index_map;


        tosdb_manager_global_offset_table_buffer = buffer_new();
        ctx->got_table_buffer = tosdb_manager_global_offset_table_buffer;

        if(!tosdb_manager_global_offset_table_buffer) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot allocate got buffer");

            exit_code = -1;
            goto exit_with_destroy_context;
        }

        linker_global_offset_table_entry_t empty_got_entry = {0};

        buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // null entry
        buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // got itself
    }

    if(linker_build_module(ctx, mod_id, !ctx->for_hypervisor_application) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot build module");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "modules built");

    if(linker_calculate_program_size(ctx) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot calculate program size");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    uint64_t total_program_size = ctx->program_size + ctx->metadata_size;

    frame_t* program_dump_frame = NULL;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
                                                       total_program_size / FRAME_SIZE,
                                                       FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK,
                                                       &program_dump_frame, NULL) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate region frame");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    uint64_t program_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(program_dump_frame->frame_address);

    if(memory_paging_add_va_for_frame(program_va, program_dump_frame, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot map region frame");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    memory_memclean((void*)program_va, total_program_size);

    ctx->program_start_physical = program_dump_frame->frame_address;
    ctx->program_start_virtual = program_dump_frame->frame_address;

    if(linker_bind_linear_addresses(ctx) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot bind addresses");

        exit_code = -1;
        goto exit_with_destroy_context;
    }


    if(linker_bind_got_entry_values(ctx) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot bind got entry values");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    if(linker_link_program(ctx) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot link program");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    uint64_t vm_program_size = ctx->program_size + ctx->metadata_size;
    uint8_t* vm_program = (uint8_t*)program_va;

    if(linker_dump_program_to_array(ctx,
                                    LINKER_PROGRAM_DUMP_TYPE_METADATA |
                                    LINKER_PROGRAM_DUMP_TYPE_CODE,
                                    vm_program) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot dump program");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    ctx->got_table_buffer = NULL; // do not free got table buffer
    ctx->got_symbol_index_map = NULL; // do not free got symbol index map

    uint64_t got_size = 0;
    uint64_t got_physical_address = tosdb_manager_clone_global_offset_table(&got_size);

    if(got_physical_address == -1ULL) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot clone got");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "program dump frame address: 0x%llx", program_dump_frame->frame_address);
    PRINTLOG(LINKER, LOG_DEBUG, "program physical address: 0x%llx", ctx->program_start_physical);
    PRINTLOG(LINKER, LOG_DEBUG, "program total size: 0x%llx", vm_program_size);
    PRINTLOG(LINKER, LOG_DEBUG, "program size: 0x%llx", ctx->program_size);
    PRINTLOG(LINKER, LOG_DEBUG, "entry point virtual address: 0x%llx", ctx->entrypoint_address_virtual);
    PRINTLOG(LINKER, LOG_DEBUG, "got physical address: 0x%llx", got_physical_address);
    PRINTLOG(LINKER, LOG_DEBUG, "metadata physical address: 0x%llx", ctx->metadata_address_physical);

    deployed_module = memory_malloc(sizeof(tosdb_manager_deployed_module_t));

    if(!deployed_module) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate deployed module");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    deployed_module->program_handle = mod_id;
    deployed_module->program_dump_frame_address = program_dump_frame->frame_address;
    deployed_module->program_size = ctx->program_size;
    deployed_module->program_physical_address = ctx->program_start_physical;
    deployed_module->program_virtual_address = ctx->program_start_virtual;
    deployed_module->program_entry_point_virtual_address = ctx->entrypoint_address_virtual;
    deployed_module->metadata_physical_address = ctx->metadata_address_physical;
    deployed_module->metadata_size = ctx->metadata_size;

    hashmap_put(tosdb_manager_deployed_modules, (void*)mod_id, deployed_module);

    ipc->program_build.program_handle = deployed_module->program_handle;
    ipc->program_build.program_dump_frame_address = deployed_module->program_dump_frame_address;
    ipc->program_build.program_size = deployed_module->program_size;
    ipc->program_build.program_physical_address = deployed_module->program_physical_address;
    ipc->program_build.program_virtual_address = deployed_module->program_virtual_address;
    ipc->program_build.program_entry_point_virtual_address = deployed_module->program_entry_point_virtual_address;
    ipc->program_build.got_physical_address = got_physical_address;
    ipc->program_build.got_size = got_size;
    ipc->program_build.metadata_physical_address = deployed_module->metadata_physical_address;
    ipc->program_build.metadata_size = deployed_module->metadata_size;

exit_with_destroy_context:
    linker_destroy_context(ctx);
exit:
    ipc->is_response_success = (exit_code == 0);
    ipc->is_response_done = true;
    task_set_interrupt_received(ipc->sender_task_id);
}

static void tosdb_manger_build_program(tosdb_t* tdb, tosdb_manager_ipc_t* ipc) {
    // logging_module_levels[LINKER] = LOG_DEBUG;

    int8_t exit_code = 0;

    tosdb_database_t* db_system = tosdb_database_create_or_open(tdb, "system");

    if(!db_system) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open system database");

        exit_code = -1;
        goto exit;
    }

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db_system, "modules", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);


    if(!tbl_sections || !tbl_modules || !tbl_symbols || !tbl_relocations) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open system tables");

        exit_code = -1;
        goto exit;
    }

    tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

    if(!s_sym_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create record for search");

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->set_string(s_sym_rec, "name", ipc->program_build.entry_point_name);

    list_t* found_symbols = s_sym_rec->search_record(s_sym_rec);

    if(!found_symbols) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->destroy(s_sym_rec);

    if(list_size(found_symbols) == 0) {
        PRINTLOG(LINKER, LOG_ERROR, "entrypoint symbol not found: %s", ipc->program_build.entry_point_name);
        list_destroy(found_symbols);

        exit_code = -1;
        goto exit;
    }

    s_sym_rec = (tosdb_record_t*)list_queue_pop(found_symbols);
    list_destroy(found_symbols);

    if(!s_sym_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get record for entrypoint symbol");

        exit_code = -1;
        goto exit;
    }

    uint64_t sec_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "section_id", (int64_t*)&sec_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get section id for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    uint64_t sym_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "id", (int64_t*)&sym_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol id for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->destroy(s_sym_rec);


    PRINTLOG(LINKER, LOG_DEBUG, "entrypoint symbol %s id 0x%llx section id: 0x%llx",
             ipc->program_build.entry_point_name, sym_id, sec_id);


    tosdb_record_t* s_sec_rec = tosdb_table_create_record(tbl_sections);

    if(!s_sec_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create record for search");

        exit_code = -1;
        goto exit;
    }

    s_sec_rec->set_int64(s_sec_rec, "id", sec_id);

    if(!s_sec_rec->get_record(s_sec_rec)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get record for search");

        exit_code = -1;
        goto exit;
    }

    uint64_t mod_id = 0;

    if(!s_sec_rec->get_int64(s_sec_rec, "module_id", (int64_t*)&mod_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get module id for entrypoint symbol");

        s_sec_rec->destroy(s_sec_rec);

        exit_code = -1;
        goto exit;
    }

    s_sec_rec->destroy(s_sec_rec);

    PRINTLOG(LINKER, LOG_TRACE, "module id: 0x%llx", mod_id);

    return tosdb_manger_build_module(tdb, ipc, mod_id, sym_id);

exit:
    ipc->is_response_success = (exit_code == 0);
    ipc->is_response_done = true;
    task_set_interrupt_received(ipc->sender_task_id);
}

int32_t tosdb_manager_main(int32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    if(tosdb_manager_is_initialized) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_main: already initialized");
        return -1;
    }

    if(!tosdb_manager_deployed_modules) {
        tosdb_manager_deployed_modules = hashmap_integer(128);
    }

    // logging_module_levels[AHCI] = LOG_TRACE;

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

    PRINTLOG(TOSDB, LOG_INFO, "ahci and nvme initialized");

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

    PRINTLOG(TOSDB, LOG_DEBUG, "GPT disk opened");
    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB partition searching");

    efi_guid_t kernel_guid = EFI_PART_TYPE_TURNSTONE_TOSDB_PART_GUID;

    disk_or_partition_t* tosdb_part = (disk_or_partition_t*)sata0->get_partition_by_type_data(sata0, &kernel_guid);

    if (tosdb_part == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "TOSDB partition not found");
        return -1;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB partition found");
    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB backend creating");

    tosdb_backend_t* tosdb_backend = tosdb_backend_disk_new(tosdb_part);

    if (tosdb_backend == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "Failed to create TOSDB backend");
        return -1;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB backend created");
    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB database opening");

    tosdb_t* tdb = tosdb_new(tosdb_backend, COMPRESSION_TYPE_NONE);

    if (tdb == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "Failed to create TOSDB instance");
        return -1;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB database opened");
    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB cache config setting");

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 2 << 20;
    cc.index_data_size = 4 << 20;
    cc.secondary_index_data_size = 4 << 20;
    cc.valuelog_size = 16 << 20;

    if(!tosdb_cache_config_set(tdb, &cc)) {
        PRINTLOG(TOSDB, LOG_ERROR, "Failed to set cache config");
        return -1;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB cache config set");
    PRINTLOG(TOSDB, LOG_DEBUG, "TOSDB defalut databases and tables openning");

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

    PRINTLOG(TOSDB, LOG_INFO, "TOSDB defalut databases and tables opened");

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
            PRINTLOG(TOSDB, LOG_DEBUG, "tosdb_manager_main: waiting for message");
            task_set_message_waiting();
            task_yield();

            continue;
        }

        PRINTLOG(TOSDB, LOG_DEBUG, "tosdb_manager_main: processing message");

        tosdb_manager_ipc_t* ipc = (tosdb_manager_ipc_t*)list_queue_pop(mq_list);

        if(ipc == NULL) {
            PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_main: cannot pop message");
            continue;
        }

        switch(ipc->type) {
        case TOSDB_MANAGER_IPC_TYPE_CLOSE:
            PRINTLOG(TOSDB, LOG_DEBUG, "tosdb_manager_main: received close message");
            tosdb_manager_main_close = true;
            break;
        case TOSDB_MANAGER_IPC_TYPE_PROGRAM_LOAD:
            PRINTLOG(TOSDB, LOG_DEBUG, "tosdb_manager_main: received program load message");
            tosdb_manger_build_program(tdb, ipc);
            break;
        case TOSDB_MANAGER_IPC_TYPE_MODULE_LOAD:
            PRINTLOG(TOSDB, LOG_DEBUG, "tosdb_manager_main: received program load message");
            tosdb_manger_build_module(tdb, ipc, ipc->program_build.program_handle, -1);
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

static uint64_t tosdb_manager_task_id = 0;

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

int8_t tosdb_manager_clear(void) {
    if(!tosdb_manager_is_initialized) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot clear not initialized manager");
        return -1;
    }

    tosdb_manager_is_initialized = false;
    tosdb_manager_task_id = 0;
    tosdb_manager_deployed_modules = NULL;

    return 0;
}

int8_t tosdb_manager_ipc_send_and_wait(tosdb_manager_ipc_t* ipc) {
    if(!tosdb_manager_is_initialized) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb_manager_close: not initialized");
        return -1;
    }

    if(tosdb_manager_task_id == 0) {
        return -1;
    }

    ipc->sender_task_id = task_get_id();

    list_t* ipc_mq = task_get_message_queue(tosdb_manager_task_id, 0);

    if(ipc_mq == NULL) {
        return -1;
    }

    list_queue_push(ipc_mq, ipc);

    while(!ipc->is_response_done) {
        task_set_message_waiting();
        task_yield();
    }

    return 0;
}
