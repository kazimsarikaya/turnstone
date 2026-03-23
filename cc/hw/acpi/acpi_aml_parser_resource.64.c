/**
 * @file acpi_aml_parser_resource.64.c
 * @brief acpi parser methods for device resources
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___ACPI_AML_IMPLEMENTATION 0
#include <acpi/aml_internal.h>
#include <acpi/aml_resource.h>
#include <stdbufs.h>
#include <logging.h>

MODULE("turnstone.kernel.hw.acpi");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t acpi_aml_resource_parse_smallitem_irq(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_smallitem_t* res) {
    if(device->interrupts == NULL) {
        device->interrupts = list_create_list_with_heap(ctx->heap);
    }

    acpi_aml_device_interrupt_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_interrupt_t), 0);

    if(item == NULL) {
        return -1;
    }

    item->edge            = res->irq.mode;
    item->low             = res->irq.polarity;
    item->shared          = res->irq.sharing;
    item->wake_capability = res->irq.wake_capability;
    item->interrupt_no    = 0;

    uint16_t intno = res->irq.irq_masks;

    while((intno & 1) != 1) {
        item->interrupt_no++;
        intno >>= 1;
    }

    list_list_insert(device->interrupts, item);

    return 0;
}

static int8_t acpi_aml_resource_parse_smallitem_io(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_smallitem_t* res) {
    if(device->ioports == NULL) {
        device->ioports = list_create_list_with_heap(ctx->heap);
    }

    acpi_aml_device_ioport_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_ioport_t), 0);

    if(item == NULL) {
        return -1;
    }

    item->min       = res->io.min;
    item->max       = res->io.max;
    item->alignment = 1 << res->io.align;
    item->length    = res->io.length;

    list_list_insert(device->ioports, item);

    return 0;
}

static int8_t acpi_aml_resource_parse_smallitem_dma(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_smallitem_t* res) {
    if(device->dmas == NULL) {
        device->dmas = list_create_list_with_heap(ctx->heap);
    }

    acpi_aml_device_dma_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_dma_t), 0);

    if(item == NULL) {
        return -1;
    }

    item->master   = res->dma.bus_master_status;
    item->channels = res->dma.channels;
    item->speed    = res->dma.speed_type;

    list_list_insert(device->dmas, item);

    return 0;
}

static int8_t acpi_aml_resource_parse_smallitem_endtag(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_smallitem_t* res) {
    UNUSED(ctx);
    UNUSED(device);

    return res->end_tag.checksum == 0?0:-1;
}

static int8_t acpi_aml_resource_parse_largeitem_memory_range_32bit_fixed(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_largeitem_t* res) {
    if(device->memory_ranges == NULL) {
        device->memory_ranges = list_create_list_with_heap(ctx->heap);
    }

    acpi_aml_device_memory_range_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_memory_range_t), 0);

    if(item == NULL) {
        return -1;
    }

    item->writable     = res->memory_range_32bit_fixed.rw;
    item->cacheable    = 0;
    item->prefetchable = 0;
    item->type         = ACPI_AML_DEVICE_MEMORY_RANGE_MEMORY;
    item->min          = res->memory_range_32bit_fixed.base;
    item->max          = res->memory_range_32bit_fixed.base + res->memory_range_32bit_fixed.length;

    item->max--;

    list_list_insert(device->memory_ranges, item);

    return 0;
}

static int8_t acpi_aml_resource_parse_largeitem_extended_interrupt(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_largeitem_t* res) {
    if(device->interrupts == NULL) {
        device->interrupts = list_create_list_with_heap(ctx->heap);
    }

    for(int32_t i = 0; i < res->extended_interrupt.count; i++) {
        acpi_aml_device_interrupt_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_interrupt_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->edge            = res->extended_interrupt.mode;
        item->low             = res->extended_interrupt.polarity;
        item->shared          = res->extended_interrupt.sharing;
        item->wake_capability = res->extended_interrupt.wake_capability;
        item->interrupt_no    =  res->extended_interrupt.interrupts[i];

        list_list_insert(device->interrupts, item);
    }

    return 0;
}

static int8_t acpi_aml_resource_parse_largeitem_word_address_space(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_largeitem_t* res) {
    PRINTLOG(ACPIAML, LOG_TRACE, "parsing word address space resource for device %s", device->name);
    PRINTLOG(ACPIAML, LOG_TRACE, "type %i decode_type %i ", res->word_address_space.type, res->word_address_space.decode_type);

    if(res->word_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_MEMORY) {
        if(device->memory_ranges == NULL) {
            device->memory_ranges = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_memory_range_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_memory_range_t), 0);


        if(item == NULL) {
            return -1;
        }

        item->writable     = res->word_address_space.type_spesific_flags.memory_flag.write;
        item->cacheable    = (res->word_address_space.type_spesific_flags.memory_flag.mem & 1) == 1;
        item->prefetchable = (res->word_address_space.type_spesific_flags.memory_flag.mem & 2) == 2;
        item->type         = res->word_address_space.type_spesific_flags.memory_flag.mtp;
        item->min          = res->word_address_space.min;
        item->max          = res->word_address_space.max;

        if(res->word_address_space.min_address_fixed == 0) {
            item->min <<= res->word_address_space.gra + 1;
        }

        if(res->word_address_space.max_address_fixed == 0) {
            item->max <<= res->word_address_space.gra + 1;
        }

        list_list_insert(device->memory_ranges, item);
    } else if(res->word_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_IO) {
        if(device->ioports == NULL) {
            device->ioports = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_ioport_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_ioport_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->min = res->word_address_space.min;
        item->max = res->word_address_space.max;

        if(res->word_address_space.min_address_fixed == 0) {
            item->min <<= res->word_address_space.gra + 1;
        }

        if(res->word_address_space.max_address_fixed == 0) {
            item->max <<= res->word_address_space.gra + 1;
        }

        item->alignment = 1 << res->word_address_space.type_spesific_flags.io_flag.rng;
        item->length    = item->max - item->min + 1;

        list_list_insert(device->ioports, item);
    } else if(res->word_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_BUS) {
        if(device->buses == NULL) {
            device->buses = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_bus_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_bus_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->min = res->word_address_space.min;
        item->max = res->word_address_space.max;

        if(res->word_address_space.min_address_fixed == 0) {
            item->min <<= res->word_address_space.gra + 1;
        }

        if(res->word_address_space.max_address_fixed == 0) {
            item->max <<= res->word_address_space.gra + 1;
        }

        list_list_insert(device->buses, item);
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "device %s unknown word address type %i", device->name, res->word_address_space.type);

        return -1;
    }

    return 0;
}

static int8_t acpi_aml_resource_parse_largeitem_dword_address_space(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_largeitem_t* res) {
    PRINTLOG(ACPIAML, LOG_TRACE, "parsing dword address space resource for device %s", device->name);
    PRINTLOG(ACPIAML, LOG_TRACE, "type %i decode_type %i ", res->dword_address_space.type, res->dword_address_space.decode_type);


    if(res->dword_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_MEMORY) {
        if(device->memory_ranges == NULL) {
            device->memory_ranges = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_memory_range_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_memory_range_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->writable     = res->dword_address_space.type_spesific_flags.memory_flag.write;
        item->cacheable    = (res->dword_address_space.type_spesific_flags.memory_flag.mem & 1) == 1;
        item->prefetchable = (res->dword_address_space.type_spesific_flags.memory_flag.mem & 2) == 2;
        item->type         = res->dword_address_space.type_spesific_flags.memory_flag.mtp;
        item->min          = res->dword_address_space.min;
        item->max          = res->dword_address_space.max;

        if(res->dword_address_space.min_address_fixed == 0) {
            item->min <<= res->dword_address_space.gra + 1;
        }

        if(res->dword_address_space.max_address_fixed == 0) {
            item->max <<= res->dword_address_space.gra + 1;
        }

        list_list_insert(device->memory_ranges, item);
    } else if(res->dword_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_IO) {
        if(device->ioports == NULL) {
            device->ioports = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_ioport_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_ioport_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->min = res->dword_address_space.min;
        item->max = res->dword_address_space.max;

        if(res->dword_address_space.min_address_fixed == 0) {
            item->min <<= res->dword_address_space.gra + 1;
        }

        if(res->dword_address_space.max_address_fixed == 0) {
            item->max <<= res->dword_address_space.gra + 1;
        }

        item->alignment = 1 << res->dword_address_space.type_spesific_flags.io_flag.rng;
        item->length    = item->max - item->min + 1;

        list_list_insert(device->ioports, item);
    } else if(res->dword_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_BUS) {
        if(device->buses == NULL) {
            device->buses = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_bus_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_bus_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->min = res->dword_address_space.min;
        item->max = res->dword_address_space.max;

        if(res->dword_address_space.min_address_fixed == 0) {
            item->min <<= res->dword_address_space.gra + 1;
        }

        if(res->dword_address_space.max_address_fixed == 0) {
            item->max <<= res->dword_address_space.gra + 1;
        }

        list_list_insert(device->buses, item);
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "device %s unknown dword address type %i", device->name, res->dword_address_space.type);

        return -1;
    }

    return 0;
}

static int8_t acpi_aml_resource_parse_largeitem_qword_address_space(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_largeitem_t* res) {
    PRINTLOG(ACPIAML, LOG_TRACE, "parsing qword address space resource for device %s", device->name);
    PRINTLOG(ACPIAML, LOG_TRACE, "type %i decode_type %i ", res->qword_address_space.type, res->qword_address_space.decode_type);

    if(res->qword_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_MEMORY) {
        if(device->memory_ranges == NULL) {
            device->memory_ranges = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_memory_range_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_memory_range_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->writable     = res->qword_address_space.type_spesific_flags.memory_flag.write;
        item->cacheable    = (res->qword_address_space.type_spesific_flags.memory_flag.mem & 1) == 1;
        item->prefetchable = (res->qword_address_space.type_spesific_flags.memory_flag.mem & 2) == 2;
        item->type         = res->qword_address_space.type_spesific_flags.memory_flag.mtp;
        item->min          = res->qword_address_space.min;
        item->max          = res->qword_address_space.max;

        if(res->qword_address_space.min_address_fixed == 0) {
            item->min <<= res->qword_address_space.gra + 1;
        }

        if(res->qword_address_space.max_address_fixed == 0) {
            item->max <<= res->qword_address_space.gra + 1;
        }

        list_list_insert(device->memory_ranges, item);
    } else if (res->qword_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_IO) {
        if(device->ioports == NULL) {
            device->ioports = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_ioport_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_ioport_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->min = res->qword_address_space.min;
        item->max = res->qword_address_space.max;

        if(res->qword_address_space.min_address_fixed == 0) {
            item->min <<= res->qword_address_space.gra + 1;
        }

        if(res->qword_address_space.max_address_fixed == 0) {
            item->max <<= res->qword_address_space.gra + 1;
        }

        item->alignment = 1 << res->qword_address_space.type_spesific_flags.io_flag.rng;
        item->length    = item->max - item->min + 1;

        list_list_insert(device->ioports, item);
    } else if(res->qword_address_space.type == ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_BUS) {
        if(device->buses == NULL) {
            device->buses = list_create_list_with_heap(ctx->heap);
        }

        acpi_aml_device_bus_t* item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_bus_t), 0);

        if(item == NULL) {
            return -1;
        }

        item->min = res->qword_address_space.min;
        item->max = res->qword_address_space.max;

        if(res->qword_address_space.min_address_fixed == 0) {
            item->min <<= res->qword_address_space.gra + 1;
        }

        if(res->qword_address_space.max_address_fixed == 0) {
            item->max <<= res->qword_address_space.gra + 1;
        }

        list_list_insert(device->buses, item);
    } else {
        PRINTLOG(ACPIAML, LOG_ERROR, "device %s unknown qword address type %i", device->name, res->qword_address_space.type);

        return -1;
    }

    return 0;
}

static int8_t acpi_aml_resource_parse_smallitem(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_smallitem_t* res) {
    int8_t mres = -1;

    switch (res->name) {
    case ACPI_AML_RESOURCE_SMALLITEM_IRQ:
        mres = acpi_aml_resource_parse_smallitem_irq(ctx, device, res);
        break;
    case ACPI_AML_RESOURCE_SMALLITEM_IO:
        mres = acpi_aml_resource_parse_smallitem_io(ctx, device, res);
        break;
    case ACPI_AML_RESOURCE_SMALLITEM_DMA:
        mres = acpi_aml_resource_parse_smallitem_dma(ctx, device, res);
        break;
    case ACPI_AML_RESOURCE_SMALLITEM_ENDTAG:
        mres = acpi_aml_resource_parse_smallitem_endtag(ctx, device, res);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "smallitem type is not implemented %i", res->name);
        break;
    }

    return mres;
}

static int8_t acpi_aml_resource_parse_largeitem(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_resource_largeitem_t* res) {
    int8_t mres = -1;

    switch (res->name) {
    case ACPI_AML_RESOURCE_LARGEITEM_32BIT_FIXEDMEMORY_RANGE:
        mres = acpi_aml_resource_parse_largeitem_memory_range_32bit_fixed(ctx, device, res);
        break;
    case ACPI_AML_RESOURCE_LARGEITEM_EXTENDED_INTERRUPT:
        mres = acpi_aml_resource_parse_largeitem_extended_interrupt(ctx, device, res);
        break;
    case ACPI_AML_RESOURCE_LARGEITEM_WORD_ADDRESS_SPACE:
        mres = acpi_aml_resource_parse_largeitem_word_address_space(ctx, device, res);
        break;
    case ACPI_AML_RESOURCE_LARGEITEM_DWORD_ADDRESS_SPACE:
        mres = acpi_aml_resource_parse_largeitem_dword_address_space(ctx, device, res);
        break;
    case ACPI_AML_RESOURCE_LARGEITEM_QWORD_ADDRESS_SPACE:
        mres = acpi_aml_resource_parse_largeitem_qword_address_space(ctx, device, res);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "largeitem type is not implemented %i", res->name);
        break;
    }

    return mres;
}

int32_t acpi_aml_resource_parse(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_object_t* buffer) {
    if(ctx == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "acpi ctx is null");
        return -1;
    }

    if(buffer == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "buffer is null");
        return -1;
    }

    if(buffer->type != ACPI_AML_OT_BUFFER) {
        PRINTLOG(ACPIAML, LOG_ERROR, "buffer type mismatch");
        return -1;
    }

    int64_t crs_size         = buffer->buffer.buflen;
    int64_t offset           = 0;
    int64_t itemlen          = 0;
    acpi_aml_resource_t* res = (acpi_aml_resource_t*)buffer->buffer.buf;

    int32_t parse_res = 0;

    while(offset < crs_size) {
        if(res->identifier.type == ACPI_AML_RESOURCE_SMALLITEM) {
            PRINTLOG(ACPIAML, LOG_TRACE, "device %s small item %i with length %i", device->name, res->smallitem.name, res->smallitem.length);
            itemlen    = res->smallitem.length + 1;
            parse_res += acpi_aml_resource_parse_smallitem(ctx, device, (acpi_aml_resource_smallitem_t*)res);
        } else if (res->identifier.type == ACPI_AML_RESOURCE_LARGEITEM) {
            PRINTLOG(ACPIAML, LOG_TRACE, "device %s large item %i with length %i", device->name, res->largeitem.name, res->largeitem.length);
            itemlen    = res->largeitem.length + 3;
            parse_res += acpi_aml_resource_parse_largeitem(ctx, device, (acpi_aml_resource_largeitem_t*)res);
        } else {
            PRINTLOG(ACPIAML, LOG_ERROR, "device %s unknown crs type %i, stopping enumaration", device->name, res->identifier.type);
            break;
        }

        offset += itemlen;
        res     = (acpi_aml_resource_t*)(buffer->buffer.buf + offset);
    }

    return parse_res;
}
#pragma GCC diagnostic pop

int8_t acpi_aml_resource_print(acpi_aml_parser_context_t* ctx, const acpi_aml_device_t* device) {
    if(!ctx) {
        PRINTLOG(ACPIAML, LOG_ERROR, "acpi ctx is null");
        return -1;
    }

    if(!device) {
        PRINTLOG(ACPIAML, LOG_ERROR, "device is null");
        return -1;
    }

    if(device->memory_ranges) {
        iterator_t* mem_iter = list_iterator_create(device->memory_ranges);

        while(!mem_iter->end_of_iterator(mem_iter)) {
            const acpi_aml_device_memory_range_t* mem = mem_iter->get_item(mem_iter);

            printf("  memory range 0x%llx-0x%llx %s %s %s type %i\n", mem->min, mem->max, mem->writable?"rw":"ro", mem->cacheable?"cacheable":"non-cacheable", mem->prefetchable?"prefetchable":"non-prefetchable", mem->type);

            mem_iter = mem_iter->next(mem_iter);
        }

        mem_iter->destroy(mem_iter);
    }

    if(device->ioports) {
        iterator_t* io_iter = list_iterator_create(device->ioports);

        while(!io_iter->end_of_iterator(io_iter)) {
            const acpi_aml_device_ioport_t* io = io_iter->get_item(io_iter);

            printf("  ioport range 0x%x-0x%x alignment: 0x%x length: 0x%x\n", io->min, io->max, io->alignment, io->length);

            io_iter = io_iter->next(io_iter);
        }

        io_iter->destroy(io_iter);
    }

    if(device->interrupts) {
        iterator_t* int_iter = list_iterator_create(device->interrupts);

        while(!int_iter->end_of_iterator(int_iter)) {
            const acpi_aml_device_interrupt_t* int_item = int_iter->get_item(int_iter);

            printf("  interrupt 0x%02x edge %i low %i shared %i wake_capability %i\n", int_item->interrupt_no, int_item->edge, int_item->low, int_item->shared, int_item->wake_capability);

            int_iter = int_iter->next(int_iter);
        }

        int_iter->destroy(int_iter);
    }

    if(device->dmas) {
        iterator_t* dma_iter = list_iterator_create(device->dmas);

        while(!dma_iter->end_of_iterator(dma_iter)) {
            const acpi_aml_device_dma_t* dma_item = dma_iter->get_item(dma_iter);

            printf("  dma channels %i speed %i bus_master %i\n", dma_item->channels, dma_item->speed, dma_item->master);

            dma_iter = dma_iter->next(dma_iter);
        }

        dma_iter->destroy(dma_iter);
    }

    if(device->buses) {
        iterator_t* bus_iter = list_iterator_create(device->buses);

        while(!bus_iter->end_of_iterator(bus_iter)) {
            const acpi_aml_device_bus_t* bus_item = bus_iter->get_item(bus_iter);

            printf("  bus range 0x%x-0x%x\n", bus_item->min, bus_item->max);

            bus_iter = bus_iter->next(bus_iter);
        }

        bus_iter->destroy(bus_iter);
    }

    return 0;
}
