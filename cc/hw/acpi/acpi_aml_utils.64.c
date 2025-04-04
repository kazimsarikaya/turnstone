/**
 * @file acpi_aml_utils.64.c
 * @brief acpi parser utils
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <acpi/aml_internal.h>
#include <strings.h>
#include <logging.h>
#include <utils.h>
#include <memory/paging.h>
#include <pci.h>
#include <ports.h>
#include <bplustree.h>
#include <stdbufs.h>

MODULE("turnstone.kernel.hw.acpi");


int8_t acpi_aml_write_sysio_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, const acpi_aml_object_t* obj);
int8_t acpi_aml_write_pci_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, const acpi_aml_object_t* obj);
int8_t acpi_aml_write_memory_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, const acpi_aml_object_t* obj);
int8_t acpi_aml_read_sysio_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res);
int8_t acpi_aml_read_pci_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res);
int8_t acpi_aml_read_memory_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res);

int8_t acpi_aml_is_null_target(acpi_aml_object_t* obj) {
    if(obj == NULL) {
        return 0;
    }

    if(obj->name == NULL && obj->type == ACPI_AML_OT_NUMBER && obj->number.value == 0) {
        return 0;
    }

    return -1;
}

acpi_aml_object_t* acpi_aml_get_if_arg_local_obj(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj, uint8_t write, uint8_t copy) {
    if(obj && obj->type == ACPI_AML_OT_LOCAL_OR_ARG) {
        if(ctx->method_context == NULL) {
            return NULL;
        }

        acpi_aml_method_context_t* mthctx = ctx->method_context;

        uint8_t laidx = obj->local_or_arg.idx_local_or_arg;

        if(laidx > 14) {
            acpi_aml_print_object(ctx, obj);
            PRINTLOG(ACPIAML, LOG_FATAL, "local/arg index out of bounds %x", laidx);
            return NULL;
        }

        acpi_aml_object_t* la_obj = mthctx->mthobjs[laidx];

        if(la_obj == NULL && laidx <= 7) { // if localX does not exists create it
            PRINTLOG(ACPIAML, LOG_TRACE, "----- creating local arg %i", laidx);
            la_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

            if(la_obj == NULL) {
                return NULL;
            }

            la_obj->type = ACPI_AML_OT_UNINITIALIZED;
            mthctx->mthobjs[laidx] = la_obj;
            PRINTLOG(ACPIAML, LOG_TRACE, "----- new local arg %i for read %p", laidx, la_obj);
        } else if(la_obj != NULL && laidx <= 7 && write) {
            acpi_aml_destroy_object(ctx, la_obj);
            la_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

            if(la_obj == NULL) {
                return NULL;
            }

            la_obj->type = ACPI_AML_OT_UNINITIALIZED;
            mthctx->mthobjs[laidx] = la_obj;
            PRINTLOG(ACPIAML, LOG_TRACE, "----- new local arg %i for write %p", laidx, la_obj);
        }

        if(la_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_FATAL, "local/arg does not exists %x", laidx);
            return NULL;
        }

        if(laidx >= 8) {
            if(write && (mthctx->dirty_args[laidx - 8] == 0 || copy == 1)) {
                la_obj = acpi_aml_duplicate_object(ctx, la_obj);
                memory_free_ext(ctx->heap, la_obj->name);
                la_obj->name = NULL;

                if(mthctx->mthobjs[laidx]->name == NULL) {
                    acpi_aml_destroy_object(ctx, mthctx->mthobjs[laidx]);
                }

                mthctx->mthobjs[laidx] = la_obj;
                mthctx->dirty_args[laidx - 8] = 1;
            }

            if(write == 0 && copy == 1 && mthctx->dirty_args[laidx - 8] == 0) {
                PRINTLOG(ACPIAML, LOG_WARNING, "read copy");
                la_obj = acpi_aml_duplicate_object(ctx, la_obj);
                memory_free_ext(ctx->heap, la_obj->name);
                la_obj->name = NULL;
                mthctx->dirty_args[laidx - 8] = 1;
            }
        }

        obj = la_obj;
        PRINTLOG(ACPIAML, LOG_TRACE, "----- refered local arg %i  %p", laidx, la_obj);
    }
    return obj;
}

int8_t acpi_aml_write_sysio_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, const acpi_aml_object_t* obj) {
    UNUSED(ctx);

    if(obj == NULL || obj->type != ACPI_AML_OT_FIELD || obj->field.related_object == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "Field or region is null %i", obj == NULL?0:1);
        return -1;
    }

    acpi_aml_object_t* opregion = NULL;
    boolean_t indexedfield = 0;

    if(obj->field.related_object->type == ACPI_AML_OT_OPREGION  &&
       obj->field.related_object->opregion.region_space == ACPI_AML_OPREGT_SYSIO) {
        opregion = obj->field.related_object;
    } else if(obj->field.related_object->type == ACPI_AML_OT_FIELD &&
              obj->field.related_object->field.related_object->type == ACPI_AML_OT_OPREGION  &&
              obj->field.related_object->field.related_object->opregion.region_space == ACPI_AML_OPREGT_SYSIO) {
        opregion = obj->field.related_object->field.related_object;
        indexedfield = 1;
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "op region space is not sysio %i", obj->field.related_object->opregion.region_space);
        return -1;
    }

    uint64_t offset = 0;
    uint64_t field_offset = 0;
    uint8_t access_type = 0;
    uint8_t update_rule = 0;
    uint64_t sizeasbit = 0;

    if(indexedfield) {
        if(acpi_aml_write_sysio_as_integer(ctx, obj->field.offset / 8, obj->field.related_object) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "cannot select indexed field");
            return -1;
        }

        offset = obj->field.selector_object->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.selector_object->field.offset;
        access_type = obj->field.selector_object->field.access_type;
        update_rule = obj->field.selector_object->field.update_rule;
        sizeasbit = obj->field.selector_object->field.sizeasbit;
    } else {
        offset = obj->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.offset;
        access_type = obj->field.access_type;
        update_rule = obj->field.update_rule;
        sizeasbit = obj->field.sizeasbit;
    }

    uint64_t tmp = 0;
    uint64_t access_len = 0;

    switch (access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        access_len = 8;
        tmp = inb(offset);
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        access_len = 16;
        tmp = inw(offset);
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        access_len = 32;
        tmp = inl(offset);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }

    uint64_t mask = (1ULL << sizeasbit) - 1;

    PRINTLOG(ACPIAML, LOG_TRACE, "io writing masking sizeasbit 0x%llx value 0x%llx access_len 0x%llx mask 0x%llx", sizeasbit, val, access_len, mask);

    val &= mask;
    val <<= (field_offset % access_len);

    mask <<= (field_offset % access_len);

    PRINTLOG(ACPIAML, LOG_TRACE, "io writing old data 0x%llx value 0x%llx mask 0x%llx", tmp, val, mask );

    switch (update_rule) {
    case ACPI_AML_FIELD_PRESERVE:
        val = (tmp & ~mask) | val;
        break;
    case ACPI_AML_FIELD_WRITE_ONES:
        val = ~mask | val;
        break;
    case ACPI_AML_FIELD_WRITE_ZEROES:
        break;
    }

    PRINTLOG(ACPIAML, LOG_TRACE, "io writing offset 0x%llx value 0x%llx", offset, val);

    switch (access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        outb(offset, val & 0xFF);
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        outw(offset, val & 0xFFFF);
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        outl(offset, val & 0xFFFFFFFF);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }


    return 0;
}

int8_t acpi_aml_write_pci_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, const acpi_aml_object_t* obj) {
    UNUSED(ctx);

    if(obj == NULL || obj->type != ACPI_AML_OT_FIELD || obj->field.related_object == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "Field or region is null %i", obj == NULL?0:1);
        return -1;
    }

    acpi_aml_object_t* opregion = NULL;
    boolean_t indexedfield = 0;
    uint8_t update_rule = 0;

    if(obj->field.related_object->type == ACPI_AML_OT_OPREGION  &&
       obj->field.related_object->opregion.region_space == ACPI_AML_OPREGT_PCICFG) {
        opregion = obj->field.related_object;
    } else if(obj->field.related_object->type == ACPI_AML_OT_FIELD &&
              obj->field.related_object->field.related_object->type == ACPI_AML_OT_OPREGION  &&
              obj->field.related_object->field.related_object->opregion.region_space == ACPI_AML_OPREGT_PCICFG) {
        opregion = obj->field.related_object->field.related_object;
        indexedfield = 1;
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "op region space is not sysio %i", obj->field.related_object->opregion.region_space);
        return -1;
    }

    uint64_t offset = 0;
    uint64_t field_offset = 0;
    uint8_t access_type = 0;
    uint64_t sizeasbit = 0;

    if(indexedfield) {
        if(acpi_aml_write_pci_as_integer(ctx, obj->field.offset / 8, obj->field.related_object) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "cannot select indexed field");
            return -1;
        }

        offset = obj->field.selector_object->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.selector_object->field.offset;
        access_type = obj->field.selector_object->field.access_type;
        update_rule = obj->field.selector_object->field.update_rule;
        sizeasbit = obj->field.selector_object->field.sizeasbit;
    } else {
        offset = obj->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.offset;
        access_type = obj->field.access_type;
        update_rule = obj->field.update_rule;
        sizeasbit = obj->field.sizeasbit;
    }

    char_t* region_name = opregion->name;
    char_t* aml_device_name = strndup(region_name, strlen(region_name) - 4);

    const acpi_aml_device_t* dev = acpi_device_lookup_by_name(ctx, aml_device_name);


    if(dev == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "cannot find pci device %s", aml_device_name);
        memory_free(aml_device_name);
        return -1;
    }

    memory_free(aml_device_name);

    int64_t adr = 0;

    if(dev->adr == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "pci device does not have adr");
        return -1;
    }

    if(acpi_aml_read_as_integer(ctx, dev->adr, &adr) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "cannot read pci device address");
        return -1;
    }

    uint32_t pci_address = PCI_IO_PORT_CREATE_ADDRESS(0, (adr >> 16) & 0x1F, adr & 0x7, offset);

    uint64_t tmp = 0;
    uint64_t access_len = 0;

    switch (access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        access_len = 8;
        tmp = pci_io_port_read_data(pci_address, 1);
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        access_len = 16;
        tmp = pci_io_port_read_data(pci_address, 2);
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        access_len = 32;
        tmp = pci_io_port_read_data(pci_address, 4);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }

    uint64_t mask = (1ULL << sizeasbit) - 1;

    val &= mask;
    val <<= (field_offset % access_len);

    mask <<= (field_offset % access_len);

    switch (update_rule) {
    case ACPI_AML_FIELD_PRESERVE:
        val = (tmp & ~mask) | val;
        break;
    case ACPI_AML_FIELD_WRITE_ONES:
        val = ~mask | val;
        break;
    case ACPI_AML_FIELD_WRITE_ZEROES:
        break;
    }

    switch (access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        pci_io_port_write_data(pci_address, val, 1);
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        pci_io_port_write_data(pci_address, val, 2);
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        pci_io_port_write_data(pci_address, val, 4);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }


    return 0;
}

int8_t acpi_aml_write_memory_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, const acpi_aml_object_t* obj) {
    UNUSED(ctx);

    if(obj == NULL && !(obj->type == ACPI_AML_OT_FIELD || obj->type == ACPI_AML_OT_BUFFERFIELD) && obj->field.related_object == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "Field or region is null %i", obj == NULL?0:1);
        return -1;
    }

    uint8_t* memva = NULL;

    if(obj->field.related_object->type == ACPI_AML_OT_OPREGION) {
        memva = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(obj->field.related_object->opregion.region_offset);
    } else if(obj->field.related_object->type == ACPI_AML_OT_BUFFER) {
        memva = obj->field.related_object->buffer.buf;
    } else if(obj->field.related_object->type == ACPI_AML_OT_STRING) {
        memva = (uint8_t*)obj->field.related_object->string;
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "not opregion or buffer %i", obj->field.related_object->type);
        return -1;
    }

    memva += obj->field.offset / 8;

    uint8_t* ba = memva;
    uint16_t* wa = (uint16_t*)memva;
    uint32_t* dwa = (uint32_t*)memva;
    uint64_t* qwa = (uint64_t*)memva;

    uint64_t tmp = 0;
    uint64_t access_len = 0;

    switch (obj->field.access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        access_len = 8;
        tmp = *ba;
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        access_len = 16;
        tmp = *wa;
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        access_len = 32;
        tmp = *dwa;
        break;
    case ACPI_AML_FIELD_QWORD_ACCESS:
        access_len = 64;
        tmp = *qwa;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }

    uint64_t mask = (1ULL << obj->field.sizeasbit) - 1;

    val &= mask;
    val <<= (obj->field.offset % access_len);

    mask <<= (obj->field.offset % access_len);

    switch (obj->field.update_rule) {
    case ACPI_AML_FIELD_PRESERVE:
        tmp = (tmp & ~mask) | val;
        break;
    case ACPI_AML_FIELD_WRITE_ONES:
        tmp = ~mask | val;
        break;
    case ACPI_AML_FIELD_WRITE_ZEROES:
        tmp = val;
        break;
    }

    switch (obj->field.access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        *ba = (uint8_t)tmp;
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        *wa = (uint16_t)tmp;
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        *dwa = (uint32_t)tmp;
        break;
    case ACPI_AML_FIELD_QWORD_ACCESS:
        *qwa = (uint64_t)tmp;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }

    return 0;
}

int8_t acpi_aml_read_sysio_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res){
    if(obj == NULL || obj->type != ACPI_AML_OT_FIELD || obj->field.related_object == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "Field or region is null %i", obj == NULL?0:1);
        return -1;
    }

    acpi_aml_object_t* opregion = NULL;
    boolean_t indexedfield = 0;

    if(obj->field.related_object->type == ACPI_AML_OT_OPREGION  &&
       obj->field.related_object->opregion.region_space == ACPI_AML_OPREGT_SYSIO) {
        opregion = obj->field.related_object;
    } else if(obj->field.related_object->type == ACPI_AML_OT_FIELD &&
              obj->field.related_object->field.related_object->type == ACPI_AML_OT_OPREGION  &&
              obj->field.related_object->field.related_object->opregion.region_space == ACPI_AML_OPREGT_SYSIO) {
        opregion = obj->field.related_object->field.related_object;
        indexedfield = 1;
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "op region space is not sysio %i", obj->field.related_object->opregion.region_space);
        return -1;
    }

    uint64_t offset = 0;
    uint64_t field_offset = 0;
    uint8_t access_type = 0;
    uint64_t sizeasbit = 0;

    if(indexedfield) {
        if(acpi_aml_write_sysio_as_integer(ctx, obj->field.offset / 8, obj->field.related_object) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "cannot select indexed field");
            return -1;
        }

        offset = obj->field.selector_object->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.selector_object->field.offset;
        access_type = obj->field.selector_object->field.access_type;
        sizeasbit = obj->field.selector_object->field.sizeasbit;
    } else {
        offset = obj->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.offset;
        access_type = obj->field.access_type;
        sizeasbit = obj->field.sizeasbit;
    }

    PRINTLOG(ACPIAML, LOG_TRACE, "reading io port offset 0x%04llx", offset);

    uint64_t tmp = 0;
    uint32_t access_len = 0;

    switch (access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        access_len = 8;
        tmp = inb(offset);
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        access_len = 16;
        tmp = inw(offset);
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        access_len = 32;
        tmp = inl(offset);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }

    PRINTLOG(ACPIAML, LOG_TRACE, "read value 0x%llx field_offset %lli size %lli", tmp, field_offset, sizeasbit);

    uint64_t mask = (1ULL << sizeasbit) - 1;
    tmp >>= (field_offset % access_len);
    tmp &= mask;

    *res = tmp;

    return 0;
}

int8_t acpi_aml_read_pci_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res){
    if(obj == NULL || obj->type != ACPI_AML_OT_FIELD || obj->field.related_object == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "Field or region is null %i", obj == NULL?0:1);
        return -1;
    }

    acpi_aml_object_t* opregion = NULL;
    boolean_t indexedfield = 0;

    if(obj->field.related_object->type == ACPI_AML_OT_OPREGION  &&
       obj->field.related_object->opregion.region_space == ACPI_AML_OPREGT_PCICFG) {
        opregion = obj->field.related_object;
    } else if(obj->field.related_object->type == ACPI_AML_OT_FIELD &&
              obj->field.related_object->field.related_object->type == ACPI_AML_OT_OPREGION  &&
              obj->field.related_object->field.related_object->opregion.region_space == ACPI_AML_OPREGT_PCICFG) {
        opregion = obj->field.related_object->field.related_object;
        indexedfield = 1;
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "op region space is not pci config %i", obj->field.related_object->opregion.region_space);
        return -1;
    }

    uint64_t offset = 0;
    uint64_t field_offset = 0;
    uint8_t access_type = 0;
    uint64_t sizeasbit = 0;

    if(indexedfield) {
        if(acpi_aml_write_pci_as_integer(ctx, obj->field.offset / 8, obj->field.related_object) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "cannot select indexed field");
            return -1;
        }

        offset = obj->field.selector_object->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.selector_object->field.offset;
        access_type = obj->field.selector_object->field.access_type;
        sizeasbit = obj->field.selector_object->field.sizeasbit;
    } else {
        offset = obj->field.offset / 8 + opregion->opregion.region_offset;
        field_offset = obj->field.offset;
        access_type = obj->field.access_type;
        sizeasbit = obj->field.sizeasbit;
    }

    char_t* region_name = opregion->name;
    char_t* aml_device_name = strndup(region_name, strlen(region_name) - 4);

    const acpi_aml_device_t* dev = acpi_device_lookup_by_name(ctx, aml_device_name);


    if(dev == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "cannot find pci device %s", aml_device_name);
        memory_free(aml_device_name);
        return -1;
    }

    memory_free(aml_device_name);

    int64_t adr = 0;

    if(dev->adr == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "pci device does not have adr");
        return -1;
    }

    if(acpi_aml_read_as_integer(ctx, dev->adr, &adr) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "cannot read pci device address");
        return -1;
    }

    uint32_t pci_address = PCI_IO_PORT_CREATE_ADDRESS(0, (adr >> 16) & 0x1F, adr & 0x7, offset);

    uint64_t tmp = 0;
    uint64_t access_len = 0;

    switch (access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        access_len = 8;
        tmp = pci_io_port_read_data(pci_address, 1);
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        access_len = 16;
        tmp = pci_io_port_read_data(pci_address, 2);
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        access_len = 32;
        tmp = pci_io_port_read_data(pci_address, 4);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }

    uint64_t mask = (1ULL << sizeasbit) - 1;
    tmp >>= (field_offset % access_len);
    tmp &= mask;

    *res = tmp;

    return 0;
}

int8_t acpi_aml_read_memory_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res){
    UNUSED(ctx);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "Field or region is null %i", obj == NULL?0:1);
        return -1;
    }

    if(!(obj->type == ACPI_AML_OT_FIELD || obj->type == ACPI_AML_OT_BUFFERFIELD) && obj->field.related_object == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "Field or region is null %i", obj == NULL?0:1);
        return -1;
    }

    uint8_t* memva = NULL;

    if(obj->field.related_object->type == ACPI_AML_OT_OPREGION) {
        memva = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(obj->field.related_object->opregion.region_offset);
    } else if(obj->field.related_object->type == ACPI_AML_OT_BUFFER) {
        memva = obj->field.related_object->buffer.buf;
    } else if(obj->field.related_object->type == ACPI_AML_OT_STRING) {
        memva = (uint8_t*)obj->field.related_object->string;
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "not opregion or buffer %i", obj->field.related_object->type);
        return -1;
    }

    memva += obj->field.offset / 8;

    uint8_t* ba = memva;
    uint16_t* wa = (uint16_t*)memva;
    uint32_t* dwa = (uint32_t*)memva;
    uint64_t* qwa = (uint64_t*)memva;

    uint64_t tmp = 0;
    uint64_t access_len = 0;

    switch (obj->field.access_type) {
    case ACPI_AML_FIELD_BYTE_ACCESS:
        access_len = 8;
        tmp = *ba;
        break;
    case ACPI_AML_FIELD_WORD_ACCESS:
        access_len = 16;
        tmp = *wa;
        break;
    case ACPI_AML_FIELD_DWORD_ACCESS:
        access_len = 32;
        tmp = *dwa;
        break;
    case ACPI_AML_FIELD_QWORD_ACCESS:
        access_len = 64;
        tmp = *qwa;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown memory access type %i", obj->field.access_type);
        return -1;
    }

    uint64_t mask = (1ULL << obj->field.sizeasbit) - 1;
    tmp >>= (obj->field.offset % access_len);
    tmp &= mask;

    *res = tmp;

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t acpi_aml_read_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res){
    acpi_aml_object_t* t_obj = (acpi_aml_object_t*)obj;
    t_obj = acpi_aml_get_if_arg_local_obj(ctx, t_obj, 0, 0);

    if(t_obj == NULL || res == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "object or res is null %i %i", t_obj == NULL?0:1, res == NULL?0:1);

        return -1;
    }

    int8_t result = 0;
    int64_t ival = 0;
    char_t* strptr;
    acpi_aml_object_t* mth_res = NULL;
    uint8_t region_space;

    PRINTLOG(ACPIAML, LOG_TRACE, "reading from object type %i", t_obj->type);

    switch (t_obj->type) {
    case ACPI_AML_OT_NUMBER:
        *res = t_obj->number.value;
        break;
    case ACPI_AML_OT_STRING:
        strptr = t_obj->string;
        if(strptr[0] == '0' && (strptr[1] == 'x' || strptr[1] == 'X')) {
            strptr += 2;
        }
        while(1) {
            if(*strptr >= 'A' && *strptr <= 'F') {
                ival |= *strptr - 'A' + 10;
            } else if(*strptr >= 'a' && *strptr <= 'f') {
                ival |= *strptr - 'a' + 10;
            } else if(*strptr >= '0' && *strptr <= '9') {
                ival |= *strptr - '0';
            } else {
                ival >>= 4;
                break;
            }
            ival <<= 4;
            strptr++;
        }
        *res = ival;
        break;
    case ACPI_AML_OT_BUFFER:
        *res =  *((int64_t*)t_obj->buffer.buf);
        break;
    case ACPI_AML_OT_OPCODE_EXEC_RETURN:
        return acpi_aml_read_as_integer(ctx, t_obj->opcode_exec_return, res);
    case ACPI_AML_OT_METHOD:
        if(acpi_aml_execute(ctx, t_obj, &mth_res) != 0) {
            return -1;
        }

        result = acpi_aml_read_as_integer(ctx, mth_res, res);

        if(mth_res->name == NULL) {
            acpi_aml_destroy_object(ctx, mth_res);
        }

        return result;
    case ACPI_AML_OT_FIELD:
        region_space = t_obj->field.related_object->opregion.region_space;

        if(t_obj->field.related_object->type == ACPI_AML_OT_FIELD) { // indexedfield
            region_space = t_obj->field.related_object->field.related_object->opregion.region_space;
        }

        switch(region_space) {
        case ACPI_AML_OPREGT_SYSMEM:
            return acpi_aml_read_memory_as_integer(ctx, t_obj, res);
        case ACPI_AML_OPREGT_PCICFG:
            return acpi_aml_read_pci_as_integer(ctx, t_obj, res);
        case ACPI_AML_OPREGT_SYSIO:
            return acpi_aml_read_sysio_as_integer(ctx, t_obj, res);
        default:
            PRINTLOG(ACPIAML, LOG_ERROR, "region space id not implemented %i", t_obj->field.related_object->opregion.region_space);
            return -1;
        }
        return -1;
    case ACPI_AML_OT_BUFFERFIELD:
        return acpi_aml_read_memory_as_integer(ctx, t_obj, res);
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "ctx %s read as integer objtype %i 0x%p", ctx->scope_prefix, t_obj->type, t_obj);
        return -1;
        break;
    }

    return 0;
}
#pragma GCC diagnostic pop

int8_t acpi_aml_write_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, acpi_aml_object_t* obj) {
    obj = acpi_aml_get_if_arg_local_obj(ctx, obj, 1, 0);
    uint8_t region_space;

    if(obj == NULL) {
        return -1;
    }

    if(obj->type == ACPI_AML_OT_UNINITIALIZED) {
        obj->type = ACPI_AML_OT_NUMBER;
        obj->number.bytecnt = 8;
    }

    switch (obj->type) {
    case ACPI_AML_OT_NUMBER:
        obj->number.value = val;
        break;
    case ACPI_AML_OT_FIELD:
        region_space = obj->field.related_object->opregion.region_space;

        if(obj->field.related_object->type == ACPI_AML_OT_FIELD) { // indexedfield
            region_space = obj->field.related_object->field.related_object->opregion.region_space;
        }

        switch(region_space) {
        case ACPI_AML_OPREGT_SYSMEM:
            return acpi_aml_write_memory_as_integer(ctx, val, obj);
        case ACPI_AML_OPREGT_PCICFG:
            return acpi_aml_write_pci_as_integer(ctx, val, obj);
        case ACPI_AML_OPREGT_SYSIO:
            return acpi_aml_write_sysio_as_integer(ctx, val, obj);
        default:
            PRINTLOG(ACPIAML, LOG_ERROR, "region space id not implemented %i", obj->field.related_object->opregion.region_space);
            return -1;
        }
        return -1;
    case ACPI_AML_OT_BUFFERFIELD:
        return acpi_aml_write_memory_as_integer(ctx, val, obj);
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "write as integer objtype %i", obj->type);
        return -1;
    }

    return 0;
}

int8_t acpi_aml_write_as_string(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* src, acpi_aml_object_t* dst) {
    src = acpi_aml_get_if_arg_local_obj(ctx, src, 0, 0);
    acpi_aml_object_t* original_dst = dst;
    dst = acpi_aml_get_if_arg_local_obj(ctx, dst, 0, 0);

    if(!(src->type == ACPI_AML_OT_STRING || src->type == ACPI_AML_OT_NUMBER || src->type == ACPI_AML_OT_BUFFER)) {
        PRINTLOG(ACPIAML, LOG_ERROR, "source type missmatch %i", src->type);

        return -1;
    }

    char_t* src_data = NULL;

    if(src->type == ACPI_AML_OT_STRING) {
        src_data = src->string;
        src_data = strdup_at_heap(ctx->heap, src_data);
    }

    if(src->type == ACPI_AML_OT_BUFFER) {
        src_data = (char_t*)src->buffer.buf;
        src_data = strdup_at_heap(ctx->heap, src_data);
    }

    if(src->type == ACPI_AML_OT_NUMBER) {
        src_data = utoh(src->number.value);
        char_t* tmp = strdup_at_heap(ctx->heap, src_data);
        memory_free(src_data);
        src_data = tmp;
    }

    if(dst->type == ACPI_AML_OT_FIELD) {
        for(uint64_t i = 0; i < strlen(src_data); i++) {
            if(acpi_aml_write_as_integer(ctx, src_data[i], dst) != 0) {
                PRINTLOG(ACPIAML, LOG_ERROR, "cannot write string to field");
                memory_free_ext(ctx->heap, src_data);

                return -1;
            }
        }

        memory_free_ext(ctx->heap, src_data);

        return 0;
    }

    if(dst->type == ACPI_AML_OT_BUFFERFIELD) {
        uint64_t* tmp = (uint64_t*)src_data;

        if(acpi_aml_write_as_integer(ctx, *tmp, dst) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "cannot write string to bufferfield");

            memory_free_ext(ctx->heap, src_data);

            return -1;
        }

        memory_free_ext(ctx->heap, src_data);

        return 0;
    }


    dst = acpi_aml_get_if_arg_local_obj(ctx, original_dst, 1, 0);

    if(dst->type == ACPI_AML_OT_UNINITIALIZED) {
        dst->type = ACPI_AML_OT_STRING;
    }

    if(dst->type != ACPI_AML_OT_STRING) {
        PRINTLOG(ACPIAML, LOG_ERROR, "dest type missmatch %i", dst->type);
        memory_free_ext(ctx->heap, src_data);

        return -1;
    }

    memory_free_ext(ctx->heap, dst->string);

    dst->string = src_data;

    return 0;
}

int8_t acpi_aml_write_as_buffer(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* src, acpi_aml_object_t* dst) {
    src = acpi_aml_get_if_arg_local_obj(ctx, src, 0, 0);
    acpi_aml_object_t* original_dst = dst;
    dst = acpi_aml_get_if_arg_local_obj(ctx, dst, 0, 0);

    if(!(src->type == ACPI_AML_OT_STRING || src->type == ACPI_AML_OT_NUMBER || src->type == ACPI_AML_OT_BUFFER)) {
        PRINTLOG(ACPIAML, LOG_ERROR, "source type missmatch %i", src->type);

        return -1;
    }

    uint8_t* src_data = NULL;
    int64_t src_len = 0;

    if(src->type == ACPI_AML_OT_STRING) {
        src_len = strlen(src->string) + 1;
        src_data = memory_malloc_ext(ctx->heap, src_len, 0);
        memory_memcopy(src->string, src_data, src_len);
    }

    if(src->type == ACPI_AML_OT_BUFFER) {
        src_len = src->buffer.buflen;
        src_data = memory_malloc_ext(ctx->heap, src_len, 0);
        memory_memcopy(src->buffer.buf, src_data, src_len);
    }

    if(src->type == ACPI_AML_OT_NUMBER) {
        src_len = src->number.bytecnt;
        src_data = memory_malloc_ext(ctx->heap, src_len, 0);
        memory_memcopy(&src->number.value, src_data, src_len);
    }

    if(dst->type == ACPI_AML_OT_FIELD) {
        for(int64_t i = 0; i < src_len; i++) {
            if(acpi_aml_write_as_integer(ctx, src_data[i], dst) != 0) {
                PRINTLOG(ACPIAML, LOG_ERROR, "cannot write string to field");

                memory_free_ext(ctx->heap, src_data);

                return -1;
            }
        }

        memory_free_ext(ctx->heap, src_data);

        return 0;
    }

    if(dst->type == ACPI_AML_OT_BUFFERFIELD) {
        uint64_t* tmp = (uint64_t*)src_data;

        if(acpi_aml_write_as_integer(ctx, *tmp, dst) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "cannot write string to bufferfield");

            memory_free_ext(ctx->heap, src_data);

            return -1;
        }

        memory_free_ext(ctx->heap, src_data);

        return 0;
    }

    if(dst->type == ACPI_AML_OT_DEBUG) {
        int64_t i = 0;

        for(; i < src_len - 1; i++) {
            printf("%x ", src_data[i]);
        }

        printf("%x\n", src_data[i]);
        memory_free_ext(ctx->heap, src_data);

        return 0;
    }


    dst = acpi_aml_get_if_arg_local_obj(ctx, original_dst, 1, 0);

    boolean_t need_buf_alloc = 0;

    if(dst->type == ACPI_AML_OT_UNINITIALIZED) {
        dst->type = ACPI_AML_OT_BUFFER;
        need_buf_alloc = 1;
    }

    if(dst->type != ACPI_AML_OT_BUFFER) {
        PRINTLOG(ACPIAML, LOG_ERROR, "dest type missmatch %i", dst->type);

        return -1;
    }

    if(need_buf_alloc) {
        dst->buffer.buf = memory_malloc_ext(ctx->heap, src_len, 0);
        dst->buffer.buflen = src_len;
    } else {
        memory_memclean(dst->buffer.buf, dst->buffer.buflen);
    }

    int64_t l = MIN(src_len, dst->buffer.buflen);

    memory_memcopy(src_data, dst->buffer.buf, l);
    memory_free_ext(ctx->heap, src_data);

    return 0;
}


int8_t acpi_aml_is_lead_name_char(uint8_t* c) {
    if(('A' <= *c && *c <= 'Z') || *c == '_') {
        return 0;
    }
    return -1;
}

int8_t acpi_aml_is_digit_char(uint8_t* c) {
    if('0' <= *c && *c <= '9') {
        return 0;
    }
    return -1;
}

int8_t acpi_aml_is_name_char(uint8_t* data) {
    if(acpi_aml_is_lead_name_char(data) == 0 ||  acpi_aml_is_digit_char(data) == 0 ) {
        return 0;
    }
    return -1;
}

int8_t acpi_aml_is_root_char(uint8_t* c) {
    if (*c == ACPI_AML_ROOT_CHAR) {
        return 0;
    }
    return -1;
}

int8_t acpi_aml_is_parent_prefix_char(uint8_t* c){
    if (*c == ACPI_AML_PARENT_CHAR) {
        return 0;
    }
    return -1;
}



char_t* acpi_aml_normalize_name(acpi_aml_parser_context_t* ctx, const char_t* prefix, const char_t* name) {
    uint64_t max_len = strlen(prefix) + strlen(name) + 1;
    char_t* dst_name = memory_malloc_ext(ctx->heap, sizeof(char_t) * max_len, 0x0);

    uint64_t prefix_len = strlen(prefix);

    if(dst_name == NULL) {
        return NULL;
    }

    if(acpi_aml_is_root_char((uint8_t*)name) == 0) {
        strcopy(name, dst_name);
    } else {
        uint64_t prefix_cnt = 0;
        const char_t* tmp = name;

        while(acpi_aml_is_parent_prefix_char((uint8_t*)tmp) == 0) {
            tmp++;
            prefix_cnt++;
        }

        if((prefix_len - 1) < (prefix_cnt * 4)) {
            memory_free_ext(ctx->heap, dst_name);
            return NULL;
        }

        uint64_t src_len = strlen(prefix) - (prefix_cnt * 4);
        memory_memcopy(prefix, dst_name, src_len);
        strcopy(name + prefix_cnt, dst_name + src_len);
    }

    char_t* nomname = memory_malloc_ext(ctx->heap, sizeof(char_t) * strlen(dst_name) + 1, 0x0);

    if(nomname == NULL) {
        memory_free_ext(ctx->heap, dst_name);
        return NULL;
    }

    strcopy(dst_name, nomname);

    memory_free_ext(ctx->heap, dst_name);

    return nomname;
}



int8_t acpi_aml_is_nameseg(uint8_t* data) {
    if(acpi_aml_is_lead_name_char(data) == 0 && acpi_aml_is_name_char(data + 1) == 0 && acpi_aml_is_name_char(data + 2) == 0
       && acpi_aml_is_name_char(data + 3) == 0) {
        return 0;
    }
    return -1;
}

int8_t acpi_aml_is_namestring_start(uint8_t* data){
    if(acpi_aml_is_lead_name_char(data) == 0 || acpi_aml_is_root_char(data) == 0 || acpi_aml_is_parent_prefix_char(data) == 0) {
        return 0;
    }
    if(*data == ACPI_AML_DUAL_PREFIX) {
        data++;
        for(int8_t i = 0; i < 8; i++) {
            if(acpi_aml_is_name_char(data + i) != 0) {
                return -1;
            }
        }
        return 0;
    }
    if(*data == ACPI_AML_MULTI_PREFIX) {
        data++;
        uint8_t segcnt = *data;
        data++;
        for(int8_t i = 0; i < 4 * segcnt; i++) {
            if(acpi_aml_is_name_char(data + i) != 0) {
                return -1;
            }
        }
        return 0;
    }
    return -1;
}

uint64_t acpi_aml_parse_package_length(acpi_aml_parser_context_t* ctx){
    uint8_t pkgleadbyte = *ctx->data;
    ctx->data++;
    uint8_t bytecnt = pkgleadbyte >> 6;
    uint8_t usedbytes = 1 + bytecnt;
    uint64_t pkglen = 0;

    if(bytecnt == 0) {
        pkglen = pkgleadbyte & 0x3F;
    }else {
        pkglen = pkgleadbyte & 0x0F;
        uint8_t tmp8 = 0;
        uint64_t tmp64 = 0;
        if(bytecnt > 0) {
            tmp8 = *ctx->data;
            tmp64 = tmp8;
            pkglen = (tmp64 << 4) | pkglen;
            ctx->data++;
            bytecnt--;
        }
        if(bytecnt > 0) {
            tmp8 = *ctx->data;
            tmp64 = tmp8;
            pkglen = (tmp64 << 12) | pkglen;
            ctx->data++;
            bytecnt--;
        }
        if(bytecnt > 0) {
            tmp8 = *ctx->data;
            tmp64 = tmp8;
            pkglen = (tmp64 << 20) | pkglen;
            ctx->data++;
            bytecnt--;
        }
    }

    ctx->remaining -= usedbytes;
    return pkglen - usedbytes;
}

uint64_t acpi_aml_len_namestring(acpi_aml_parser_context_t* ctx){
    uint64_t res = 0;
    uint8_t* local_data = ctx->data;

    while(acpi_aml_is_root_char(local_data) == 0 || acpi_aml_is_parent_prefix_char(local_data) == 0) {
        res++;
        local_data++;
    }

    if(*local_data == ACPI_AML_DUAL_PREFIX) {
        return res + 8;
    }else if(*local_data == ACPI_AML_MULTI_PREFIX) {
        local_data++;
        return res + 4 * (*local_data);
    } else if(*local_data == ACPI_AML_ZERO) {
        return res;
    }

    return res + 4;
}

acpi_aml_object_t* acpi_aml_symbol_lookup_at_table(acpi_aml_parser_context_t* ctx, index_t* table, const char_t* prefix, const char_t* symbol_name){
    char_t* tmp_prefix = memory_malloc_ext(ctx->heap, strlen(prefix) + 1, 0x0);

    if(tmp_prefix == NULL) {
        return NULL;
    }

    strcopy(prefix, tmp_prefix);

    while(1) {
        PRINTLOG(ACPIAML, LOG_TRACE, "searching symbol current prefix %s", tmp_prefix);

        char_t* nomname = acpi_aml_normalize_name(ctx, tmp_prefix, symbol_name);

        if(nomname == NULL) {
            memory_free_ext(ctx->heap, tmp_prefix);
            return NULL;
        }

        PRINTLOG(ACPIAML, LOG_TRACE, "normalized name %s", nomname);

        acpi_aml_object_t* obj = (acpi_aml_object_t*)table->find(table, nomname);

        if(obj) {
            memory_free_ext(ctx->heap, nomname);
            memory_free_ext(ctx->heap, tmp_prefix);

            PRINTLOG(ACPIAML, LOG_TRACE, "symbol found");

            return obj;
        }

        if(strlen(tmp_prefix) == 1) {
            memory_free_ext(ctx->heap, nomname);
            break;
        }

        if(strlen(symbol_name) > 0 && acpi_aml_is_root_char((uint8_t*)symbol_name) == 0) {
            memory_free_ext(ctx->heap, nomname);
            break;
        }

        tmp_prefix[strlen(tmp_prefix) - 4] = NULL;

        memory_free_ext(ctx->heap, nomname);
    }

    memory_free_ext(ctx->heap, tmp_prefix);

    PRINTLOG(ACPIAML, LOG_TRACE, "symbol not found");

    return NULL;
}

acpi_aml_object_t* acpi_aml_symbol_lookup(acpi_aml_parser_context_t* ctx, const char_t* symbol_name){
    if(ctx->local_symbols != NULL) {
        PRINTLOG(ACPIAML, LOG_TRACE, "searching symbol %s in local symbols", symbol_name);
        acpi_aml_object_t* obj = acpi_aml_symbol_lookup_at_table(ctx, ctx->local_symbols, ctx->scope_prefix, symbol_name);

        if(obj != NULL) {
            return obj;
        }

        PRINTLOG(ACPIAML, LOG_TRACE, "symbol %s not found in local symbols, searching in global symbols", symbol_name);

    }

    return acpi_aml_symbol_lookup_at_table(ctx, ctx->symbols, ctx->scope_prefix, symbol_name);
}

int8_t acpi_aml_add_obj_to_symboltable(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj) {
    acpi_aml_object_t* oldsym = acpi_aml_symbol_lookup(ctx, obj->name);
    if(oldsym != NULL) {
        if(obj->type == ACPI_AML_OT_SCOPE && (
               oldsym->type == ACPI_AML_OT_SCOPE ||
               oldsym->type == ACPI_AML_OT_DEVICE ||
               oldsym->type == ACPI_AML_OT_POWERRES ||
               oldsym->type == ACPI_AML_OT_PROCESSOR ||
               oldsym->type == ACPI_AML_OT_THERMALZONE
               )
           ) {

            acpi_aml_destroy_object(ctx, obj);

            return 0;
        }

        acpi_aml_destroy_object(ctx, obj);

        return -1;
    }

    if(ctx->local_symbols != NULL) {
        ctx->local_symbols->insert(ctx->local_symbols, obj->name, obj, NULL);
    }else {
        ctx->symbols->insert(ctx->symbols, obj->name, obj, NULL);
    }

    return 0;
}

uint8_t acpi_aml_get_index_of_extended_code(uint8_t code) {
    uint8_t res = -1;

    if(code >= 0x80) {
        res = code - 0x6d;
    }else if(code >= 0x30) {
        res = code - 0x21;
    } else if(code >= 0x1f) {
        res = code - 0x1b;
    } else if(code >= 0x12) {
        res = code - 0x10;
    } else {
        res = code - 0x01;
    }

    return res;
}

void acpi_aml_destroy_symbol_table(acpi_aml_parser_context_t* ctx, uint8_t local){
    uint64_t item_count = 0;
    iterator_t* iter = NULL;
    index_t* symtbl;

    if(local) {
        symtbl = ctx->local_symbols;
    } else {
        symtbl = ctx->symbols;
    }

    iter = symtbl->create_iterator(symtbl);

    if(iter == NULL) {
        return;
    }

    while(iter->end_of_iterator(iter) != 0) {
        acpi_aml_object_t* sym = (acpi_aml_object_t*)iter->get_item(iter);
        acpi_aml_destroy_object(ctx, sym);
        item_count++;
        iter = iter->next(iter);
    }

    iter->destroy(iter);
    bplustree_destroy_index(symtbl);
}

void acpi_aml_destroy_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj){
    if(obj == NULL) {
        return;
    }

    iterator_t* iter = NULL;

    switch (obj->type) {
    case ACPI_AML_OT_STRING:
        memory_free_ext(ctx->heap, obj->string);
        break;
    case ACPI_AML_OT_BUFFER:
        memory_free_ext(ctx->heap, obj->buffer.buf);
        break;
    case ACPI_AML_OT_PACKAGE:
        iter = list_iterator_create(obj->package.elements);

        while(iter->end_of_iterator(iter) != 0) {
            acpi_aml_object_t* t_obj = (acpi_aml_object_t*)iter->get_item(iter);

            if(t_obj->name == NULL || t_obj->type == ACPI_AML_OT_RUNTIMEREF || t_obj->type == ACPI_AML_OT_PACKAGE) {
                acpi_aml_destroy_object(ctx, t_obj);
            }

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        list_destroy(obj->package.elements);

        if(obj->package.pkglen->name == NULL) {
            memory_free_ext(ctx->heap, obj->package.pkglen);
        }
        break;
    case ACPI_AML_OT_DATAREGION:
        memory_free_ext(ctx->heap, obj->dataregion.signature);
        memory_free_ext(ctx->heap, obj->dataregion.oemid);
        memory_free_ext(ctx->heap, obj->dataregion.oemtableid);
        break;
    case ACPI_AML_OT_OPCODE_EXEC_RETURN:
        if(obj->opcode_exec_return->name == NULL) {
            acpi_aml_destroy_object(ctx, obj->opcode_exec_return);
        }
        break;
    case ACPI_AML_OT_NUMBER:
    case ACPI_AML_OT_EVENT:
    case ACPI_AML_OT_MUTEX:
    case ACPI_AML_OT_OPREGION:
    case ACPI_AML_OT_POWERRES:
    case ACPI_AML_OT_PROCESSOR:
    case ACPI_AML_OT_THERMALZONE:
    case ACPI_AML_OT_EXTERNAL:
    case ACPI_AML_OT_METHOD:
    case ACPI_AML_OT_TIMER:
    case ACPI_AML_OT_SCOPE:
    case ACPI_AML_OT_FIELD:
    case ACPI_AML_OT_BUFFERFIELD:
    case ACPI_AML_OT_DEVICE:
    case ACPI_AML_OT_ALIAS:
    case ACPI_AML_OT_RUNTIMEREF:
        break;
    default:
        // printf("ACPIAML: Warning object destroy may be required %li\n", obj->type);
        break;
    }

    memory_free_ext(ctx->heap, obj->name);
    memory_free_ext(ctx->heap, obj);
}

char_t* acpi_aml_parse_eisaid(acpi_aml_parser_context_t* ctx, uint64_t eisaid_num) {
    char_t* res = memory_malloc_ext(ctx->heap, 8, 0);

    if(res == NULL) {
        return NULL;
    }

    eisaid_num = BYTE_SWAP32(eisaid_num);

    res[6] = DIGIT_TO_HEX(eisaid_num & 0xF);
    eisaid_num >>= 4;
    res[5] = DIGIT_TO_HEX(eisaid_num & 0xF);
    eisaid_num >>= 4;
    res[4] = DIGIT_TO_HEX(eisaid_num & 0xF);
    eisaid_num >>= 4;
    res[3] = DIGIT_TO_HEX(eisaid_num & 0xF);
    eisaid_num >>= 4;

    res[2] = (eisaid_num & 0x1F) + 'A' - 1;
    eisaid_num >>= 5;
    res[1] = (eisaid_num & 0x1F) + 'A' - 1;
    eisaid_num >>= 5;
    res[0] = (eisaid_num & 0x1F) + 'A' - 1;

    return res;
}

void acpi_aml_print_symbol_table(acpi_aml_parser_context_t* ctx){
    uint64_t item_count = 0;
    iterator_t* iter = ctx->symbols->create_iterator(ctx->symbols);

    while(iter->end_of_iterator(iter) != 0) {
        acpi_aml_object_t* sym = (acpi_aml_object_t*)iter->get_item(iter);

        acpi_aml_print_object(ctx, sym);

        item_count++;

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    printf("totoal syms %lli\n", item_count );;
}

void acpi_aml_print_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj){

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_FATAL, "null object");
        return;
    }

    if(obj->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
        obj = obj->opcode_exec_return;
        if(obj == NULL) {
            PRINTLOG(ACPIAML, LOG_FATAL, "null object");
            return;
        }
    }

    int64_t len = 0;
    int64_t ival = 0;
    char_t* eisaid = NULL;

    printf("object id=%p name=%s type=", obj, obj->name);

    switch (obj->type) {
    case ACPI_AML_OT_NUMBER:
        printf("number value=0x%llx bytecnt=%i", obj->number.value, obj->number.bytecnt );
        if(strends(obj->name, "_HID") == 0) {
            eisaid = acpi_aml_parse_eisaid(ctx, obj->number.value);
            printf(" eisaid=%s\n", eisaid);
            memory_free_ext(ctx->heap, eisaid);
        } else {
            printf("\n");
        }
        break;
    case ACPI_AML_OT_STRING:
        printf("string value=%s\n", obj->string );
        break;
    case ACPI_AML_OT_ALIAS:
        printf("alias value=%s\n", obj->alias_target->name );
        break;
    case ACPI_AML_OT_BUFFER:
        printf("buffer buflen=%lli\n", obj->buffer.buflen );
        break;
    case ACPI_AML_OT_PACKAGE:
        acpi_aml_read_as_integer(ctx, obj->package.pkglen, &ival);
        printf("pkg pkglen=%lli initial pkglen=%lli\n", len, list_size(obj->package.elements) );
        break;
    case ACPI_AML_OT_SCOPE:
        printf("scope\n");
        break;
    case ACPI_AML_OT_DEVICE:
        printf("device\n");
        break;
    case ACPI_AML_OT_POWERRES:
        printf("powerres system_level=%i resource_order=%i\n", obj->powerres.system_level, obj->powerres.resource_order);
        break;
    case ACPI_AML_OT_PROCESSOR:
        printf("processor procid=%i pblk_addr=0x%x pblk_len=%i\n", obj->processor.procid, obj->processor.pblk_addr, obj->processor.pblk_len);
        break;
    case ACPI_AML_OT_THERMALZONE:
        printf("thermalzone\n");
        break;
    case ACPI_AML_OT_MUTEX:
        printf("mutex syncflags=%i\n", obj->mutex_sync_flags);
        break;
    case ACPI_AML_OT_EVENT:
        printf("event\n");
        break;
    case ACPI_AML_OT_DATAREGION:
        printf("dataregion signature=%s oemid=%s oemtableid=%s\n", obj->dataregion.signature, obj->dataregion.oemid, obj->dataregion.oemtableid);
        break;
    case ACPI_AML_OT_OPREGION:
        printf("opregion region_space=0x%x region_offset=0x%llx region_len=0x%llx\n", obj->opregion.region_space, obj->opregion.region_offset, obj->opregion.region_len);
        break;
    case ACPI_AML_OT_METHOD:
        printf("method argcount=%i serflag=%i synclevel=%i termlistlen=%lli\n", obj->method.arg_count, obj->method.serflag,
               obj->method.sync_level, obj->method.termlist_length);
        break;
    case ACPI_AML_OT_EXTERNAL:
        printf("external objecttype=%i argcount=%i\n", obj->external.object_type, obj->external.arg_count);
        break;
    case ACPI_AML_OT_FIELD:
    case ACPI_AML_OT_BUFFERFIELD:
        printf("field related_object=%s offset=0x%llx sizeasbit=%lli ", obj->field.related_object->name, obj->field.offset, obj->field.sizeasbit);

        if(obj->field.selector_object) {
            printf("selector_object=%s ", obj->field.selector_object->name);
        }

        if(obj->field.selector_data) {
            printf("selecto_data=%s ", obj->field.selector_data->name);
        }

        printf("\n");
        break;
    case ACPI_AML_OT_LOCAL_OR_ARG:
        printf("%s%i\n", obj->local_or_arg.idx_local_or_arg > 7 ? "ARG" : "LOCAL", obj->local_or_arg.idx_local_or_arg > 7 ? obj->local_or_arg.idx_local_or_arg - 1 : obj->local_or_arg.idx_local_or_arg);
        break;
    case ACPI_AML_OT_REFOF:
        printf("refof ");
        acpi_aml_print_object(ctx, obj->refof_target);
        break;
    default:
        printf("unknown object %i\n", obj->type);
    }
}

acpi_aml_object_t* acpi_aml_duplicate_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj){
    acpi_aml_object_t* new_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(new_obj == NULL) {
        return NULL;
    }

    memory_memcopy(obj, new_obj, sizeof(acpi_aml_object_t));

    new_obj->name = strdup_at_heap(ctx->heap, obj->name);

    switch (obj->type) {
    case ACPI_AML_OT_STRING:
        new_obj->string = strdup_at_heap(ctx->heap, obj->string);
        break;
    case ACPI_AML_OT_BUFFER:
        new_obj->buffer.buf = memory_malloc_ext(ctx->heap, obj->buffer.buflen, 0x0);
        memory_memcopy(obj->buffer.buf, new_obj->buffer.buf, obj->buffer.buflen);
        break;
    case ACPI_AML_OT_NUMBER:
    case ACPI_AML_OT_EVENT:
    case ACPI_AML_OT_MUTEX:
    case ACPI_AML_OT_OPREGION:
    case ACPI_AML_OT_POWERRES:
    case ACPI_AML_OT_PROCESSOR:
    case ACPI_AML_OT_EXTERNAL:
    case ACPI_AML_OT_METHOD:
    case ACPI_AML_OT_TIMER:
    case ACPI_AML_OT_LOCAL_OR_ARG:
        break;
    case ACPI_AML_OT_REFOF:
        PRINTLOG(ACPIAML, LOG_WARNING, "duplicate of refof");
        break;
    default: // TODO: other pointers
        PRINTLOG(ACPIAML, LOG_FATAL, "not implemented %i", obj->type);
        break;
    }

    return new_obj;
}

acpi_aml_object_t* acpi_aml_get_real_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj) {
    while(obj && (obj->type == ACPI_AML_OT_ALIAS || obj->type == ACPI_AML_OT_RUNTIMEREF || obj->type == ACPI_AML_OT_OPCODE_EXEC_RETURN)) {
        if(obj->type == ACPI_AML_OT_ALIAS) {
            if(obj->alias_target == NULL) {
                return NULL;
            }

            obj = obj->alias_target;
        } else if(obj->type == ACPI_AML_OT_RUNTIMEREF) {
            obj = acpi_aml_symbol_lookup(ctx, obj->name);
        } else if(obj->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
            obj = obj->opcode_exec_return;
        }
    }

    return obj;
}
