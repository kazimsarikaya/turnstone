/**
 * @file usb_xhci.64.c
 * @brief USB XHCI driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <driver/usb_xhci.h>
#include <pci.h>
#include <logging.h>
#include <memory/paging.h>
#include <time/timer.h>
#include <apic.h>
#include <hashmap.h>
#include <pipeline.h>
#include <cpu.h>
#include <cpu/task.h>
#include <strings.h>

MODULE("turnstone.kernel.hw.usb");

typedef struct usb_controller_metadata_t {
    uint64_t                                   controller_id;
    volatile usb_xhci_capabilities_t*          cap;
    volatile usb_xhci_operational_registers_t* op_regs;
    uint32_t                                   context_size;
    uint8_t                                    port_count;
    boolean_t                                  addr64;
    uint64_t*                                  dcbaa;
    uint64_t                                   dcbaa_size;
    frame_t*                                   dcbaa_frame;
    usb_xhci_trb_t*                            cmd_ring;
    uint64_t                                   cmd_ring_size;
    frame_t*                                   cmd_ring_frame;
    int32_t                                    current_cmd_index;
    uint64_t*                                  erst;
    uint64_t                                   erst_size;
    frame_t*                                   erst_frame;
    usb_xhci_trb_t*                            event_ring;
    uint64_t                                   event_ring_size;
    frame_t*                                   event_ring_frame;
    usb_xhci_doorbell_t*                       doorbells;
    usb_xhci_runtime_registers_t*              runtime;
    uint32_t                                   event_cycle_bit;
    uint32_t                                   cycle_bit;
    hashmap_t*                                 slot_id_device_mapping;
    boolean_t                                  interrupter_task_initialized;
    uint64_t                                   interrupter_tid;
    boolean_t                                  port_status_listener_task_initialized;
    uint64_t                                   port_status_listener_tid;
    list_t*                                    port_status_listener_event_queue;
    uint32_t                                   max_psa_size;
} usb_controller_metadata_t;

typedef struct usb_device_controller_context_t {
    struct {
        uint64_t    ep_trb_fa;
        uint64_t    ep_trb_va;
        uint32_t    ep_trb_index;
        frame_t*    ep_trb_frame;
        uint64_t    data_buffer_fa;
        uint64_t    data_buffer_va;
        frame_t*    data_buffer_frame;
        pipeline_t* ep_pipeline;
        uint64_t    max_packet_size_aligned;
        uint64_t    expected_packet_size;
        uint64_t    stream_context_count;
        uint64_t    stream_context_fa;
        uint64_t    stream_context_va;
        frame_t*    stream_context_frame;
        uint32_t    cycle_bit;
    } endpoints[USB_XHCI_MAX_ENDPOINTS];
} usb_device_controller_context_t;

typedef struct usb_driver_t {
    USB_DRIVER_COMMON_FIELDS;
} usb_driver_t;

static hashmap_t* usb_xhci_interrupt_controller_mapping = NULL;
static uint64_t usb_xhci_next_controller_id = 0;

static int8_t usb_xhci_destroy_device_controller_context(usb_controller_t* controller, usb_device_t* device) {
    if(!controller || !device || !device->controller_context) {
        return -1;
    }

    usb_device_controller_context_t* context = device->controller_context;

    PRINTLOG(USB, LOG_DEBUG, "destroying device controller context 0x%p for device 0x%p", context, device);

    frame_allocator_t* fa = frame_get_allocator();

    for(int32_t i = 0; i < USB_XHCI_MAX_ENDPOINTS; i++) {
        if(context->endpoints[i].ep_trb_frame) {
            if(memory_paging_delete_va_for_frame(context->endpoints[i].ep_trb_va,
                                                 context->endpoints[i].ep_trb_frame) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot delete ep trb va for frame");
            }
            fa->release_frame(fa, context->endpoints[i].ep_trb_frame);
        }

        if(context->endpoints[i].data_buffer_frame) {
            if(memory_paging_delete_va_for_frame(context->endpoints[i].data_buffer_va,
                                                 context->endpoints[i].data_buffer_frame) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot delete data buffer va for frame");
            }
            fa->release_frame(fa, context->endpoints[i].data_buffer_frame);
        }

        if(context->endpoints[i].stream_context_frame) {
            if(memory_paging_delete_va_for_frame(context->endpoints[i].stream_context_va,
                                                 context->endpoints[i].stream_context_frame) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot delete stream context va for frame");
            }
            fa->release_frame(fa, context->endpoints[i].stream_context_frame);
        }

        if(context->endpoints[i].ep_pipeline) {
            pipeline_destroy(context->endpoints[i].ep_pipeline);
        }
    }

    memory_free(context);

    device->controller_context = NULL;

    return 0;
}

typedef struct usb_xhci_pool_event_result_t {
    uint64_t            trb_fa;
    usb_xhci_trb_type_t wanted_type;
    usb_xhci_trb_t      out_trb;
    boolean_t           found;
    boolean_t           search;
} usb_xhci_pool_event_result_t;

static usb_xhci_pool_event_result_t usb_xhci_pool_event_result[10] = {
    {0, 0, {0}, false, false},
    {0, 0, {0}, false, false},
};


static void usb_xhci_pool_event_init(uint64_t controller_id, uint64_t trb_fa, usb_xhci_trb_type_t wanted_type) {
    PRINTLOG(USB, LOG_TRACE, "init pool event wait for trb fa 0x%llx type %d", trb_fa, wanted_type);
    memory_memset(&usb_xhci_pool_event_result[controller_id].out_trb, 0, sizeof(usb_xhci_trb_t));
    usb_xhci_pool_event_result[controller_id].trb_fa = trb_fa;
    usb_xhci_pool_event_result[controller_id].wanted_type = wanted_type;
    usb_xhci_pool_event_result[controller_id].found = false;

    asm volatile ("" ::: "memory");

    usb_xhci_pool_event_result[controller_id].search = true;
}

static int8_t usb_xhci_pool_event(uint64_t controller_id) {
    uint64_t timeout = 1000;
    while(timeout--) {
        if(usb_xhci_pool_event_result[controller_id].found) {
            return 0;
        }

        time_timer_msleep(5);
    }

    PRINTLOG(USB, LOG_ERROR, "cannot find event for trb 0x%llx of type %d",
             usb_xhci_pool_event_result[controller_id].trb_fa,
             usb_xhci_pool_event_result[controller_id].wanted_type);

    return -1;
}

void video_text_print(const char_t* str);

static int8_t usb_xhci_isr(interrupt_frame_ext_t* frame) {
    uint64_t vector = frame->interrupt_number - 0x20; // convert to isr
    const usb_controller_t* usb_controller = hashmap_get(usb_xhci_interrupt_controller_mapping, (void*)vector);

    if(!usb_controller) {
        video_text_print("USB XHCI ISR: No controller for vector\n");
        apic_eoi();
        return 0;
    }

    usb_controller_metadata_t* metadata = usb_controller->metadata;

    if(metadata->interrupter_tid) {
        // wake up interrupter task
        task_set_interrupt_received(metadata->interrupter_tid);
    } else {
        video_text_print("USB XHCI ISR: No interrupter task\n");
    }

    metadata->runtime->interrupters[0].iman |= 1; // re-enable interrupts

    apic_eoi();
    return 0;
}

static int8_t usb_xhci_reset_port(usb_controller_t* usb_controller, uint8_t port) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    PRINTLOG(USB, LOG_TRACE, "resetting port %d", port);

    volatile usb_xhci_port_register_set_t* portreg = &metadata->op_regs->portsc[port];
    usb_xhci_portsc_t portsc = (usb_xhci_portsc_t)portreg->portsc;

    portsc.bits.pr = 1;
    portsc.bits.csc = 1;
    portsc.bits.ped = 1;
    portreg->portsc = portsc.raw;

    time_timer_spinsleep(100);

    portsc.bits.pr = 0;
    portreg->portsc = portsc.raw;
    time_timer_spinsleep(100);
    portsc = (usb_xhci_portsc_t)portreg->portsc;

    if(portsc.bits.pr == 1) {
        PRINTLOG(USB, LOG_ERROR, "port %d reset failed", port);
        return -1;
    }

    if(portsc.bits.csc && portsc.bits.ped) {
        portsc.bits.csc = 1;
        portsc.bits.ped = 1;
        portreg->portsc = portsc.raw;
    }

    time_timer_spinsleep(100);

    if(portsc.bits.ped) {
        PRINTLOG(USB, LOG_TRACE, "port %d enabled", port);
    } else {
        PRINTLOG(USB, LOG_TRACE, "port %d disabled", port);
    }

    PRINTLOG(USB, LOG_TRACE, "port %d reset complete", port);

    return portsc.bits.ped ? 1 : 0;
}

static int8_t usb_xhci_reset_all_ports(usb_controller_t* usb_controller) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    boolean_t failed = false;

    for(int16_t i = 0; i < metadata->port_count; i++) {
        if(usb_xhci_reset_port(usb_controller, i) < 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot reset port %d", i);
            failed = true;
        }
    }

    return failed ? -1 : 0;
}

static int8_t usb_xhci_set_slot_and_address(usb_controller_t* usb_controller, usb_transfer_t* transfer){
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    if(!transfer->driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_t* device = transfer->driver->usb_device;
    usb_xhci_trb_t* cmd_rings = metadata->cmd_ring;
    usb_xhci_doorbell_t* doorbells = metadata->doorbells;

    if(!device) {
        PRINTLOG(USB, LOG_ERROR, "no device for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(device->slot_id) {
        PRINTLOG(USB, LOG_ERROR, "device already has slot id %d", device->slot_id);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(device->address) {
        PRINTLOG(USB, LOG_ERROR, "device already has address %d", device->address);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(device->controller_context) {
        PRINTLOG(USB, LOG_ERROR, "device already has controller device context");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_controller_context_t* context =  memory_malloc(sizeof(usb_device_controller_context_t));
    device->controller_context = context;

    if(!context) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for device controller context");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    frame_allocator_t* fa = frame_get_allocator();
    frame_t* ep_ctrl_frame = NULL;

    frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK;

    if(!metadata->addr64) {
        fa_type |= FRAME_ALLOCATION_TYPE_UNDER_4G;
    }

    uint64_t ep0_frame_size = (metadata->cmd_ring_size + 1) * sizeof(usb_xhci_trb_t);
    ep0_frame_size = (ep0_frame_size + FRAME_SIZE - 1) & ~(FRAME_SIZE - 1);
    uint64_t ep0_frame_count = ep0_frame_size / FRAME_SIZE;

    if(fa->allocate_frame_by_count(fa, ep0_frame_count, fa_type, &ep_ctrl_frame, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate frame for ep0 trb");
        memory_free(device->controller_context);
        device->controller_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint64_t ep0_trb_fa = ep_ctrl_frame->frame_address;
    uint64_t ep0_trb_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ep0_trb_fa);

    memory_paging_add_va_for_frame(ep0_trb_va, ep_ctrl_frame,
                                   MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                   MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                   MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE);

    memory_memclean((void*)ep0_trb_va, FRAME_SIZE);

    context->endpoints[0].ep_trb_fa = ep0_trb_fa;
    context->endpoints[0].ep_trb_va = ep0_trb_va;
    context->endpoints[0].ep_trb_index = 0;
    context->endpoints[0].ep_trb_frame = ep_ctrl_frame;
    context->endpoints[0].cycle_bit = 1;

    usb_xhci_trb_t* ep0_trbs = (usb_xhci_trb_t*)ep0_trb_va;

    ep0_trbs[metadata->cmd_ring_size].parameter = ep0_trb_fa;
    ep0_trbs[metadata->cmd_ring_size].status = 0;
    ep0_trbs[metadata->cmd_ring_size].control = ((uint64_t)USB_XHCI_TRB_TYPE_TR_LINK << 10) | (0 << 5) | (0 << 4) | (1 << 1) | context->endpoints[0].cycle_bit;


    uint64_t* dbcbaa = metadata->dcbaa;
    // Set slot
    usb_xhci_trb_t* cur_cmd_ring = &cmd_rings[metadata->current_cmd_index];
    uint64_t cur_cmd_ring_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)cur_cmd_ring);

    cur_cmd_ring->parameter = 0;
    cur_cmd_ring->status = 0;
    cur_cmd_ring->control = ((uint64_t)USB_XHCI_TRB_TYPE_CR_ENABLE_SLOT << 10) | (1 << 5) | metadata->cycle_bit;

    usb_xhci_pool_event_init(metadata->controller_id, cur_cmd_ring_fa, USB_XHCI_TRB_TYPE_ER_COMMAND_COMPLETE);

    doorbells[0].db = 0; // ring doorbell for slot 0 (command ring)

    metadata->current_cmd_index = (metadata->current_cmd_index + 1) % metadata->cmd_ring_size;

    if(usb_xhci_pool_event(metadata->controller_id) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get event for command");
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        memory_free(device->controller_context);
        device->controller_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint8_t cc = (usb_xhci_pool_event_result[metadata->controller_id].out_trb.status >> 24) & 0xFF;

    if(cc != USB_XHCI_TRB_CCODE_CC_SUCCESS) {
        PRINTLOG(USB, LOG_ERROR, "cannot enable slot, completion code: %d", cc);
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        memory_free(device->controller_context);
        device->controller_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    device->slot_id = usb_xhci_pool_event_result[metadata->controller_id].out_trb.control >> 24;

    hashmap_put(metadata->slot_id_device_mapping, (void*)(uintptr_t)device->slot_id, device);

    PRINTLOG(USB, LOG_TRACE, "new device slot id: %d", device->slot_id);

    uint64_t dcb_fa = dbcbaa[device->slot_id];
    uint64_t* dcb = (uint64_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dcb_fa);

    uint32_t* ictx = (uint32_t*)dcb;
    ictx[0] = 0;
    ictx[1] = 0x3;

    uint32_t* slot_ctx = &ictx[8]; // + metadata->context_size / 8;

    if(device->parent) {
        slot_ctx[0] = (1 << 27) | (device->speed << 20) | ((device->port + 1));
        slot_ctx[1] |= (device->parent->port + 1) << 16;
    } else {
        slot_ctx[0] = (1 << 27) | (device->speed << 20) | ((device->port + 1) << 16);
        slot_ctx[1] = (device->port + 1) << 16;
    }

    uint32_t* ep0_ctx = &ictx[16]; // + 2 * metadata->context_size / 8;
    ep0_ctx[0] = 0;
    ep0_ctx[1] = 0x00400026; // Max Packet Size = 64, Endpoint Type = Control
    ep0_ctx[2] = ep0_trb_fa | 1;
    ep0_ctx[3] = ep0_trb_fa >> 32;
    ep0_ctx[4] = 8;

    // Set Address
    cur_cmd_ring = &cmd_rings[metadata->current_cmd_index];
    cur_cmd_ring_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)cur_cmd_ring);
    uint64_t dev_ctx = metadata->dcbaa[device->slot_id];
    if(!dev_ctx) {
        PRINTLOG(USB, LOG_ERROR, "Device context not allocated for slot %d", device->slot_id);
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        hashmap_delete(metadata->slot_id_device_mapping, (void*)(uintptr_t)device->slot_id);
        device->slot_id = 0;
        memory_free(device->controller_context);
        device->controller_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }
    cur_cmd_ring->parameter = dcb_fa;
    cur_cmd_ring->status = 0;
    cur_cmd_ring->control = ((uint64_t)USB_XHCI_TRB_TYPE_CR_ADDRESS_DEVICE << 10) | (device->slot_id << 24) | (1 << 5) | metadata->cycle_bit;

    usb_xhci_pool_event_init(metadata->controller_id, cur_cmd_ring_fa, USB_XHCI_TRB_TYPE_ER_COMMAND_COMPLETE);

    doorbells[0].db = 0; // ring doorbell for slot 0 (command ring)

    metadata->current_cmd_index = (metadata->current_cmd_index + 1) % metadata->cmd_ring_size;

    if(usb_xhci_pool_event(metadata->controller_id) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get event for command");
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        hashmap_delete(metadata->slot_id_device_mapping, (void*)(uintptr_t)device->slot_id);
        device->slot_id = 0;
        memory_free(device->controller_context);
        device->controller_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    cc = (usb_xhci_pool_event_result[metadata->controller_id].out_trb.status >> 24) & 0xFF;

    if(cc != USB_XHCI_TRB_CCODE_CC_SUCCESS) {
        PRINTLOG(USB, LOG_ERROR, "cannot address device, completion code: %d", cc);
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        hashmap_delete(metadata->slot_id_device_mapping, (void*)(uintptr_t)device->slot_id);
        device->slot_id = 0;
        memory_free(device->controller_context);
        device->controller_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    device->address = ictx[3] & 0xFF;
    transfer->request->value = device->address;
    PRINTLOG(USB, LOG_TRACE, "new device address: %d", device->address);

    transfer->complete = true;
    transfer->success = true;

    return 0;
}

static int8_t usb_xhci_evalutate_context(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_xhci_trb_t* cmd_rings = metadata->cmd_ring;
    usb_xhci_doorbell_t* doorbells = metadata->doorbells;

    if(!transfer->driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_t* device = transfer->driver->usb_device;
    usb_driver_t* driver = transfer->driver;
    usb_device_request_t* request = transfer->request;

    if(!device) {
        PRINTLOG(USB, LOG_ERROR, "no device for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(!device->slot_id) {
        PRINTLOG(USB, LOG_ERROR, "device has no slot id");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(!driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint64_t* dbcbaa = metadata->dcbaa;
    uint64_t dcb_fa = dbcbaa[device->slot_id];
    uint64_t* dcb = (uint64_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dcb_fa);

    uint32_t* ictx = (uint32_t*)dcb;
    ictx[0] = 0;
    ictx[1] = request->index; // bitmask of contexts to evaluate

    if(ictx[1] == 0) {
        PRINTLOG(USB, LOG_ERROR, "no context to evaluate");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(ictx[1] & 1) { // slot context
        uint32_t* slot_ctx = &ictx[8]; // + metadata->context_size / 8;

        if(device->parent) {
            slot_ctx[0] = (1 << 27) | (device->speed << 20) | ((device->port + 1));
            slot_ctx[1] |= (device->parent->port + 1) << 16;
        } else {
            slot_ctx[0] = (1 << 27) | (device->speed << 20) | ((device->port + 1) << 16);
            slot_ctx[1] = (device->port + 1) << 16;
        }

        if(device->is_hub) {
            slot_ctx[0] |= (1 << 26); // set hub flag
            slot_ctx[1] |= (device->hub_num_ports & 0xFF) << 24;
        }

    }

    if(ictx[1] & 2) { // endpoint 0 context
        if(!device->controller_context) {
            PRINTLOG(USB, LOG_ERROR, "device has no controller device context");
            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        usb_device_controller_context_t* context = device->controller_context;

        uint32_t* ep0_ctx = &ictx[16]; // + 2 * metadata->context_size / 8;
        ep0_ctx[0] = 0;
        ep0_ctx[1] = 0x00400026; // Max Packet Size = 64, Endpoint Type = Control
        ep0_ctx[2] = context->endpoints[0].ep_trb_fa | 1;
        ep0_ctx[3] = context->endpoints[0].ep_trb_fa >> 32;
        ep0_ctx[4] = 8;
    }

    // Set Address
    usb_xhci_trb_t* cur_cmd_ring = &cmd_rings[metadata->current_cmd_index];
    uint64_t cur_cmd_ring_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)cur_cmd_ring);
    uint64_t dev_ctx = metadata->dcbaa[device->slot_id];
    if(!dev_ctx) {
        PRINTLOG(USB, LOG_ERROR, "Device context not allocated for slot %d", device->slot_id);

        transfer->complete = true;
        transfer->success = false;
        return -1;
    }
    cur_cmd_ring->parameter = dcb_fa;
    cur_cmd_ring->status = 0;
    cur_cmd_ring->control = ((uint64_t)USB_XHCI_TRB_TYPE_CR_EVALUATE_CONTEXT << 10) | (device->slot_id << 24) | (1 << 5) | metadata->cycle_bit;

    usb_xhci_pool_event_init(metadata->controller_id, cur_cmd_ring_fa, USB_XHCI_TRB_TYPE_ER_COMMAND_COMPLETE);

    doorbells[0].db = 0; // ring doorbell for slot 0 (command ring)

    metadata->current_cmd_index = (metadata->current_cmd_index + 1) % metadata->cmd_ring_size;

    if(usb_xhci_pool_event(metadata->controller_id) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get event for command");

        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint8_t cc = (usb_xhci_pool_event_result[metadata->controller_id].out_trb.status >> 24) & 0xFF;

    if(cc != USB_XHCI_TRB_CCODE_CC_SUCCESS) {
        PRINTLOG(USB, LOG_ERROR, "cannot address device, completion code: %d", cc);

        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    device->address = ictx[3] & 0xFF;
    transfer->request->value = device->address;
    PRINTLOG(USB, LOG_TRACE, "new device address: %d", device->address);

    transfer->complete = true;
    transfer->success = true;

    return 0;
}

static int8_t usb_xhci_setup_endpoint(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_driver_t* driver = transfer->driver;

    if(!driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_t* device = driver->usb_device;
    usb_device_request_t* request = transfer->request;

    if(!device) {
        PRINTLOG(USB, LOG_ERROR, "no device for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(!device->slot_id) {
        PRINTLOG(USB, LOG_ERROR, "device has no slot id");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(!device->controller_context) {
        PRINTLOG(USB, LOG_ERROR, "device has no controller device context");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_controller_context_t* context = device->controller_context;

    uint32_t max_packet_size = request->value & 0x1FFFF;
    if(max_packet_size == 0) {
        PRINTLOG(USB, LOG_ERROR, "invalid max packet size 0, real request value 0x%04x", request->value);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_interface_t* interface = driver->interface;

    if(!interface) {
        PRINTLOG(USB, LOG_ERROR, "no interface for driver");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    for(uint32_t i = 0; i < interface->num_endpoints; i++) {
        if(interface->endpoints[i]->desc->endpoint_address == request->index &&
           interface->endpoints[i]->endpoint_companion) {
            max_packet_size *= (interface->endpoints[i]->endpoint_companion->max_burst + 1);
            break;
        }
    }

    if(max_packet_size > 0x1FFFF) {
        PRINTLOG(USB, LOG_ERROR, "invalid max packet size %d", max_packet_size);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    PRINTLOG(USB, LOG_TRACE, "max packet size with burst: %d", max_packet_size);

    uint8_t ep_direction = (request->index >> 7) & 0x01;
    uint8_t ep_index = 2 * (request->index & 0x0F) + ep_direction;

    if(ep_index == 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot setup endpoint 0");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(ep_index > 31) {
        PRINTLOG(USB, LOG_ERROR, "invalid endpoint index %d", ep_index);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(context->endpoints[ep_index - 1].ep_trb_frame) {
        PRINTLOG(USB, LOG_ERROR, "endpoint %d already has trb frame", ep_index);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint64_t dcb_fa = metadata->dcbaa[device->slot_id];
    uint64_t* dcb = (uint64_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dcb_fa);

    if(!dcb_fa) {
        PRINTLOG(USB, LOG_ERROR, "Device context not allocated for slot %d", device->slot_id);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }



    PRINTLOG(USB, LOG_TRACE, "setting up endpoint %d,%d", ep_index, device->slot_id);

    frame_allocator_t* fa = frame_get_allocator();
    frame_t* ep_trb_frame = NULL;

    frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK;

    if(!metadata->addr64) {
        fa_type |= FRAME_ALLOCATION_TYPE_UNDER_4G;
    }

    uint64_t max_packet_size_aligned = (max_packet_size + 15) & ~15;

    uint64_t ep_frame_size = (metadata->cmd_ring_size + 1) * sizeof(usb_xhci_trb_t);
    ep_frame_size = (ep_frame_size + FRAME_SIZE - 1) & ~(FRAME_SIZE - 1);
    uint64_t ep_frame_count = ep_frame_size / FRAME_SIZE;

    PRINTLOG(USB, LOG_TRACE, "allocating %llx frames for ep%d trb", ep_frame_count, ep_index);

    if(fa->allocate_frame_by_count(fa, ep_frame_count, fa_type, &ep_trb_frame, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate frame for ep%d trb", ep_index);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint64_t ep_trb_fa = ep_trb_frame->frame_address;
    uint64_t ep_trb_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ep_trb_fa);
    memory_paging_add_va_for_frame(ep_trb_va, ep_trb_frame,
                                   MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                   MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                   MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE);
    memory_memclean((void*)ep_trb_va, FRAME_SIZE);

    PRINTLOG(USB, LOG_DEBUG, "ep%d trb fa 0x%llx va 0x%llx", ep_index, ep_trb_fa, ep_trb_va);

    context->endpoints[ep_index - 1].ep_trb_fa = ep_trb_fa;
    context->endpoints[ep_index - 1].ep_trb_va = ep_trb_va;
    context->endpoints[ep_index - 1].ep_trb_index = 0;
    context->endpoints[ep_index - 1].ep_trb_frame = ep_trb_frame;
    context->endpoints[ep_index - 1].max_packet_size_aligned = max_packet_size_aligned;
    context->endpoints[ep_index - 1].cycle_bit = 1;

    usb_xhci_trb_t* ep_trbs = (usb_xhci_trb_t*)ep_trb_va;

    ep_trbs[metadata->cmd_ring_size].parameter = ep_trb_fa;
    ep_trbs[metadata->cmd_ring_size].status = 0;
    ep_trbs[metadata->cmd_ring_size].control = ((uint64_t)USB_XHCI_TRB_TYPE_TR_LINK << 10) | (0 << 5) | (0 << 4) | (1 << 1) | context->endpoints[ep_index - 1].cycle_bit;

    uint32_t ep_ctx_dword0 = 0;

    if(request->length) {
        frame_t* stream_context_frame = NULL;
        if(fa->allocate_frame_by_count(fa, 1, fa_type, &stream_context_frame, NULL) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate frame for stream context");
            memory_paging_delete_va_for_frame(ep_trb_va, ep_trb_frame);
            fa->release_frame(fa, ep_trb_frame);
            context->endpoints[ep_index - 1].ep_trb_frame = NULL;
            context->endpoints[ep_index - 1].ep_trb_fa = 0;
            context->endpoints[ep_index - 1].ep_trb_va = 0;
            context->endpoints[ep_index - 1].ep_trb_index = 0;
            transfer->data = NULL;
            transfer->length = 0;
            transfer->complete = true;
            transfer->success = false;
            return -1;
        }
        uint64_t stream_context_fa = stream_context_frame->frame_address;
        uint64_t stream_context_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stream_context_fa);
        memory_paging_add_va_for_frame(stream_context_va, stream_context_frame,
                                       MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                       MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                       MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE);
        memory_memclean((void*)stream_context_va, FRAME_SIZE);

        context->endpoints[ep_index - 1].stream_context_fa = stream_context_fa;
        context->endpoints[ep_index - 1].stream_context_va = stream_context_va;
        context->endpoints[ep_index - 1].stream_context_frame = stream_context_frame;
        context->endpoints[ep_index - 1].stream_context_count = request->length;

        usb_xhci_stream_context_t* sctx = (usb_xhci_stream_context_t*)stream_context_va;

        for(uint32_t s = 0; s < request->length; s++) {
            sctx[s].dequeu_address = ep_trb_fa | (1 << 1) | 1;
        }

        ep_trb_fa = stream_context_fa;

        ep_ctx_dword0 = (1 << 15) | (request->length << 10); // LSA=1, max streams
    }


    uint32_t* ictx = (uint32_t*)dcb;
    ictx[0] = 0;
    ictx[1] = 1 | (1 << ep_index);

    uint32_t* ep_ctx = &ictx[16 + (ep_index - 1) * (metadata->context_size / sizeof(uint32_t))];
    uint32_t ep_type = 2; // Bulk
    if(ep_direction == 1) {
        ep_type |= 0x4; // IN
    }

    ep_ctx[0] = ep_ctx_dword0;
    ep_ctx[1] = (max_packet_size << 16) | (ep_type << 3) | (3 << 1);
    ep_ctx[2] = ep_trb_fa | 1;
    ep_ctx[3] = ep_trb_fa >> 32;
    ep_ctx[4] = 8;

    // Set Address
    usb_xhci_trb_t* cmd_rings = metadata->cmd_ring;
    usb_xhci_doorbell_t* doorbells = metadata->doorbells;
    usb_xhci_trb_t* cur_cmd_ring = &cmd_rings[metadata->current_cmd_index];
    uint64_t cur_cmd_ring_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)cur_cmd_ring);

    cur_cmd_ring->parameter = dcb_fa;
    cur_cmd_ring->status = 0;
    cur_cmd_ring->control = ((uint64_t)USB_XHCI_TRB_TYPE_CR_CONFIGURE_ENDPOINT << 10) | (device->slot_id << 24) | (1 << 5) | metadata->cycle_bit;

    usb_xhci_pool_event_init(metadata->controller_id, cur_cmd_ring_fa, USB_XHCI_TRB_TYPE_ER_COMMAND_COMPLETE);

    doorbells[0].db = 0; // ring doorbell for slot 0 (command ring)
                         //
    metadata->current_cmd_index = (metadata->current_cmd_index + 1) % metadata->cmd_ring_size;

    if(usb_xhci_pool_event(metadata->controller_id) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get event for command");
        memory_paging_delete_va_for_frame(ep_trb_va, ep_trb_frame);
        fa->release_frame(fa, ep_trb_frame);
        context->endpoints[ep_index - 1].ep_trb_frame = NULL;
        context->endpoints[ep_index - 1].ep_trb_fa = 0;
        context->endpoints[ep_index - 1].ep_trb_va = 0;
        context->endpoints[ep_index - 1].ep_trb_index = 0;
        transfer->data = NULL;
        transfer->length = 0;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    int8_t cc = (usb_xhci_pool_event_result[metadata->controller_id].out_trb.status >> 24) & 0xFF;

    if(cc != USB_XHCI_TRB_CCODE_CC_SUCCESS) {
        PRINTLOG(USB, LOG_ERROR, "cannot configure endpoint, completion code: %d", cc);
        memory_paging_delete_va_for_frame(ep_trb_va, ep_trb_frame);
        fa->release_frame(fa, ep_trb_frame);
        context->endpoints[ep_index - 1].ep_trb_frame = NULL;
        context->endpoints[ep_index - 1].ep_trb_fa = 0;
        context->endpoints[ep_index - 1].ep_trb_va = 0;
        context->endpoints[ep_index - 1].ep_trb_index = 0;
        transfer->data = NULL;
        transfer->length = 0;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "endpoint %d setup complete", ep_index);
    transfer->complete = true;
    transfer->success = true;

    return 0;
}

static int8_t usb_xhci_setup_endpoint_pipeline(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    if(!transfer->driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_t* device = transfer->driver->usb_device;
    usb_driver_t* driver = transfer->driver;
    usb_device_request_t* request = transfer->request;

    if(!device) {
        PRINTLOG(USB, LOG_ERROR, "no device for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(!driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(!device->slot_id) {
        PRINTLOG(USB, LOG_ERROR, "device has no slot id");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(!device->controller_context) {
        PRINTLOG(USB, LOG_ERROR, "device has no controller device context");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_controller_context_t* context = device->controller_context;

    uint32_t expected_packet_size = request->value & 0x1FFFF;
    if(expected_packet_size == 0) {
        PRINTLOG(USB, LOG_ERROR, "invalid expected packet size 0, real request value 0x%04x", request->value);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "expected packet size: %d", expected_packet_size);

    uint8_t ep_direction = (request->index >> 7) & 0x01;
    uint8_t ep_index = 2 * (request->index & 0x0F) + ep_direction;

    if(ep_index == 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot setup endpoint 0");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(ep_index > 31) {
        PRINTLOG(USB, LOG_ERROR, "invalid endpoint index %d", ep_index);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    pipeline_t* pipeline = (pipeline_t*)transfer->data;

    if(!pipeline) {
        PRINTLOG(USB, LOG_ERROR, "no pipeline for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    frame_allocator_t* fa = frame_get_allocator();

    frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK;

    if(!metadata->addr64) {
        fa_type |= FRAME_ALLOCATION_TYPE_UNDER_4G;
    }

    uint64_t max_packet_size_aligned = context->endpoints[ep_index - 1].max_packet_size_aligned;
    uint64_t data_buffer_size = max_packet_size_aligned * metadata->cmd_ring_size;
    uint64_t data_buffer_frame_count = (data_buffer_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t* data_buffer_frame = NULL;
    if(fa->allocate_frame_by_count(fa, data_buffer_frame_count, fa_type, &data_buffer_frame, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate frame for ep%d data buffer", ep_index);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint64_t data_buffer_fa = data_buffer_frame->frame_address;
    uint64_t data_buffer_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(data_buffer_fa);
    memory_paging_add_va_for_frame(data_buffer_va, data_buffer_frame,
                                   MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                   MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                   MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE);
    memory_memclean((void*)data_buffer_va, data_buffer_size);

    PRINTLOG(USB, LOG_TRACE, "ep%d,%d data buffer fa 0x%llx va 0x%llx", ep_index, device->slot_id, data_buffer_fa, data_buffer_va);

    context->endpoints[ep_index - 1].ep_trb_index = 0;
    context->endpoints[ep_index - 1].expected_packet_size = expected_packet_size;
    context->endpoints[ep_index - 1].ep_pipeline = pipeline;
    context->endpoints[ep_index - 1].data_buffer_fa = data_buffer_fa;
    context->endpoints[ep_index - 1].data_buffer_va = data_buffer_va;
    context->endpoints[ep_index - 1].data_buffer_frame = data_buffer_frame;
    context->endpoints[ep_index - 1].cycle_bit = 1;

    usb_xhci_trb_t* ep_trbs = (usb_xhci_trb_t*)context->endpoints[ep_index - 1].ep_trb_va;

    for(uint64_t i = 0; i < metadata->cmd_ring_size; i++) {
        ep_trbs[i].parameter = data_buffer_fa + i * max_packet_size_aligned;
        ep_trbs[i].status = expected_packet_size;
        ep_trbs[i].control = ((uint64_t)USB_XHCI_TRB_TYPE_TR_NORMAL << 10) | (1 << 5) | context->endpoints[ep_index - 1].cycle_bit;
    }

    usb_xhci_doorbell_t* doorbells = metadata->doorbells;
    doorbells[device->slot_id].db = ep_index; // ring doorbell for the endpoint

    transfer->complete = true;
    transfer->success = true;

    PRINTLOG(USB, LOG_DEBUG, "endpoint %d pipeline setup complete", ep_index);
    return 0;
}

#if 0
static int8_t usb_xhci_reset_endpoint(usb_controller_t* usb_controller, usb_device_t* device, uint8_t ep_index) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_xhci_trb_t* cmd_rings = metadata->cmd_ring;
    usb_xhci_trb_t* cur_cmd_ring = &cmd_rings[metadata->current_cmd_index];

    cur_cmd_ring->control = (device->slot_id << 24) |
                            (ep_index << 16) |
                            (USB_XHCI_TRB_TYPE_CR_RESET_ENDPOINT << 10) | 1;

    usb_xhci_doorbell_t* doorbells = metadata->doorbells;

    doorbells[0].db = 0; // ring doorbell for slot 0 (command ring)

    metadata->current_cmd_index = (metadata->current_cmd_index + 1) % metadata->cmd_ring_size;

    return 0;
}
#endif

static int8_t usb_xhci_control_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    usb_driver_t* driver = transfer->driver;

    if(!driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_t* device = driver->usb_device;

    if(!device) {
        PRINTLOG(USB, LOG_ERROR, "no device for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_request_t* request = transfer->request;

    usb_request_recipient_t request_recipient = (usb_request_recipient_t)(request->type & USB_REQUEST_RECIPIENT_MASK);
    usb_request_type_t request_type = (usb_request_type_t)(request->type & USB_REQUEST_TYPE_MASK);

    // special handling for some standard requests
    if(request_type == USB_REQUEST_TYPE_STANDARD) {
        if(request_recipient == USB_REQUEST_RECIPIENT_DEVICE) {
            if(request->request == USB_REQUEST_SET_ADDRESS) {
                PRINTLOG(USB, LOG_TRACE, "SET_ADDRESS request");
                return usb_xhci_set_slot_and_address(usb_controller, transfer);
            }

            if(request->request == USB_REQUEST_EVALUATE_CONTEXT) {
                PRINTLOG(USB, LOG_TRACE, "EVALUATE_CONTEXT request");
                return usb_xhci_evalutate_context(usb_controller, transfer);
            }
        }

        if(request_recipient == USB_REQUEST_RECIPIENT_ENDPOINT) {
            if(request->request == USB_ENDPOINT_SETUP_ENDPOINT) {
                PRINTLOG(USB, LOG_TRACE, "SETUP_ENDPOINT request");
                return usb_xhci_setup_endpoint(usb_controller, transfer);
            }

            if(request->request == USB_ENDPOINT_SETUP_PIPELINE) {
                PRINTLOG(USB, LOG_TRACE, "SETUP_ENDPOINT_PIPELINE request");
                return usb_xhci_setup_endpoint_pipeline(usb_controller, transfer);
            }
        }
    }

    uint64_t dbc_fa = metadata->dcbaa[device->slot_id];

    usb_device_controller_context_t* context = device->controller_context;

    uint64_t transfer_ring_fa = context->endpoints[0].ep_trb_fa;
    uint64_t transfer_ring_va = context->endpoints[0].ep_trb_va;

    usb_xhci_trb_t* transfer_ring = (usb_xhci_trb_t*)transfer_ring_va;
    uint32_t trb_index = context->endpoints[0].ep_trb_index;

    PRINTLOG(USB, LOG_TRACE, "XHCI control transfer for slot %d, dev_ctx=0x%llx, transfer_ring=0x%llx, trb_index=%d",
             device->slot_id, dbc_fa, transfer_ring_fa, trb_index);


    void* data = transfer->data;
    uint16_t length = transfer->length;


    transfer_ring[trb_index].parameter = (((uint64_t)request->length) << 48) |
                                         (((uint64_t)request->index) << 32) |
                                         (((uint64_t)request->value) << 16) |
                                         (((uint64_t)request->request) << 8) |
                                         ((uint64_t)request->type);
    transfer_ring[trb_index].status = 8;
    transfer_ring[trb_index].control = (USB_XHCI_TRB_TYPE_TR_SETUP << 10) | (1 << 6) | context->endpoints[0].cycle_bit;

    if((trb_index + 1) == metadata->cmd_ring_size) {
        context->endpoints[0].cycle_bit ^= 1;
    }

    trb_index = (trb_index + 1) % metadata->cmd_ring_size;
    context->endpoints[0].ep_trb_index = trb_index;

    uint32_t dir_bit = (request->type & USB_REQUEST_DIRECTION_DEVICE_TO_HOST) ? (1 << 16) : 0;


    if(length) {
        uint32_t remaining_length = length;
        uint32_t max_packet_size = device->max_packet_size;

        uint64_t data_fa = 0;
        if(memory_paging_get_physical_address((uint64_t)data, &data_fa) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot get data buffer physical address 0x%p", data);

            // Clean up the transfer ring TRB we just added
            trb_index = (trb_index + metadata->cmd_ring_size - 1) % metadata->cmd_ring_size;
            context->endpoints[0].ep_trb_index = trb_index;

            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        // Data Stage TRB (if length > 0)
        while (remaining_length > 0) {
            uint32_t curr_length = MIN(remaining_length, max_packet_size);

            PRINTLOG(USB, LOG_TRACE, "data fa: 0x%llx length 0x%x", data_fa, curr_length);

            transfer_ring[trb_index].parameter = data_fa;
            transfer_ring[trb_index].status = curr_length;
            transfer_ring[trb_index].control = dir_bit | (USB_XHCI_TRB_TYPE_TR_DATA << 10)  | context->endpoints[0].cycle_bit;

            if((trb_index + 1) == metadata->cmd_ring_size) {
                context->endpoints[0].cycle_bit ^= 1;
            }

            trb_index = (trb_index + 1) % metadata->cmd_ring_size;
            context->endpoints[0].ep_trb_index = trb_index;

            data_fa += curr_length;
            remaining_length -= curr_length;
        }

    }


    // Status Stage TRB
    transfer_ring[trb_index].parameter = 0;
    transfer_ring[trb_index].status = 0;
    transfer_ring[trb_index].control = dir_bit | (USB_XHCI_TRB_TYPE_TR_STATUS << 10) | (1 << 5) | context->endpoints[0].cycle_bit;

    uint64_t cur_trb_fa = transfer_ring_fa + trb_index * sizeof(usb_xhci_trb_t);

    if((trb_index + 1) == metadata->cmd_ring_size) {
        context->endpoints[0].cycle_bit ^= 1;
    }

    trb_index = (trb_index + 1) % metadata->cmd_ring_size;
    context->endpoints[0].ep_trb_index = trb_index;

    // Ring doorbell for transfer (Endpoint 0, DCI = 1)
    usb_xhci_doorbell_t* doorbell = metadata->doorbells;

    PRINTLOG(USB, LOG_TRACE, "control transfer: bmRequestType=0x%02x bRequest=0x%02x wValue=0x%04x wIndex=0x%04x wLength=0x%04x",
             request->type, request->request, request->value, request->index, request->length);

    if(device->is_hub &&
       (transfer->request->request == USB_REQUEST_SET_FEATURE ||
        transfer->request->request == USB_REQUEST_CLEAR_FEATURE
       ) &&
       transfer->request->value == USB_HUB_FEATURE_PORT_RESET) {

        uint32_t ep_index = device->hub_status_endpoint_address;
        uint8_t ep_direction = (ep_index >> 7) & 0x01;
        ep_index = 2 * (ep_index & 0x0F) + ep_direction;

        usb_device_controller_context_t* hub_dc_ctx = device->controller_context;

        if(!hub_dc_ctx) {
            PRINTLOG(USB, LOG_ERROR, "cannot get hub controller device context");
            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        usb_xhci_trb_t* ep_trbs = (usb_xhci_trb_t*)hub_dc_ctx->endpoints[ep_index - 1].ep_trb_va;

        if(!ep_trbs) {
            PRINTLOG(USB, LOG_ERROR, "cannot get hub endpoint %d trb", ep_index);
            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        uint32_t hub_ep_trb_index = hub_dc_ctx->endpoints[ep_index - 1].ep_trb_index;

        PRINTLOG(USB, LOG_TRACE, "Adding TRBs to hub status endpoint %d (ep_index %d) trb_index %d",
                 device->hub_status_endpoint_address, ep_index, hub_ep_trb_index);


        ep_trbs[hub_ep_trb_index].parameter = hub_dc_ctx->endpoints[ep_index - 1].ep_trb_fa + hub_dc_ctx->endpoints[ep_index].max_packet_size_aligned * hub_ep_trb_index;
        ep_trbs[hub_ep_trb_index].status = device->max_packet_size;
        ep_trbs[hub_ep_trb_index].control = (USB_XHCI_TRB_TYPE_TR_NORMAL << 10) | (1 << 5) | hub_dc_ctx->endpoints[ep_index - 1].cycle_bit;

        if((hub_ep_trb_index + 1) == metadata->cmd_ring_size) {
            hub_dc_ctx->endpoints[ep_index - 1].cycle_bit ^= 1;
        }

        hub_ep_trb_index = (hub_ep_trb_index + 1) % metadata->cmd_ring_size;

        hub_dc_ctx->endpoints[ep_index - 1].ep_trb_index = hub_ep_trb_index;

    }

    if(!transfer->is_async) {
        usb_xhci_pool_event_init(metadata->controller_id, cur_trb_fa, USB_XHCI_TRB_TYPE_ER_TRANSFER);
    }

    doorbell[device->slot_id].db = 1; // DCI = 1 for Endpoint 0

    PRINTLOG(USB, LOG_TRACE, "XHCI control transfer initiated for slot %d", device->slot_id);

    if(transfer->is_async) {
        transfer->complete = true;
        transfer->success = true;
        return 0;
    }

    if(usb_xhci_pool_event(metadata->controller_id) != 0) {
        PRINTLOG(USB, LOG_ERROR, "control transfer failed, cannot get event for command");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    int8_t cc = (usb_xhci_pool_event_result[metadata->controller_id].out_trb.status >> 24) & 0xFF;

    if(cc != USB_XHCI_TRB_CCODE_CC_SUCCESS) {
        PRINTLOG(USB, LOG_ERROR, "control transfer failed, completion code: %d", cc);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    transfer->complete = true;
    transfer->success = true;

    return 0;
}

static uint32_t usb_xhci_get_microframe_index(usb_controller_t* usb_controller) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    uint32_t mfindex = 0;

    // MFIndex is supported
    mfindex = (metadata->runtime->mfindex & 0x7FF);
    mfindex = mfindex >> 3;

    return mfindex;
}

static int8_t usb_xhci_data_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    if(!transfer->driver) {
        PRINTLOG(USB, LOG_ERROR, "no driver for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_t* device = transfer->driver->usb_device;

    usb_xhci_doorbell_t* doorbell = &metadata->doorbells[device->slot_id];

    usb_device_controller_context_t* context = device->controller_context;

    if(!context) {
        PRINTLOG(USB, LOG_ERROR, "device has no controller device context");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_endpoint_t* endpoint = transfer->endpoint;

    if(!endpoint) {
        PRINTLOG(USB, LOG_ERROR, "no endpoint for transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint8_t ep_addr = endpoint->desc->endpoint_address;
    PRINTLOG(USB, LOG_TRACE, "data transfer on endpoint 0x%02x", ep_addr);
    uint8_t ep_direction = (ep_addr >> 7) & 0x01;
    uint8_t ep_index = 2 * (ep_addr & 0x0F) + ep_direction;

    if(ep_index == 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot do data transfer on endpoint 0");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    if(ep_index > 31) {
        PRINTLOG(USB, LOG_ERROR, "invalid endpoint index %d", ep_index);
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint64_t transfer_ring_fa = context->endpoints[ep_index - 1].ep_trb_fa;
    uint64_t transfer_ring_va = context->endpoints[ep_index - 1].ep_trb_va;

    usb_xhci_trb_t* transfer_ring = (usb_xhci_trb_t*)transfer_ring_va;
    uint32_t trb_index = context->endpoints[ep_index - 1].ep_trb_index;

    uint32_t length = transfer->length;
    void* data = transfer->data;

    PRINTLOG(USB, LOG_TRACE, "XHCI data transfer for slot %d, ep_index %d, transfer_ring=0x%llx trb_index %d",
             device->slot_id, ep_index, transfer_ring_fa, trb_index);

    if(length) {
        uint32_t remaining_length = length;
        uint32_t max_packet_size = endpoint->desc->max_packet_size;

        if(max_packet_size == 0) {
            PRINTLOG(USB, LOG_ERROR, "invalid max packet size 0");
            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        if(endpoint->endpoint_companion) {
            max_packet_size *= (endpoint->endpoint_companion->max_burst + 1);
        }

        if(max_packet_size > 0x1FFFF) {
            PRINTLOG(USB, LOG_ERROR, "invalid max packet size %d", max_packet_size);
            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        uint64_t data_fa = 0;
        if(memory_paging_get_physical_address((uint64_t)data, &data_fa) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot get data buffer physical address 0x%p", data);

            // Clean up the transfer ring TRB we just added
            trb_index = (trb_index + metadata->cmd_ring_size - 1) % metadata->cmd_ring_size;
            context->endpoints[ep_index - 1].ep_trb_index = trb_index;

            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        PRINTLOG(USB, LOG_TRACE, "data buffer fa 0x%llx and length 0x%x", data_fa, length);

        uint32_t mfindex = 0;

        uint32_t ioc = 1;
        uint32_t mfindex_inc = 0;

        if(transfer->is_isochronous) {
            mfindex = usb_xhci_get_microframe_index(usb_controller);
            PRINTLOG(USB, LOG_TRACE, "current microframe index: %d", mfindex);
            ioc = 0;
        }

        while (remaining_length > 0) {
            uint32_t curr_length = MIN(remaining_length, max_packet_size);

            // PRINTLOG(USB, LOG_TRACE, "data fa: 0x%llx length 0x%x", data_fa, curr_length);

            usb_xhci_trb_type_t trb_type = USB_XHCI_TRB_TYPE_TR_NORMAL;

            if(transfer->is_isochronous) {
                trb_type = USB_XHCI_TRB_TYPE_TR_ISOCH;
                mfindex_inc++;
                if(mfindex_inc == 8) {
                    mfindex = (mfindex + 1); // % 8;
                    mfindex_inc = 0;
                }
            }

            transfer_ring[trb_index].parameter = data_fa;
            transfer_ring[trb_index].status = curr_length;
            transfer_ring[trb_index].control = (mfindex << 20) | (trb_type << 10) | (ioc << 5)  | context->endpoints[ep_index - 1].cycle_bit;


            uint64_t cur_trb_fa = transfer_ring_fa + trb_index * sizeof(usb_xhci_trb_t);

            if(!transfer->is_async) {
                usb_xhci_pool_event_init(metadata->controller_id, cur_trb_fa, USB_XHCI_TRB_TYPE_ER_TRANSFER);
            }

            doorbell->db =  (transfer->stream_id << 16) | ep_index; // DCI = ep_index

            if((trb_index + 1) == metadata->cmd_ring_size) {
                context->endpoints[ep_index - 1].cycle_bit ^= 1;
            }

            trb_index = (trb_index + 1) % metadata->cmd_ring_size;
            context->endpoints[ep_index - 1].ep_trb_index = trb_index;

            if(!transfer->is_async) {
                if(usb_xhci_pool_event(metadata->controller_id) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot get event for trb");
                    transfer->complete = true;
                    transfer->success = false;
                    return -1;
                }

                int8_t cc = (usb_xhci_pool_event_result[metadata->controller_id].out_trb.status >> 24) & 0xFF;

                if(cc != USB_XHCI_TRB_CCODE_CC_SUCCESS &&
                   cc != USB_XHCI_TRB_CCODE_CC_SHORT_PACKET) {
                    PRINTLOG(USB, LOG_ERROR, "data transfer failed, completion code: %d", cc);
                    transfer->complete = true;
                    transfer->success = false;
                    return -1;
                }
            }

            data_fa += curr_length;
            remaining_length -= curr_length;
        }
    } else {
        PRINTLOG(USB, LOG_ERROR, "zero-length data transfer");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    PRINTLOG(USB, LOG_TRACE, "XHCI data transfer complete for slot %d", device->slot_id);

    transfer->complete = true;
    transfer->success = true;

    return 0;
}

static void usb_xhci_interrupter_task_handle_stall(usb_controller_metadata_t* metadata,
                                                   usb_xhci_trb_t*            event_trb) {
    uint32_t ep_id = (event_trb->control >> 16) & 0x1F;
    uint32_t slot_id = (event_trb->control >> 24) & 0xFF;

    if(slot_id == 0) {
        PRINTLOG(USB, LOG_WARNING, "invalid slot id 0 for ep id %d", ep_id);
        return;
    }

    if(ep_id == 0 || ep_id > USB_XHCI_MAX_ENDPOINTS) {
        PRINTLOG(USB, LOG_WARNING, "invalid ep id %d for slot id %d", ep_id, slot_id);
        return;
    }

    uint32_t ep_id_at_interface = (ep_id >> 1) | ((ep_id & 1) << 7);

    if(ep_id_at_interface == 0x80) {
        ep_id_at_interface = 0; // control endpoint
    }

    usb_device_t* device = (usb_device_t*)hashmap_get(metadata->slot_id_device_mapping, (void*)(uint64_t)slot_id);

    if(!device) {
        // PRINTLOG(USB, LOG_TRACE, "no device for slot id %d ep id %d", slot_id, ep_id);
        return;
    }

    /*
       if(usb_xhci_reset_endpoint(device->controller, device, ep_id) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot reset endpoint %d for device 0x%p slot id %d", ep_id, device, slot_id);
       } else {
        PRINTLOG(USB, LOG_DEBUG, "reset endpoint %d for device 0x%p slot id %d", ep_id, device, slot_id);
       }

       time_timer_sleep(50); // wait for 50ms
     */


    usb_device_request_t usb_request = {0};

    usb_request.type = USB_REQUEST_TYPE_STANDARD | USB_REQUEST_RECIPIENT_ENDPOINT | USB_REQUEST_DIRECTION_HOST_TO_DEVICE;
    usb_request.request = USB_REQUEST_CLEAR_FEATURE;
    usb_request.index = ep_id_at_interface;

    usb_transfer_t usb_transfer = {0};

    usb_driver_t dummy_driver = {0};
    dummy_driver.usb_device = device;

    usb_transfer.driver = &dummy_driver;
    usb_transfer.request = &usb_request;
    usb_transfer.is_async = true;

    if(usb_xhci_control_transfer(device->controller, &usb_transfer) != 0 || !usb_transfer.success) {
        PRINTLOG(USB, LOG_ERROR, "cannot clear stall for endpoint %d for device 0x%p slot id %d", ep_id, device, slot_id);
    } else {
        PRINTLOG(USB, LOG_DEBUG, "clear stall for endpoint %d for device 0x%p slot id %d", ep_id, device, slot_id);
    }


}

static void usb_xhci_interrupter_task_handle_er_transfer(usb_controller_metadata_t* metadata,
                                                         usb_xhci_trb_t*            event_trb) {
    uint32_t cc = (event_trb->status >> 24) & 0xFF;
    uint32_t ep_id = (event_trb->control >> 16) & 0x1F;
    uint32_t slot_id = (event_trb->control >> 24) & 0xFF;

    if(slot_id == 0) {
        PRINTLOG(USB, LOG_WARNING, "invalid slot id 0 for ep id %d", ep_id);
        return;
    }

    if(ep_id == 0 || ep_id > USB_XHCI_MAX_ENDPOINTS) {
        PRINTLOG(USB, LOG_WARNING, "invalid ep id %d for slot id %d", ep_id, slot_id);
        return;
    }

    const usb_device_t* device = hashmap_get(metadata->slot_id_device_mapping, (void*)(uint64_t)slot_id);

    if(!device) {
        // PRINTLOG(USB, LOG_TRACE, "no device for slot id %d ep id %d", slot_id, ep_id);
        return;
    }

    if(!device->configurations) {
        // PRINTLOG(USB, LOG_WARNING, "no configurations for device 0x%p slot id %d ep id %d", device, slot_id, ep_id);
        return;
    }

    uint32_t ep_id_at_interface = (ep_id >> 1) | ((ep_id & 1) << 7);

    if(ep_id_at_interface == 0x80) {
        ep_id_at_interface = 0; // control endpoint
    }

    usb_config_t* config = device->configurations[device->selected_config];

    if(!config) {
        // PRINTLOG(USB, LOG_WARNING, "no config for device 0x%p slot id %d ep id %d", device, slot_id, ep_id);
        return;
    }

    usb_driver_t* driver = NULL;

    for(uint32_t i = 0; i < config->num_interfaces; i++) {
        usb_interface_t* interface = config->interfaces[i];

        if(!interface || !interface->driver) {
            continue;
        }

        for(uint32_t j = 0; j < interface->num_endpoints; j++) {
            usb_endpoint_t* ep = interface->endpoints[j];

            if(!ep || !ep->desc) {
                continue;
            }

            if(ep->desc->endpoint_address == ep_id_at_interface) {
                driver = interface->driver;
                break;
            }
        }

        if(driver) {
            break;
        }
    }

    if(!driver) {
        // PRINTLOG(USB, LOG_TRACE, "no driver for device 0x%p slot id %d ep id %d", device, slot_id, ep_id);
        return;
    }

    usb_device_controller_context_t* context = device->controller_context;
    pipeline_t* ep_pipeline = context->endpoints[ep_id - 1].ep_pipeline;
    uint64_t ep_trb_fa = context->endpoints[ep_id - 1].ep_trb_fa;
    uint64_t ep_trb_va = context->endpoints[ep_id - 1].ep_trb_va;
    uint64_t expected_packet_size = context->endpoints[ep_id - 1].expected_packet_size;
    uint64_t ep_trb_index = (event_trb->parameter - ep_trb_fa) / sizeof(usb_xhci_trb_t);
    usb_xhci_trb_t* ep_trbs = (usb_xhci_trb_t*)ep_trb_va;

    if(ep_trb_index == 0) {
        // last trb in the ring is metadata->cmd_ring_size (link trb) its cycle bit should be same as trb 0 cycle bit
        PRINTLOG(USB, LOG_TRACE, "resetting cycle bit to %i for link trb at index %lli for ep id %d at controller %lli and slot %d fa 0x%llx",
                 ep_trbs[0].control & 1,
                 metadata->cmd_ring_size,
                 ep_id,
                 metadata->controller_id,
                 slot_id,
                 ep_trb_fa);
        ep_trbs[metadata->cmd_ring_size].control &= ~(1 << 0);
        ep_trbs[metadata->cmd_ring_size].control |= ep_trbs[ep_trb_index].control & 1;
    }

    if(cc != USB_XHCI_TRB_CCODE_CC_SUCCESS &&
       cc != USB_XHCI_TRB_CCODE_CC_SHORT_PACKET) {
        PRINTLOG(USB, LOG_WARNING, "transfer failed with completion code %d for device 0x%p slot id %d ep id %d",
                 cc, device, slot_id, ep_id);
        return;
    }

    if(!ep_pipeline || !driver->pipeline_callback) {
        // PRINTLOG(USB, LOG_TRACE, "no pipeline or driver callback for device 0x%p ep %d,%d index %lli parameter 0x%llx",
        // device, ep_id, slot_id, ep_trb_index, event_trb->parameter);
        return;
    }

    uint64_t data_ptr = ep_trbs[ep_trb_index].parameter;

    if(!data_ptr) {
        PRINTLOG(USB, LOG_WARNING, "no data ptr for trb");
        return;
    }

    uint32_t data_len = ep_trbs[ep_trb_index].status & 0xFFFFFF;
    data_ptr = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(data_ptr);
    uint8_t* data = (uint8_t*)(uintptr_t)data_ptr;

    if(cc == USB_XHCI_TRB_CCODE_CC_SHORT_PACKET) {
        uint32_t remaining_length = event_trb->status & 0xFFFFFF;

        if(remaining_length > data_len) {
            PRINTLOG(USB, LOG_WARNING, "short packet remaining length %d greater than data length %d",
                     remaining_length, data_len);
            return;
        }

        data_len -= remaining_length;

        if(!data_len) {
            PRINTLOG(USB, LOG_WARNING, "short packet with zero data length");
            return;
        }
    }

    if(data_len != expected_packet_size) {
        PRINTLOG(USB, LOG_TRACE, "data length %d does not match expected %lli",
                 data_len, expected_packet_size);
    }

    if(pipeline_write(ep_pipeline, data_len, data) != data_len) {
        PRINTLOG(USB, LOG_WARNING, "cannot write all data to pipeline");
    }

    driver->pipeline_callback(driver, ep_id_at_interface, ep_pipeline);

    if(ep_trb_index == metadata->cmd_ring_size - 1) {
        // last trb in the ring next one is the link trb so we need to flip the cycle bit
        PRINTLOG(USB, LOG_TRACE, "toggling cycle bit to %i for all trbs in ring for ep id %d",
                 context->endpoints[ep_id - 1].cycle_bit ^ 1,
                 ep_id);
        context->endpoints[ep_id - 1].cycle_bit ^= 1;
    }

    if(ep_trb_index != metadata->cmd_ring_size) {
        ep_trbs[ep_trb_index].control ^= (1 << 0);
    }

}

static uint64_t usb_xhci_interrupter_task_handle_events(usb_controller_metadata_t* metadata,
                                                        uint64_t                   event_index) {
    usb_xhci_trb_t* event_rings = metadata->event_ring;

    while(true) {
        usb_xhci_trb_t* event_trb = &event_rings[event_index];

        usb_xhci_trb_type_t trb_type = (event_trb->control >> 10) & 0x3F;
        uint32_t cc = (event_trb->status >> 24) & 0xFF;
        uint32_t event_trb_cycle_bit = (event_trb->control >> 0) & 0x01;

        if(event_trb_cycle_bit != metadata->event_cycle_bit) {
            // PRINTLOG(USB, LOG_TRACE, "no more events to process at index %lli, trb type %d",
            // event_index, trb_type);
            break;
        }

        if(trb_type == USB_XHCI_TRB_TYPE_TRB_RESERVED) {
            PRINTLOG(USB, LOG_ERROR, "reserved trb type at index %lli, stopping event processing",
                     event_index);

            event_index++;
            if(event_index >= metadata->event_ring_size) {
                event_index = 0;
                metadata->event_cycle_bit ^= 1;
            }

            continue;
        }

        if(trb_type == USB_XHCI_TRB_TYPE_ER_HOST_CONTROLLER) {
            if(cc == USB_XHCI_TRB_CCODE_CC_EVENT_RING_FULL_ERROR) {
                PRINTLOG(USB, LOG_WARNING, "event ring full error at index %lli, trb type %d",
                         event_index, trb_type);

            } else if(cc == USB_XHCI_TRB_CCODE_CC_RING_UNDERRUN) {
                PRINTLOG(USB, LOG_WARNING, "event ring underrun at index %lli, trb type %d",
                         event_index, trb_type);

            } else if(cc == USB_XHCI_TRB_CCODE_CC_RING_OVERRUN) {
                PRINTLOG(USB, LOG_WARNING, "event ring overrun at index %lli, trb type %d",
                         event_index, trb_type);

            } else {
                PRINTLOG(USB, LOG_WARNING, "unhandled host controller event at index %lli, trb type %d cc %d",
                         event_index, trb_type, cc);
            }

            event_index++;
            if(event_index >= metadata->event_ring_size) {
                event_index = 0;
                metadata->event_cycle_bit ^= 1;
            }

            continue;
        }

        if(usb_xhci_pool_event_result[metadata->controller_id].search &&
           trb_type == usb_xhci_pool_event_result[metadata->controller_id].wanted_type &&
           event_trb->parameter == usb_xhci_pool_event_result[metadata->controller_id].trb_fa) {
            usb_xhci_pool_event_result[metadata->controller_id].search = false;
            usb_xhci_pool_event_result[metadata->controller_id].out_trb = *event_trb;

            asm volatile ("" ::: "memory");

            usb_xhci_pool_event_result[metadata->controller_id].found = true;
        }

        if(cc == USB_XHCI_TRB_CCODE_CC_STALL_ERROR) {
            PRINTLOG(USB, LOG_WARNING, "stall error at index %lli, trb type %d",
                     event_index, trb_type);
            usb_xhci_interrupter_task_handle_stall(metadata, event_trb);
        }

        if(trb_type == USB_XHCI_TRB_TYPE_ER_TRANSFER) {
            usb_xhci_interrupter_task_handle_er_transfer(metadata, event_trb);
        } else if(trb_type == USB_XHCI_TRB_TYPE_ER_PORT_STATUS_CHANGE) {
            if(cc == USB_XHCI_TRB_CCODE_CC_SUCCESS) {
                uint64_t port_id = event_trb->parameter >> 24;;

                if(port_id == 0 || port_id > metadata->port_count) {
                    PRINTLOG(USB, LOG_WARNING, "invalid port id %lli in port status change event", port_id);
                } else {
                    PRINTLOG(USB, LOG_DEBUG, "port status change event on port id %lli", port_id);

                    if(metadata->port_status_listener_task_initialized &&
                       metadata->port_status_listener_event_queue) {
                        list_queue_push(metadata->port_status_listener_event_queue, (void*)port_id);
                    }
                }
            } else {
                PRINTLOG(USB, LOG_WARNING, "port status change event with error cc %d", cc);
            }

        } else {
            PRINTLOG(USB, LOG_WARNING, "unhandled event trb type %d cc %d param 0x%llx status 0x%x control 0x%x",
                     trb_type, cc, event_trb->parameter, event_trb->status, event_trb->control);
        }


        event_index++;
        if(event_index >= metadata->event_ring_size) {
            event_index = 0;
            metadata->event_cycle_bit ^= 1;
        }
    }

    return event_index;
}

static int8_t usb_xhci_interrupter_task(int32_t argc, void** argv) {
    PRINTLOG(USB, LOG_DEBUG, "xhci interrupter task 0x%p started", usb_xhci_interrupter_task);
    if(argc != 1 || argv == NULL || argv[0] == NULL) {
        PRINTLOG(USB, LOG_ERROR, "invalid argument count");

        return -1;
    }

    usb_controller_t* usb_controller = (usb_controller_t*)argv[0];

    cpu_cli();
    pci_msix_update_lapic((pci_generic_device_t*)usb_controller->pci_dev->pci_header, usb_controller->msix_cap, 0);
    task_set_interruptible();
    task_set_interrupt_receive_workaround(1000);
    cpu_sti();

    usb_controller_metadata_t* metadata = (usb_controller_metadata_t*)usb_controller->metadata;

    if(!metadata) {
        PRINTLOG(USB, LOG_ERROR, "no metadata for controller");
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "interrupter task for controller-%lli 0x%p started",
             usb_controller->metadata->controller_id, usb_controller);
    metadata->interrupter_task_initialized = true;

    while(true) {
        if(task_set_message_waiting()) {
            task_yield();
        }

        uint64_t event_ring_fa = metadata->erst[0];

        uint64_t erdp = metadata->runtime->interrupters[0].erdp;
        // clear first 4 bits
        erdp &= ~0xFULL;

        uint64_t event_index = ((erdp - event_ring_fa) / sizeof(usb_xhci_trb_t));

        if(event_index >= metadata->event_ring_size) {
            event_index = 0;
        }

        event_index = usb_xhci_interrupter_task_handle_events(metadata, event_index);

        erdp = event_ring_fa + event_index * sizeof(usb_xhci_trb_t);
        metadata->runtime->interrupters[0].erdp = (erdp & ~0xFULL) | (1ULL << 3);
    }

    return 0;
}


static int8_t usb_xhci_port_status_listener_task(int32_t argc, void** argv) {
    PRINTLOG(USB, LOG_DEBUG, "xhci port status listener task 0x%p started", usb_xhci_interrupter_task);
    if(argc != 1 || argv == NULL || argv[0] == NULL) {
        PRINTLOG(USB, LOG_ERROR, "invalid argument count");

        return -1;
    }

    usb_controller_t* usb_controller = (usb_controller_t*)argv[0];

    usb_controller_metadata_t* metadata = (usb_controller_metadata_t*)usb_controller->metadata;

    if(!metadata) {
        PRINTLOG(USB, LOG_ERROR, "no metadata for controller");
        return -1;
    }

    list_t* event_queue = list_create_queue();

    if(!event_queue) {
        PRINTLOG(USB, LOG_ERROR, "cannot create event queue");
        return -1;
    }

    task_add_message_queue(event_queue);

    metadata->port_status_listener_event_queue = event_queue;

    PRINTLOG(USB, LOG_INFO, "port status listener task for controller-%lli 0x%p started",
             usb_controller->metadata->controller_id, usb_controller);
    metadata->port_status_listener_task_initialized = true;

    while(true) {
        if(list_size(event_queue) == 0) {
            task_set_message_waiting();
            task_yield();
        }

        uint64_t port = (uint64_t)list_queue_pop(event_queue);

        if(port == 0 || port > metadata->port_count) {
            PRINTLOG(USB, LOG_WARNING, "invalid port id %lli in port status change event", port);
            continue;
        }

        PRINTLOG(USB, LOG_DEBUG, "handling port status change event on port id %lli", port);

        port--; // port numbers are 1-based

        volatile usb_xhci_port_register_set_t* portreg = &metadata->op_regs->portsc[port];
        usb_xhci_portsc_t portsc = (usb_xhci_portsc_t)portreg->portsc;

        PRINTLOG(USB, LOG_DEBUG, "port %lli status: 0x%08x", port, portsc.raw);

        if(!portsc.bits.csc) {
            PRINTLOG(USB, LOG_DEBUG, "no connect status change on port %lli", port);
            // TODO: maybe we should check other status change bits
            // like psc (port status change) or prc (port reset change)
        }

        if(portsc.bits.ped) {
            PRINTLOG(USB, LOG_DEBUG, "port %lli enabled. we will initialize driver", port);

            if(usb_device_init(NULL, usb_controller, port, portsc.bits.ps) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot initialize device on port %lli", port);
            }
        } else {
            PRINTLOG(USB, LOG_DEBUG, "port %lli disabled. we will remove driver", port);
        }

    }

    return 0;
}


uint32_t usb_xhci_get_max_psa_size(usb_controller_t* usb_controller) {
    if(!usb_controller || !usb_controller->metadata) {
        return 0;
    }

    usb_controller_metadata_t* metadata = usb_controller->metadata;

    return metadata->max_psa_size;
}

int8_t usb_xhci_init(usb_controller_t* usb_controller) {
    PRINTLOG(USB, LOG_INFO, "initializing XHCI controller");

    if(!usb_controller || !usb_controller->pci_dev) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");

        return -1;
    }

    if(usb_xhci_interrupt_controller_mapping == NULL) {
        usb_xhci_interrupt_controller_mapping = hashmap_integer(16);
        if(usb_xhci_interrupt_controller_mapping == NULL) {
            PRINTLOG(USB, LOG_ERROR, "cannot create interrupt controller mapping hashmap");
            return -1;
        }
    }


    const pci_dev_t* pci_dev = usb_controller->pci_dev;

    usb_controller_metadata_t* metadata = memory_malloc(sizeof(usb_controller_metadata_t));

    if(!metadata) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb controller metadata");

        return -1;
    }

    metadata->slot_id_device_mapping = hashmap_integer(16);

    if(!metadata->slot_id_device_mapping) {
        PRINTLOG(USB, LOG_ERROR, "cannot create slot id to device mapping hashmap");
        memory_free(metadata);
        return -1;
    }

    usb_controller->metadata = metadata;

    metadata->controller_id = usb_xhci_next_controller_id++;

    PRINTLOG(USB, LOG_DEBUG, "XHCI controller found: %02x:%02x:%02x:%02x",
             pci_dev->group_number, pci_dev->bus_number, pci_dev->device_number, pci_dev->function_number);

    uint64_t bar_fa = pci_get_bar_address((pci_generic_device_t*)pci_dev->pci_header, 0);
    PRINTLOG(USB, LOG_DEBUG, "XHCI BAR address: 0x%016llx", bar_fa);

    uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);
    PRINTLOG(USB, LOG_DEBUG, "XHCI BAR virtual address: 0x%016llx", bar_va);

    usb_xhci_capabilities_t* xhci_cap = (usb_xhci_capabilities_t*)bar_va;

    metadata->cap = xhci_cap;

    usb_xhci_caplen_rev_t caplen_rev  = (usb_xhci_caplen_rev_t)xhci_cap->caplength_and_revision;

    PRINTLOG(USB, LOG_DEBUG, "XHCI capability length: %d", caplen_rev.bits.capability_length);
    PRINTLOG(USB, LOG_DEBUG, "XHCI revision: 0x%04x", caplen_rev.bits.usb_revision);

    usb_xhci_hcs_params_1_t hcs_params1 = (usb_xhci_hcs_params_1_t)xhci_cap->hcs_params_1;

    PRINTLOG(USB, LOG_DEBUG, "XHCI number of device slots: %d", hcs_params1.bits.max_slots);
    PRINTLOG(USB, LOG_DEBUG, "XHCI number of interrupter: %d", hcs_params1.bits.max_interrupts);
    PRINTLOG(USB, LOG_DEBUG, "XHCI number of ports: %d", hcs_params1.bits.max_ports);

    metadata->port_count = hcs_params1.bits.max_ports;

    usb_xhci_hcs_params_2_t hcs_params2 = (usb_xhci_hcs_params_2_t)xhci_cap->hcs_params_2;

    PRINTLOG(USB, LOG_DEBUG, "XHCI max erst size: %d", hcs_params2.bits.erst_max);

    usb_xhci_hcc_params_1_t hcc_params1 = (usb_xhci_hcc_params_1_t)xhci_cap->hcc_params_1;

    PRINTLOG(USB, LOG_DEBUG, "XHCI ac64: %d", hcc_params1.bits.ac64);
    metadata->addr64 = hcc_params1.bits.ac64 ? true : false;
    PRINTLOG(USB, LOG_DEBUG, "XHCI context size: %d", hcc_params1.bits.csz);
    metadata->context_size = hcc_params1.bits.csz ? 64 : 32;
    uint64_t extended_caps_offset = hcc_params1.bits.xecp << 2;
    PRINTLOG(USB, LOG_DEBUG, "XHCI extended capabilities pointer: 0x%016llx", extended_caps_offset);
    metadata->max_psa_size = hcc_params1.bits.maxpsasize;

    if(metadata->max_psa_size) {
        metadata->max_psa_size = 1 << (metadata->max_psa_size + 1);
        PRINTLOG(USB, LOG_DEBUG, "XHCI max primary stream array size: %d", metadata->max_psa_size);
    } else {
        PRINTLOG(USB, LOG_DEBUG, "XHCI primary stream array not supported");
    }

    PRINTLOG(USB, LOG_DEBUG, "XHCI doorbell offset: 0x%08x", xhci_cap->dboff);
    PRINTLOG(USB, LOG_DEBUG, "XHCI runtime register space offset: 0x%08x", xhci_cap->rtsoff);

    uint64_t opregs_va = bar_va + caplen_rev.bits.capability_length;

    usb_xhci_operational_registers_t* opregs = (usb_xhci_operational_registers_t*)opregs_va;

    metadata->op_regs = opregs;

    // Stop the controller if running
    usb_xhci_usbcmd_t usbcmd = (usb_xhci_usbcmd_t)opregs->usbcmd;
    usbcmd.bits.run_stop = 0;
    opregs->usbcmd = usbcmd.raw;

    usb_xhci_usbsts_t usbsts = (usb_xhci_usbsts_t)opregs->usbsts;

    PRINTLOG(USB, LOG_DEBUG, "XHCI usb status is halted: %d", usbsts.bits.hch);

    // Wait for controller to halt
    while (!usbsts.bits.hch) {
        usbsts = (usb_xhci_usbsts_t)opregs->usbsts;
    }

    // Reset the controller
    usbcmd.bits.hcrst = 1;
    opregs->usbcmd = usbcmd.raw;

    // Wait for reset to complete (CNR = 0)
    uint32_t timeout = 1000000;
    while (((usb_xhci_usbsts_t)opregs->usbsts).bits.cnr && timeout--) {
        time_timer_spinsleep(100);
        usbsts = (usb_xhci_usbsts_t)opregs->usbsts;
    }
    if (timeout == 0) {
        PRINTLOG(USB, LOG_ERROR, "XHCI reset timeout");
        return -1;
    }
    PRINTLOG(USB, LOG_DEBUG, "XHCI controller reset complete");

    // Configure max slots
    usb_xhci_config_t config = {0};
    config.bits.max_slots_en = hcs_params1.bits.max_slots;
    opregs->config = config.raw;

    frame_allocator_t* fa = frame_get_allocator();

    frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK;

    if(!metadata->addr64) {
        fa_type |= FRAME_ALLOCATION_TYPE_UNDER_4G;
    }

    // Allocate Device Context Base Address Array (DCBAA)
    uint64_t dcbaa_size = (hcs_params1.bits.max_slots + 1) * 8;
    uint64_t dcbaa_fa_count = (dcbaa_size + FRAME_SIZE - 1) / FRAME_SIZE;
    uint64_t tmp_dcbaa_fa_count = dcbaa_fa_count;
    uint64_t dcb_size = FRAME_SIZE * hcs_params1.bits.max_slots;
    dcbaa_fa_count += (dcb_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t* dcbaa_frames = NULL;

    if (fa->allocate_frame_by_count(fa, dcbaa_fa_count, fa_type, &dcbaa_frames, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to allocate frames for DCBAA");
        return -1;
    }
    uint64_t dcbaa_fa = dcbaa_frames->frame_address;
    uint64_t dcbaa_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dcbaa_fa);

    if (memory_paging_add_va_for_frame(dcbaa_va, dcbaa_frames,
                                       MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                       MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                       MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to map DCBAA frames to virtual address");
        fa->release_frame(fa, dcbaa_frames);
        return -1;
    }

    memory_memclean((void*)dcbaa_va, dcbaa_fa_count * FRAME_SIZE);
    uint64_t* dcbaa = (uint64_t*)dcbaa_va;

    uint64_t dcb_fa = dcbaa_fa + (tmp_dcbaa_fa_count * FRAME_SIZE);

    for (uint32_t i = 0; i < hcs_params1.bits.max_slots; i++) {
        dcbaa[i + 1] = dcb_fa + (i * FRAME_SIZE);
    }

    opregs->dcbaap = dcbaa_fa;
    metadata->dcbaa = dcbaa;
    metadata->dcbaa_size = dcbaa_size;
    metadata->dcbaa_frame = dcbaa_frames;

    PRINTLOG(USB, LOG_DEBUG, "XHCI DCBAA initialized at 0x%016llx", dcbaa_va);

    // Allocate Command Ring
    metadata->cmd_ring_size = 255;
    uint64_t cmd_ring_size = (metadata->cmd_ring_size + 1) * sizeof(usb_xhci_trb_t);
    uint64_t cmd_ring_fa_count = (cmd_ring_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t* cmd_ring_frames = NULL;

    if (fa->allocate_frame_by_count(fa, cmd_ring_fa_count, fa_type, &cmd_ring_frames, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to allocate frames for Command Ring");
        return -1;
    }
    uint64_t cmd_ring_fa = cmd_ring_frames->frame_address;
    uint64_t cmd_ring_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(cmd_ring_fa);

    if (memory_paging_add_va_for_frame(cmd_ring_va, cmd_ring_frames,
                                       MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                       MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                       MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to map Command Ring frames to virtual address");
        fa->release_frame(fa, cmd_ring_frames);
        return -1;
    }

    memory_memclean((void*)cmd_ring_va, cmd_ring_fa_count * FRAME_SIZE);

    usb_xhci_trb_t* cmd_ring = (usb_xhci_trb_t*)cmd_ring_va;

    usb_xhci_crcr_t crcr = { .bits.ptr = cmd_ring_fa >> 6, .bits.rcs = 1 };
    opregs->crcr = crcr.raw;
    metadata->cmd_ring = cmd_ring;
    metadata->cmd_ring_frame = cmd_ring_frames;
    metadata->current_cmd_index = 0;
    PRINTLOG(USB, LOG_DEBUG, "XHCI Command Ring initialized at 0x%016llx", cmd_ring_va);

    // Allocate Event Ring Segment Table and Event Ring
    frame_t* erst_frame = NULL;

    if (fa->allocate_frame_by_count(fa, 1, fa_type, &erst_frame, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to allocate frame for ERST");
        return -1;
    }

    uint64_t erst_fa = erst_frame->frame_address;
    uint64_t erst_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(erst_fa);

    if (memory_paging_add_va_for_frame(erst_va, erst_frame,
                                       MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                       MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                       MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to map ERST frame to virtual address");
        fa->release_frame(fa, erst_frame);
        return -1;
    }

    memory_memclean((void*)erst_va, FRAME_SIZE);

    uint64_t* erst = (uint64_t*)erst_va;
    metadata->erst = erst;
    metadata->erst_size = FRAME_SIZE;
    metadata->erst_frame = erst_frame;

    metadata->event_ring_size = 255;
    uint64_t event_ring_size = (metadata->event_ring_size + 1) * sizeof(usb_xhci_trb_t);
    uint64_t event_ring_fa_count = (event_ring_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t* event_ring_frames = NULL;

    if (fa->allocate_frame_by_count(fa, event_ring_fa_count, fa_type, &event_ring_frames, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to allocate frames for Event Ring");
        return -1;
    }
    uint64_t event_ring_fa = event_ring_frames->frame_address;
    uint64_t event_ring_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(event_ring_fa);

    if (memory_paging_add_va_for_frame(event_ring_va, event_ring_frames,
                                       MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                       MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH |
                                       MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to map Event Ring frames to virtual address");
        fa->release_frame(fa, event_ring_frames);
        return -1;
    }

    memory_memclean((void*)event_ring_va, event_ring_fa_count * FRAME_SIZE);

    usb_xhci_trb_t* event_ring = (usb_xhci_trb_t*)event_ring_va;
    erst[0] = event_ring_fa;
    erst[1] = metadata->event_ring_size;

    metadata->event_ring = event_ring;
    metadata->event_ring_size = metadata->event_ring_size;
    metadata->event_ring_frame = event_ring_frames;

    // Configure primary interrupter
    usb_xhci_runtime_registers_t* runtime = (usb_xhci_runtime_registers_t*)(bar_va + xhci_cap->rtsoff);
    runtime->interrupters[0].erstsz = 1;
    runtime->interrupters[0].erstba = erst_fa;
    runtime->interrupters[0].erdp = event_ring_fa;
    runtime->interrupters[0].iman = 2; // Enable interrupter
    metadata->runtime = runtime;
    PRINTLOG(USB, LOG_DEBUG, "XHCI Event Ring initialized at 0x%016llx",
             (uint64_t)event_ring);

    metadata->doorbells = (usb_xhci_doorbell_t*)(bar_va + xhci_cap->dboff);

    // Start the controller
    usbcmd.raw = 0;
    usbcmd.bits.run_stop = 1;
    usbcmd.bits.int_enable = 1;
    opregs->usbcmd = usbcmd.raw;

    metadata->event_cycle_bit = 1;
    metadata->cycle_bit = 1;

    // Wait for controller to start
    timeout = 1000000;
    while (((usb_xhci_usbsts_t)opregs->usbsts).bits.hch && timeout--) {
        time_timer_spinsleep(100);
        usbsts = (usb_xhci_usbsts_t)opregs->usbsts;
    }
    if (timeout == 0) {
        PRINTLOG(USB, LOG_ERROR, "XHCI start timeout");
        memory_free(metadata);
        return -1;
    }
    PRINTLOG(USB, LOG_DEBUG, "XHCI controller started");

    void** pslt_args = memory_malloc(sizeof(void*) * 2);

    if(pslt_args == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for port status listener task args");
        memory_free(metadata);

        return -1;
    }

    pslt_args[0] = usb_controller;

    char_t* pslt_task_name = strprintf("usb_xhci_port_status_listener_task-%lli", metadata->controller_id);

    if(!pslt_task_name) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for port status listener task name");
        memory_free(pslt_args);
        memory_free(metadata);

        return -1;
    }

    metadata->port_status_listener_tid = task_create_task(NULL, 128 << 10, 64 << 10,
                                                          usb_xhci_port_status_listener_task, 1, pslt_args,
                                                          pslt_task_name);

    if(metadata->port_status_listener_tid == -1ULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot create port status listener task");
        memory_free(pslt_args);
        memory_free(metadata);

        return -1;
    }

    while(!metadata->port_status_listener_task_initialized) {
        time_timer_msleep(1000);
    }

    void** plt_args = memory_malloc(sizeof(void*) * 2);

    if(plt_args == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for interrupter task args");
        memory_free(metadata);

        return -1;
    }

    plt_args[0] = usb_controller;

    char_t* task_name = strprintf("usb_xhci_interrupter_task-%lli", metadata->controller_id);

    if(!task_name) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for interrupter task name");
        memory_free(plt_args);
        memory_free(metadata);

        return -1;
    }

    metadata->interrupter_tid = task_create_task(NULL, 128 << 10, 64 << 10,
                                                 usb_xhci_interrupter_task, 1, plt_args,
                                                 task_name);

    if(metadata->interrupter_tid == -1ULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot create interrupter task");
        memory_free(plt_args);
        memory_free(metadata);

        return -1;
    }

    uint8_t vector = pci_msix_set_isr((pci_generic_device_t*)usb_controller->pci_dev->pci_header,
                                      usb_controller->msix_cap,
                                      0, // interrupter 0
                                      usb_xhci_isr);

    hashmap_put(usb_xhci_interrupt_controller_mapping,
                (void*)(uintptr_t)vector, (void*)usb_controller);

    PRINTLOG(USB, LOG_DEBUG, "XHCI ISR vector %d registered", vector);

    usb_controller->reset_port = usb_xhci_reset_port;
    usb_controller->reset_all_ports = usb_xhci_reset_all_ports;
    usb_controller->control_transfer = usb_xhci_control_transfer;
    usb_controller->data_transfer = usb_xhci_data_transfer;
    usb_controller->controller_type = USB_CONTROLLER_TYPE_XHCI;
    usb_controller->destroy_controller_device_context = usb_xhci_destroy_device_controller_context;

    usb_controller->initialized = true;

    while(!metadata->interrupter_task_initialized) {
        time_timer_msleep(1000);
    }

    PRINTLOG(USB, LOG_INFO, "XHCI controller initialized successfully with %d ports", metadata->port_count);

    usb_controller->reset_all_ports(usb_controller);

    PRINTLOG(USB, LOG_INFO, "USB XHCI initialization complete");

    return 0;
}
