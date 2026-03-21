/**
 * @file acpi_aml_exec_sync.64.c
 * @brief acpi aml exec method over sync objects methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___ACPI_AML_IMPLEMENTATION 0
#include <acpi/aml_internal.h>
#include <logging.h>
#include <hashmap.h>
#include <cpu/sync.h>

MODULE("turnstone.kernel.hw.acpi");

hashmap_t* acpi_aml_mutex_map = NULL;

int8_t acpi_aml_exec_acquire(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode) {
    if(!acpi_aml_mutex_map) {
        acpi_aml_mutex_map = hashmap_string_with_heap(ctx->heap, 128);

        if(!acpi_aml_mutex_map) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to create mutex map");
            return -1;
        }
    }


    acpi_aml_object_t* mutex_obj   = opcode->operands[0];
    acpi_aml_object_t* timeout_obj = opcode->operands[1];

    if(mutex_obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "mutex operand is null for acquire opcode");
        return -1;
    }

    if(mutex_obj->type != ACPI_AML_OT_MUTEX) {
        PRINTLOG(ACPIAML, LOG_ERROR, "operand type %i is not mutex for acquire opcode", mutex_obj->type);
        return -1;
    }

    int64_t timeout = 0;

    if(timeout_obj) {
        if(acpi_aml_read_as_integer(ctx, timeout_obj, &timeout) != 0) {
            return -1;
        }
    }

    lock_t* mutex_lock = (lock_t*)hashmap_get(acpi_aml_mutex_map, mutex_obj->name);

    if(!mutex_lock) {
        mutex_lock = lock_create_with_heap(ctx->heap);

        if(!mutex_lock) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate mutex lock");
            return -1;
        }

        hashmap_put(acpi_aml_mutex_map, mutex_obj->name, mutex_lock);
    }

    PRINTLOG(ACPIAML, LOG_TRACE, "acquire mutex %s with timeout %lli", mutex_obj->name, timeout);

    lock_acquire(mutex_lock);

    acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(res == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate result object for acquire opcode");
        lock_release(mutex_lock);
        return -1;
    }

    res->type           = ACPI_AML_OT_NUMBER;
    res->number.value   = (uint64_t)-1;
    res->number.bytecnt = ctx->revision >= 2?8:4;

    opcode->return_obj = res;

    PRINTLOG(ACPIAML, LOG_TRACE, "mutex %s acquired", mutex_obj->name);

    return 0;
}

int8_t acpi_aml_exec_release(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode) {
    UNUSED(ctx);
    if(!acpi_aml_mutex_map) {
        PRINTLOG(ACPIAML, LOG_ERROR, "mutex map is not initialized for release opcode");
        return -1;
    }

    acpi_aml_object_t* mutex_obj = opcode->operands[0];

    if(mutex_obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "mutex operand is null for release opcode");
        return -1;
    }

    if(mutex_obj->type != ACPI_AML_OT_MUTEX) {
        PRINTLOG(ACPIAML, LOG_ERROR, "operand type %i is not mutex for release opcode", mutex_obj->type);
        return -1;
    }

    lock_t* mutex_lock = (lock_t*)hashmap_get(acpi_aml_mutex_map, mutex_obj->name);

    if(!mutex_lock) {
        PRINTLOG(ACPIAML, LOG_ERROR, "mutex lock not found for mutex %s in release opcode", mutex_obj->name);
        return -1;
    }

    PRINTLOG(ACPIAML, LOG_TRACE, "release mutex %s", mutex_obj->name);

    lock_release(mutex_lock);

    PRINTLOG(ACPIAML, LOG_TRACE, "mutex %s released", mutex_obj->name);

    return 0;
}

#define UNIMPLEXEC(name) \
        int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, acpi_aml_opcode_t * opcode){ \
            UNUSED(ctx); \
            PRINTLOG(ACPIAML, LOG_ERROR, "method %s for opcode 0x%04x not implemented", #name, opcode->opcode); \
            return -1; \
        }

UNIMPLEXEC(signal);
UNIMPLEXEC(wait);
UNIMPLEXEC(reset);
