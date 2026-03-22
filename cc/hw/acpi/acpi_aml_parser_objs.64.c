/**
 * @file acpi_aml_parser_objs.64.c
 * @brief acpi aml object parser methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___ACPI_AML_IMPLEMENTATION 0
#include <acpi/aml_internal.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.kernel.hw.acpi");


int8_t acpi_aml_parse_namestring(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    if(data == NULL || *data == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "data pointer null for namestring parser");
        ctx->flags.fatal = true;
        return -1;
    }

    char_t* name        = (char_t*)*data;
    uint64_t idx        = 0;
    uint64_t t_consumed = 0;

    while(acpi_aml_is_root_char(ctx->data) || acpi_aml_is_parent_prefix_char(ctx->data)) {
        name[idx++] = *ctx->data;

        ctx->data++;
        ctx->remaining--;
        t_consumed++;
    }

    if(*ctx->data == ACPI_AML_DUAL_PREFIX) {
        ctx->data++;
        ctx->remaining--;

        memory_memcopy(ctx->data, name + idx, 8);

        ctx->data      += 8;
        ctx->remaining -= 8;
        t_consumed     += 9;
    }else if(*ctx->data == ACPI_AML_MULTI_PREFIX) {
        ctx->data++;
        ctx->remaining--;

        uint8_t size = *ctx->data;
        size *= 4;

        ctx->data++;
        ctx->remaining--;

        memory_memcopy(ctx->data, name + idx, size);

        ctx->data      += size;
        ctx->remaining -= size;
        t_consumed     += 2 + size;

    } else if(*ctx->data == ACPI_AML_ZERO) {
        ctx->data++;
        ctx->remaining--;
        t_consumed++;
    } else {
        memory_memcopy(ctx->data, name + idx, 4);

        ctx->data      += 4;
        ctx->remaining -= 4;
        t_consumed     += 4;
    }

    if(consumed != NULL) {
        *consumed = t_consumed;
    }

    return 0;
}

int8_t acpi_aml_parse_const_data(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    if(data == NULL || *data == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "data pointer null for const data parser");
        ctx->flags.fatal = true;
        return -1;
    }
    acpi_aml_object_t* obj = (acpi_aml_object_t*)*data;
    uint8_t op_code        = *ctx->data;
    uint64_t len;
    uint64_t t_consumed = 0;

    ctx->data++;
    ctx->remaining--;
    t_consumed++;

    switch (op_code) {
    case ACPI_AML_ZERO:
        obj->type         = ACPI_AML_OT_NUMBER;
        obj->number.value = 0;

        if(ctx->revision >= 0x02) {
            obj->number.bytecnt = 8;
        } else {
            obj->number.bytecnt = 4;
        }

        break;
    case ACPI_AML_ONE:
        obj->type         = ACPI_AML_OT_NUMBER;
        obj->number.value = 1;

        if(ctx->revision >= 0x02) {
            obj->number.bytecnt = 8;
        } else {
            obj->number.bytecnt = 4;
        }

        break;
    case ACPI_AML_ONES:
        obj->type = ACPI_AML_OT_NUMBER;

        if(ctx->revision >= 0x02) {
            obj->number.value   = 0xFFFFFFFFFFFFFFFF;
            obj->number.bytecnt = 8;
        } else {
            obj->number.value   = 0xFFFFFFFF;
            obj->number.bytecnt = 4;
        }

        break;
    case ACPI_AML_BYTE_PREFIX:
        obj->type           = ACPI_AML_OT_NUMBER;
        obj->number.value   = *ctx->data;
        obj->number.bytecnt = 1;

        ctx->data++;
        ctx->remaining--;
        t_consumed++;
        break;
    case ACPI_AML_WORD_PREFIX:
        obj->type           = ACPI_AML_OT_NUMBER;
        obj->number.value   = *((uint16_t*)(void*)(void*)(ctx->data));
        obj->number.bytecnt = 2;

        ctx->data      += 2;
        ctx->remaining -= 2;
        t_consumed     += 2;
        break;
    case ACPI_AML_DWORD_PREFIX:
        obj->type           = ACPI_AML_OT_NUMBER;
        obj->number.value   = *((uint32_t*)(void*)(ctx->data));
        obj->number.bytecnt = 4;

        ctx->data      += 4;
        ctx->remaining -= 4;
        t_consumed     += 4;
        break;
    case ACPI_AML_QWORD_PREFIX:
        obj->type           = ACPI_AML_OT_NUMBER;
        obj->number.value   = *((uint64_t*)(void*)(ctx->data));
        obj->number.bytecnt = 8;

        ctx->data      += 8;
        ctx->remaining -= 8;
        t_consumed     += 8;
        break;
    case ACPI_AML_STRING_PREFIX:
        len = strlen((char_t*)ctx->data);
        char_t* str = memory_malloc_ext(ctx->heap, sizeof(char_t) * len + 1, 0x0);

        if(str == NULL) {
            return -1;
        }

        strcopy((char_t*)ctx->data, str);

        obj->type       = ACPI_AML_OT_STRING;
        obj->string     = str;
        ctx->data      += len + 1;
        ctx->remaining -= len + 1;
        t_consumed     += len + 1;
        break;

    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown constant data op code 0x%02x", op_code);
        return -1;
    }

    if(consumed != NULL) {
        *consumed = t_consumed;
    }

    return 0;
}

int8_t acpi_aml_parse_byte_data(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    if(data == NULL || *data == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "data pointer null for byte data parser");
        ctx->flags.fatal = true;
        return -1;
    }

    acpi_aml_object_t* obj = (acpi_aml_object_t*)*data;
    uint64_t t_consumed    = 1;

    obj->type           = ACPI_AML_OT_NUMBER;
    obj->number.value   = *ctx->data;
    obj->number.bytecnt = 1;

    ctx->data++;
    ctx->remaining--;


    if(consumed != NULL) {
        *consumed = t_consumed;
    }

    return 0;
}

int8_t acpi_aml_parse_alias(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    UNUSED(consumed);

    ctx->data++;
    ctx->remaining--;

    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* srcname  = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(srcname == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate source name for alias");
        return -1;
    }

    if(acpi_aml_parse_namestring(ctx, (void**)&srcname, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse source name for alias");
        memory_free_ext(ctx->heap, srcname);
        return -1;
    }

    acpi_aml_object_t* src_obj = acpi_aml_symbol_lookup(ctx, srcname);
    if(src_obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to find source object for alias with name %s", srcname);
        memory_free_ext(ctx->heap, srcname);
        return -1;
    }

    memory_free_ext(ctx->heap, srcname);


    namelen = acpi_aml_len_namestring(ctx);
    char_t* dstname = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(dstname == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate destination name for alias");
        return -1;
    }

    if(acpi_aml_parse_namestring(ctx, (void**)&dstname, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse destination name for alias");
        memory_free_ext(ctx->heap, dstname);
        return -1;
    }


    char_t* dstnomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, dstname);
    memory_free_ext(ctx->heap, dstname);

    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate alias object");
        return -1;
    }

    obj->type         = ACPI_AML_OT_ALIAS;
    obj->name         = dstnomname;
    obj->alias_target = src_obj;

    acpi_aml_add_obj_to_symboltable(ctx, obj);

    return 0;
}

int8_t acpi_aml_parse_scope(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    UNUSED(consumed);

    uint8_t opcode = *ctx->data;
    ctx->data++;
    ctx->remaining--;

    acpi_aml_object_type_t obj_type;

    switch (opcode) {
    case ACPI_AML_SCOPE:
        obj_type = ACPI_AML_OT_SCOPE;
        break;
    case ACPI_AML_DEVICE:
        obj_type = ACPI_AML_OT_DEVICE;
        break;
    case ACPI_AML_POWERRES:
        obj_type = ACPI_AML_OT_POWERRES;
        break;
    case ACPI_AML_PROCESSOR:
        obj_type = ACPI_AML_OT_PROCESSOR;
        break;
    case ACPI_AML_THERMALZONE:
        obj_type = ACPI_AML_OT_THERMALZONE;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown scope opcode 0x%02x", opcode);
        return -1;
    }


    uint64_t pkglen = acpi_aml_parse_package_length(ctx);

    uint64_t namelen = acpi_aml_len_namestring(ctx);

    char_t* name = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for scope");
        return -1;
    }

    int64_t tmp_start = ctx->remaining;

    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for scope");
        memory_free_ext(ctx->heap, name);
        return -1;
    }

    pkglen -= (tmp_start - ctx->remaining);

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);

    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for scope");
        memory_free_ext(ctx->heap, nomname);
        return -1;
    }

    obj->type = obj_type;
    obj->name = nomname;


    if(obj_type == ACPI_AML_OT_POWERRES) {
        obj->powerres.system_level = *ctx->data;
        ctx->data++;
        ctx->remaining--;
        obj->powerres.resource_order = *((uint16_t*)(void*)ctx->data);
        ctx->data                   += 2;
        ctx->remaining              -= 2;
        pkglen                      -= 3;
    } else if(obj_type == ACPI_AML_OT_PROCESSOR) {
        obj->processor.procid = *ctx->data;
        ctx->data++;
        ctx->remaining--;
        obj->processor.pblk_addr = *((uint32_t*)(void*)ctx->data);
        ctx->data               += 4;
        ctx->remaining          -= 4;
        obj->processor.pblk_len  = *ctx->data;
        ctx->data++;
        ctx->remaining--;
        pkglen -= 6;
    }

    char_t* new_scope_prefix = strdup_at_heap(ctx->heap, nomname);

    int8_t res = acpi_aml_add_obj_to_symboltable(ctx, obj);

    if(res != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to add scope object to symbol table");
        return res;
    }

    uint64_t old_length      = ctx->length;
    uint64_t old_remaining   = ctx->remaining;
    char_t* old_scope_prefix = ctx->scope_prefix;

    ctx->length       = pkglen;
    ctx->remaining    = pkglen;
    ctx->scope_prefix = new_scope_prefix;

    res = acpi_aml_parse_all_items(ctx, NULL, NULL);

    memory_free_ext(ctx->heap, new_scope_prefix);

    ctx->length       = old_length;
    ctx->remaining    = old_remaining - pkglen;
    ctx->scope_prefix = old_scope_prefix;

    return res;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t acpi_aml_parse_buffer(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(consumed);

    if(data == NULL || *data == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "data pointer null for buffer parser");
        ctx->flags.fatal = true;
        return -1;
    }

    acpi_aml_object_t* buf = (acpi_aml_object_t*)*data;
    uint64_t t_consumed    = 0;
    uint64_t r_consumed    = 1;


    ctx->data++;
    ctx->remaining--;

    r_consumed += ctx->remaining;
    uint64_t plen = acpi_aml_parse_package_length(ctx);
    r_consumed -= ctx->remaining;
    r_consumed += plen;


    acpi_aml_object_t* buflenobj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(buflenobj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate buffer length object for buffer parser");
        return -1;
    }

    if(acpi_aml_parse_one_item(ctx, (void**)&buflenobj, &t_consumed) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse buffer length object for buffer parser");
        memory_free_ext(ctx->heap, buflenobj);
        return -1;
    }
    plen      -= t_consumed;
    t_consumed = 0;

    int64_t buflen = 0;

    if( acpi_aml_read_as_integer(ctx, buflenobj, &buflen) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to read buffer length object as integer for buffer parser");
        memory_free_ext(ctx->heap, buflenobj);
        return -1;
    }

    if(buflenobj->name == NULL) {
        acpi_aml_destroy_object(ctx, buflenobj);
    }

    buf->type          = ACPI_AML_OT_BUFFER;
    buf->buffer.buflen = buflen;
    buf->buffer.buf    = memory_malloc_ext(ctx->heap, sizeof(uint8_t) * buflen, 0x0);

    if(buf->buffer.buf == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate buffer for buffer parser");
        return -1;
    }

    memory_memcopy(ctx->data, buf->buffer.buf, plen);

    ctx->data      += plen;
    ctx->remaining -= plen;

    if(consumed != NULL) {
        *consumed = r_consumed;
    }

    return 0;
}
#pragma GCC diagnostic pop

int8_t acpi_aml_parse_package(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    uint64_t t_consumed = 0;
    uint64_t r_consumed = 1;

    if(data == NULL || *data == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "data pointer null for package parser");
        ctx->flags.fatal = true;
        return -1;
    }

    acpi_aml_object_t* pkg = (acpi_aml_object_t*)*data;
    pkg->type = ACPI_AML_OT_PACKAGE;

    ctx->data++;
    ctx->remaining--;

    r_consumed += ctx->remaining;
    uint64_t plen = acpi_aml_parse_package_length(ctx);
    r_consumed -= ctx->remaining;
    r_consumed += plen;

    acpi_aml_object_t* pkglen = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(pkglen == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate package length object for package parser");
        return -1;
    }

    if(acpi_aml_parse_byte_data(ctx, (void**)&pkglen, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse package length object for package parser");
        memory_free_ext(ctx->heap, pkglen);
        return -1;
    }
    plen--;

    pkg->package.pkglen   = pkglen;
    pkg->package.elements = list_create_list_with_heap(ctx->heap);

    while(plen > 0) {
        t_consumed = 0;
        acpi_aml_object_t* tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for package element in package parser");
            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, &t_consumed) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse package element for package parser");
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }
        list_list_insert(pkg->package.elements, tmp_obj);
        plen -= t_consumed;
    }


    if(consumed != NULL) {
        *consumed = r_consumed;
    }

    return 0;
}

int8_t acpi_aml_parse_varpackage(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    uint64_t t_consumed = 0;
    uint64_t r_consumed = 1;

    if(data == NULL || *data == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "data pointer null for varpackage parser");
        ctx->flags.fatal = true;
        return -1;
    }

    acpi_aml_object_t* pkg = (acpi_aml_object_t*)*data;
    pkg->type = ACPI_AML_OT_PACKAGE;

    ctx->data++;
    ctx->remaining--;

    r_consumed += ctx->remaining;
    uint64_t plen = acpi_aml_parse_package_length(ctx);
    r_consumed -= ctx->remaining;
    r_consumed += plen;

    acpi_aml_object_t* pkglen = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(pkglen == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate package length object for varpackage parser");
        return -1;
    }

    if(acpi_aml_parse_one_item(ctx, (void**)&pkglen, &t_consumed) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse package length object for varpackage parser");
        memory_free_ext(ctx->heap, pkglen);
        return -1;
    }
    plen      -= t_consumed;
    t_consumed = 0;

    pkg->package.pkglen   = pkglen;
    pkg->package.elements = list_create_list_with_heap(ctx->heap);

    while(plen > 0) {
        t_consumed = 0;
        acpi_aml_object_t* tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for varpackage element in varpackage parser");
            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, &t_consumed) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse varpackage element for varpackage parser");
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }
        list_list_insert(pkg->package.elements, tmp_obj);
        plen -= t_consumed;
    }

    t_consumed = 0;
    if(consumed != NULL) {
        *consumed = r_consumed;
    }

    return 0;
}



int8_t acpi_aml_parse_method(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    uint64_t r_consumed = 1;
    uint64_t t_consumed = 0;

    ctx->data++;
    ctx->remaining--;

    r_consumed += ctx->remaining;
    uint64_t plen = acpi_aml_parse_package_length(ctx);
    r_consumed -= ctx->remaining;
    r_consumed += plen;

    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for method");
        return -1;
    }

    t_consumed = ctx->remaining;

    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for method");
        memory_free_ext(ctx->heap, name);
        return -1;
    }

    t_consumed -= ctx->remaining;
    plen       -= t_consumed;
    r_consumed += t_consumed;

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);


    uint8_t flags = *ctx->data;
    ctx->data++;
    ctx->remaining--;
    r_consumed++;
    plen--;


    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for method parser");
        return -1;
    }

    obj->name                   = nomname;
    obj->type                   = ACPI_AML_OT_METHOD;
    obj->method.arg_count       = flags & 0x03;
    obj->method.serflag         = flags & 0x04;
    obj->method.sync_level      = flags >> 4;
    obj->method.termlist_length = plen;
    obj->method.termlist        = ctx->data;

    acpi_aml_add_obj_to_symboltable(ctx, obj);

    ctx->data      += plen;
    ctx->remaining -= plen;
    r_consumed     += plen;

    if(consumed != NULL) {
        *consumed = r_consumed;
    }

    return 0;
}

int8_t acpi_aml_parse_external(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    uint64_t t_consumed = 0;

    ctx->data++;
    ctx->remaining--;
    t_consumed++;

    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "data pointer null for external parser");
        return -1;
    }


    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for external");
        memory_free_ext(ctx->heap, name);
        return -1;
    }
    t_consumed += namelen;

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);


    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for external parser");
        return -1;
    }

    obj->name = nomname;
    obj->type = ACPI_AML_OT_EXTERNAL;

    uint8_t flags;

    flags = *ctx->data;
    ctx->data++;
    ctx->remaining--;
    t_consumed++;
    obj->external.object_type = flags;

    flags = *ctx->data;
    ctx->data++;
    ctx->remaining--;
    t_consumed++;
    obj->external.arg_count = flags;

    acpi_aml_add_obj_to_symboltable(ctx, obj);

    if(consumed != NULL) {
        *consumed = t_consumed;
    }

    return 0;
}

int8_t acpi_aml_parse_mutex(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    uint64_t t_consumed = 0;
    uint64_t r_consumed = 0;

    ctx->data++;
    ctx->remaining--;
    r_consumed++;

    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for mutex");
        return -1;
    }

    t_consumed = ctx->remaining;
    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for mutex");
        memory_free_ext(ctx->heap, name);
        return -1;
    }
    t_consumed -= ctx->remaining;
    r_consumed += t_consumed;

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);

    uint8_t flags = *ctx->data;
    ctx->data++;
    ctx->remaining--;
    r_consumed++;


    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for mutex parser");
        return -1;
    }

    obj->name             = nomname;
    obj->type             = ACPI_AML_OT_MUTEX;
    obj->mutex_sync_flags = flags;

    acpi_aml_add_obj_to_symboltable(ctx, obj);

    if(consumed != NULL) {
        *consumed = r_consumed;
    }

    return 0;
}

int8_t acpi_aml_parse_event(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    UNUSED(consumed);

    ctx->data++;
    ctx->remaining--;

    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for event");
        return -1;
    }

    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        memory_free_ext(ctx->heap, name);
        return -1;
    }

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);

    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for event parser");
        return -1;
    }

    obj->name = nomname;
    obj->type = ACPI_AML_OT_EVENT;

    acpi_aml_add_obj_to_symboltable(ctx, obj);

    return 0;
}

int8_t acpi_aml_parse_region(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    UNUSED(consumed);

    uint8_t opcode = *ctx->data;
    ctx->data++;
    ctx->remaining--;

    acpi_aml_object_type_t obj_type;

    switch (opcode) {
    case ACPI_AML_DATAREGION:
        obj_type = ACPI_AML_OT_DATAREGION;
        break;
    case ACPI_AML_OPREGION:
        obj_type = ACPI_AML_OT_OPREGION;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown region opcode 0x%02x", opcode);
        return -1;
    }

    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for region");
        return -1;
    }

    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for region");
        memory_free_ext(ctx->heap, name);
        return -1;
    }

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);

    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for region parser");
        return -1;
    }

    obj->name = nomname;
    obj->type = obj_type;

    acpi_aml_object_t* tmp_obj;

    if(obj_type == ACPI_AML_OT_DATAREGION) {
        tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for signature in data region parser");
            memory_free_ext(ctx->heap, obj);

            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse signature for data region parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        obj->dataregion.signature = tmp_obj->string;
        memory_free_ext(ctx->heap, tmp_obj);

        tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for oemid in data region parser");
            memory_free_ext(ctx->heap, obj);
            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse oemid for data region parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        obj->dataregion.oemid = tmp_obj->string;
        memory_free_ext(ctx->heap, tmp_obj);

        tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for oemtableid in data region parser");
            memory_free_ext(ctx->heap, obj);
            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse oemtableid for data region parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        obj->dataregion.oemtableid = tmp_obj->string;
        memory_free_ext(ctx->heap, tmp_obj);
    } else {
        tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for region space in opregion parser");
            memory_free_ext(ctx->heap, obj);
            return -1;
        }

        if(acpi_aml_parse_byte_data(ctx, (void**)&tmp_obj, NULL) !=  0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse region space for opregion parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        int64_t ival = 0;

        if(acpi_aml_read_as_integer(ctx, tmp_obj, &ival) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to read region space as integer for opregion parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        memory_free_ext(ctx->heap, tmp_obj);

        obj->opregion.region_space = ival;

        tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for region offset in opregion parser");
            memory_free_ext(ctx->heap, obj);
            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse region offset for opregion parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        if(acpi_aml_read_as_integer(ctx, tmp_obj, &ival) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to read region offset as integer for opregion parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        if(tmp_obj->name == NULL) {
            acpi_aml_destroy_object(ctx, tmp_obj);
        }

        obj->opregion.region_offset = ival;

        tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(tmp_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate temporary object for region length in opregion parser");
            memory_free_ext(ctx->heap, obj);
            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse region length for opregion parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        if(acpi_aml_read_as_integer(ctx, tmp_obj, &ival) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to read region length as integer for opregion parser");
            memory_free_ext(ctx->heap, obj);
            memory_free_ext(ctx->heap, tmp_obj);
            return -1;
        }

        if(tmp_obj->name == NULL) {
            acpi_aml_destroy_object(ctx, tmp_obj);
        }

        obj->opregion.region_len = ival;
    }

    acpi_aml_add_obj_to_symboltable(ctx, obj);

    return 0;
}

int8_t acpi_aml_parse_create_field(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    UNUSED(consumed);

    uint8_t access_type   = ACPI_AML_FIELD_ACCESS_ANY;
    uint8_t access_attrib = ACPI_AML_FIELD_ACCESS_ATTRIBUTE_NORMAL;
    uint8_t lock_rule     = ACPI_AML_FIELD_LOCK_NOLOCK;
    uint8_t update_rule   = ACPI_AML_FIELD_UPDATE_OVERRIDE;
    int64_t sizeasbit     = 0;

    uint8_t opcode = *ctx->data;
    ctx->data++;
    ctx->remaining--;

    switch (opcode) {
    case ACPI_AML_ARBFIELD:
        access_type = ACPI_AML_FIELD_ACCESS_BUFFER;
        break;
    case ACPI_AML_BITFIELD:
        sizeasbit   = 1;
        access_type = ACPI_AML_FIELD_ACCESS_BIT;
        break;
    case ACPI_AML_BYTEFIELD:
        sizeasbit   = 8;
        access_type = ACPI_AML_FIELD_ACCESS_BYTE;
        break;
    case ACPI_AML_WORDFIELD:
        sizeasbit   = 16;
        access_type = ACPI_AML_FIELD_ACCESS_WORD;
        break;
    case ACPI_AML_DWORDFIELD:
        sizeasbit   = 32;
        access_type = ACPI_AML_FIELD_ACCESS_DWORD;
        break;
    case ACPI_AML_QWORDFIELD:
        sizeasbit   = 64;
        access_type = ACPI_AML_FIELD_ACCESS_QWORD;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown create field opcode 0x%02x", opcode);
        return -1;
    }

    acpi_aml_object_t* buf = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(buf == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate buffer object for create field parser");
        return -1;
    }

    if(acpi_aml_parse_one_item(ctx, (void**)&buf, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse buffer object for create field parser");
        memory_free_ext(ctx->heap, buf);
        return -1;
    }

    acpi_aml_object_t* offset_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(offset_obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate offset object for create field parser");
        memory_free_ext(ctx->heap, buf);
        return -1;
    }

    if(acpi_aml_parse_one_item(ctx, (void**)&offset_obj, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse offset object for create field parser");
        memory_free_ext(ctx->heap, buf);
        memory_free_ext(ctx->heap, offset_obj);
        return -1;
    }

    int64_t offset = 0;

    if(acpi_aml_read_as_integer(ctx, offset_obj, &offset) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to read offset object as integer for create field parser");
        memory_free_ext(ctx->heap, buf);
        memory_free_ext(ctx->heap, offset_obj);
        return -1;
    }

    memory_free_ext(ctx->heap, offset_obj);

    if(opcode == ACPI_AML_ARBFIELD) {
        acpi_aml_object_t* size_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

        if(size_obj == NULL) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate size object for create field parser");
            memory_free_ext(ctx->heap, buf);
            return -1;
        }

        if(acpi_aml_parse_one_item(ctx, (void**)&size_obj, NULL) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse size object for create field parser");
            memory_free_ext(ctx->heap, buf);
            memory_free_ext(ctx->heap, offset_obj);
            memory_free_ext(ctx->heap, size_obj);
            return -1;
        }

        if(acpi_aml_read_as_integer(ctx, size_obj, &sizeasbit) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to read size object as integer for create field parser");
            memory_free_ext(ctx->heap, buf);
            memory_free_ext(ctx->heap, offset_obj);
            memory_free_ext(ctx->heap, size_obj);
            return -1;
        }

        memory_free_ext(ctx->heap, size_obj);
    }


    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for create field");
        memory_free_ext(ctx->heap, buf);
        return -1;
        return -1;
    }

    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for create field");
        memory_free_ext(ctx->heap, buf);
        memory_free_ext(ctx->heap, name);
        return -1;
    }

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);

    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for create field parser");
        memory_free_ext(ctx->heap, buf);
        return -1;
    }

    obj->name                 = nomname;
    obj->type                 = ACPI_AML_OT_BUFFERFIELD;
    obj->field.related_object = buf;
    obj->field.access_type    = access_type;
    obj->field.access_attrib  = access_attrib;
    obj->field.lock_rule      = lock_rule;
    obj->field.update_rule    = update_rule;
    obj->field.sizeasbit      = sizeasbit;
    obj->field.offset         = offset * 8; // convert to bit offset


    acpi_aml_add_obj_to_symboltable(ctx, obj);

    return 0;
}

int8_t acpi_aml_parse_field(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    UNUSED(consumed);
    uint64_t pkglen;
    uint64_t t_consumed;
    acpi_aml_object_t* rel_obj  = NULL;
    acpi_aml_object_t* sel_obj  = NULL;
    acpi_aml_object_t* sel_data = NULL;

    uint8_t opcode = *ctx->data;
    ctx->data++;
    ctx->remaining--;

    pkglen = acpi_aml_parse_package_length(ctx);

    t_consumed = 0;
    if(acpi_aml_parse_one_item(ctx, (void**)&rel_obj, &t_consumed) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse related object for field parser");
        return -1;
    }

    pkglen -= t_consumed;

    if(opcode == ACPI_AML_BANKFIELD || opcode == ACPI_AML_INDEXFIELD) {
        t_consumed = 0;
        if(acpi_aml_parse_one_item(ctx, (void**)&sel_obj, &t_consumed) != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse selector object for field parser");
            return -1;
        }

        pkglen -= t_consumed;

        if(opcode == ACPI_AML_BANKFIELD) {
            t_consumed = 0;
            if(acpi_aml_parse_one_item(ctx, (void**)&sel_data, &t_consumed) != 0) {
                PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse selector data for bank field parser");
                return -1;
            }

            pkglen -= t_consumed;
        }
    }

    uint8_t flags = *ctx->data;
    ctx->data++;
    ctx->remaining--;
    pkglen--;

    uint8_t access_type   = flags & 0x0F;
    uint8_t access_attrib = ACPI_AML_FIELD_ACCESS_ATTRIBUTE_NORMAL;
    uint8_t lock_rule     = (flags & 0x10) >> 4;
    uint8_t update_rule   = (flags & 0x60) >> 5;
    uint64_t sizeasbit    = 0;
    uint64_t offset       = 0;

    while(pkglen > 0) {
        uint8_t fieldcode = *ctx->data;
        if(fieldcode == 0x00) {
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            t_consumed = ctx->remaining;
            uint64_t skip = acpi_aml_parse_package_length(ctx);
            t_consumed -= ctx->remaining;
            pkglen     -= t_consumed;

            skip += t_consumed;

            offset += skip;
        } else if(fieldcode == 0x01) {
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            uint8_t at = *ctx->data;
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            access_type = at & 0x0F;

            uint8_t aa = *ctx->data;
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            UNUSED(aa);
            // TODO: parse and send remainings.
            return -1;
        }else if(fieldcode == 0x02) {
            ctx->data++;
            ctx->remaining--;
            pkglen--;
            // TODO: parse and send remainings. field references anoother field
            return -1;
        }else if(fieldcode == 0x03) {
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            uint8_t at = *ctx->data;
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            access_type = at & 0x0F;

            uint8_t eaa = *ctx->data;
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            uint8_t al = *ctx->data;
            ctx->data++;
            ctx->remaining--;
            pkglen--;

            UNUSED(eaa);
            UNUSED(al);
            // TODO: parse and send remainings.
            return -1;
        } else {
            uint64_t namelen = acpi_aml_len_namestring(ctx);
            char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

            if(name == NULL) {
                PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for field");
                return -1;
            }

            t_consumed = ctx->remaining;
            if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
                PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for field");
                memory_free_ext(ctx->heap, name);
                return -1;
            }
            t_consumed -= ctx->remaining;
            pkglen     -= t_consumed;

            char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
            memory_free_ext(ctx->heap, name);

            acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

            if(obj == NULL) {
                PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for field parser");
                return -1;
            }

            t_consumed  = ctx->remaining;
            sizeasbit   = acpi_aml_parse_package_length(ctx);
            t_consumed -= ctx->remaining;
            pkglen     -= t_consumed;

            sizeasbit += t_consumed;

            obj->name                  = nomname;
            obj->type                  = ACPI_AML_OT_FIELD;
            obj->field.related_object  = rel_obj;
            obj->field.selector_object = sel_obj;
            obj->field.selector_data   =  sel_data;
            obj->field.access_type     = access_type;
            obj->field.access_attrib   = access_attrib;
            obj->field.lock_rule       = lock_rule;
            obj->field.update_rule     = update_rule;
            obj->field.sizeasbit       = sizeasbit;
            obj->field.offset          = offset;

            acpi_aml_add_obj_to_symboltable(ctx, obj);

            offset += sizeasbit;
        }
    }

    return 0;
}

int8_t acpi_aml_parse_name(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
    UNUSED(data);
    UNUSED(consumed);

    ctx->data++;
    ctx->remaining--;

    uint64_t namelen = acpi_aml_len_namestring(ctx);
    char_t* name     = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

    if(name == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate name for name parser");
        return -1;
    }

    if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse name for name parser");
        memory_free_ext(ctx->heap, name);
        return -1;
    }

    char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
    memory_free_ext(ctx->heap, name);

    acpi_aml_object_t* obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(obj == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to allocate object for name parser");
        return -1;
    }

    obj->name = nomname;

    if(acpi_aml_parse_one_item(ctx, (void**)&obj, NULL) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "failed to parse value for name parser");
        memory_free_ext(ctx->heap, obj);
        memory_free_ext(ctx->heap, nomname);
        return -1;
    }

    acpi_aml_add_obj_to_symboltable(ctx, obj);

    return 0;
}
