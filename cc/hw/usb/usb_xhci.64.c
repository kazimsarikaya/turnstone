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

MODULE("turnstone.kernel.hw.usb");

typedef struct usb_controller_metadata_t {
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
    uint32_t                                   cycle_bit;
} usb_controller_metadata_t;

typedef struct usb_device_controller_context_t {
    struct {
        uint64_t ep_trb_fa;
        uint64_t ep_trb_va;
        uint32_t ep_trb_index;
        frame_t* ep_trb_frame;
    } endpoints[31];
} usb_device_controller_context_t;

hashmap_t* usb_xhci_interrutpt_controller_mapping = NULL;

static int8_t usb_xhci_destroy_device_controller_context(usb_controller_t* controller, usb_device_t* device) {
    if(!controller || !device || !device->controller_device_context) {
        return -1;
    }

    usb_device_controller_context_t* context = device->controller_device_context;

    frame_allocator_t* fa = frame_get_allocator();

    for(int32_t i = 0; i < 32; i++) {
        if(context->endpoints[i].ep_trb_frame) {
            if(memory_paging_delete_va_for_frame(context->endpoints[i].ep_trb_va,
                                                 context->endpoints[i].ep_trb_frame) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot delete ep trb va for frame");
            }
            fa->release_frame(fa, context->endpoints[i].ep_trb_frame);
        }
    }

    memory_free(context);

    device->controller_device_context = NULL;

    return 0;
}

static int8_t usb_xhci_pool_event(usb_controller_t*   usb_controller,
                                  uint64_t            target_trb_fa,
                                  usb_xhci_trb_type_t trb_type,
                                  usb_xhci_trb_t**    out_trb) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_xhci_trb_t* event_rings = metadata->event_ring;
    uint64_t event_ring_fa = metadata->erst[0];
    uint64_t erdp = metadata->runtime->interrupters[0].erdp;

    int32_t timeout = 1000000;
    int32_t event_index = ((erdp - event_ring_fa) / sizeof(usb_xhci_trb_t)) % USB_XHCI_EVENT_RING_SIZE;
    PRINTLOG(USB, LOG_TRACE, "pooling event trb for fa 0x%llx type 0x%x at index %d", target_trb_fa, trb_type, event_index);

    usb_xhci_trb_t* event_trb = &event_rings[event_index];

    if(event_trb->parameter != target_trb_fa) {
        PRINTLOG(USB, LOG_TRACE, "event trb parameter 0x%llx does not match target trb fa 0x%llx", event_trb->parameter, target_trb_fa);
        return -1;
    }


    while(timeout) {
        if((event_trb->control & 1) == metadata->cycle_bit) {
            PRINTLOG(USB, LOG_TRACE, "event trb found for command");
            if(((event_trb->control >> 10) & 0x3F) == trb_type) {
                event_trb->parameter = 0;
                PRINTLOG(USB, LOG_TRACE, "command completed");
                if(out_trb) {
                    *out_trb = event_trb;
                }
                break;
            }
        }
        time_timer_spinsleep(100);
    }

    if(timeout <= 0) {
        PRINTLOG(USB, LOG_ERROR, "command timeout");
        return -1;
    }

    erdp += sizeof(usb_xhci_trb_t);
    if(erdp >= (event_ring_fa + metadata->event_ring_size * sizeof(usb_xhci_trb_t))) {
        PRINTLOG(USB, LOG_TRACE, "wrapping erdp");
        metadata->cycle_bit ^= 1;
        erdp = event_ring_fa;
    }
    metadata->runtime->interrupters[0].erdp = erdp;
    PRINTLOG(USB, LOG_TRACE, "new erdp 0x%llx", erdp);

    return 0;
}

void video_text_print(const char_t* str);

static int8_t usb_xhci_isr(interrupt_frame_ext_t* frame) {
    video_text_print("XHCI IRQ\n");

    uint64_t vector = frame->interrupt_number - 0x20; // convert to isr
    const usb_controller_t* usb_controller = hashmap_get(usb_xhci_interrutpt_controller_mapping, (void*)vector);

    if(!usb_controller) {
        apic_eoi();
        return 0;
    }

    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_xhci_trb_t* event_rings = metadata->event_ring;
    uint64_t event_ring_fa = metadata->erst[0];
    uint64_t erdp = metadata->runtime->interrupters[0].erdp;

    int32_t event_index = ((erdp - event_ring_fa) / sizeof(usb_xhci_trb_t)) % USB_XHCI_EVENT_RING_SIZE;

    usb_xhci_trb_t* event_trb = &event_rings[event_index];

    usb_xhci_trb_type_t trb_type = (event_trb->control >> 10) & 0x3F;

    if(trb_type == USB_XHCI_TRB_TYPE_ER_PORT_STATUS_CHANGE) {
        memory_memclean(event_trb, sizeof(usb_xhci_trb_t));
        erdp += sizeof(usb_xhci_trb_t);
        if(erdp >= (event_ring_fa + metadata->event_ring_size * sizeof(usb_xhci_trb_t))) {
            erdp = event_ring_fa;
        }
        metadata->runtime->interrupters[0].erdp = erdp;
    }

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

static int8_t usb_xhci_probe_port(usb_controller_t* usb_controller, uint8_t port) {
    // usb_controller_metadata_t* metadata = usb_controller->metadata;

    int8_t res = usb_xhci_reset_port(usb_controller, port);

    if(res < 0) {
        return -1;
    }

    usb_controller_metadata_t* metadata = usb_controller->metadata;

    volatile usb_xhci_port_register_set_t* portreg = &metadata->op_regs->portsc[port];
    usb_xhci_portsc_t portsc = (usb_xhci_portsc_t)portreg->portsc;

    if(res == 1) {
        PRINTLOG(USB, LOG_TRACE, "port %d enabled. we will initialize driver", port);

        if(usb_device_init(NULL, usb_controller, port, portsc.bits.ps) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot initialize device on port %d", port);

            return -1;
        }

    }

    return 0;
}

static int8_t usb_xhci_probe_all_ports(usb_controller_t* usb_controller) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    boolean_t failed = false;

    for(int16_t i = 0; i < metadata->port_count; i++) {
        if(usb_xhci_probe_port(usb_controller, i) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot probe port %d", i);
            failed = true;
        }
    }

    return failed ? -1 : 0;
}

static int8_t usb_xhci_set_slot_and_address(usb_controller_t* usb_controller, usb_transfer_t* transfer){
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_device_t* device = transfer->device;
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

    if(device->controller_device_context) {
        PRINTLOG(USB, LOG_ERROR, "device already has controller device context");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    usb_device_controller_context_t* context =  memory_malloc(sizeof(usb_device_controller_context_t));
    device->controller_device_context = context;

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

    if(fa->allocate_frame_by_count(fa, 1, fa_type, &ep_ctrl_frame, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate frame for ep0 trb");
        memory_free(device->controller_device_context);
        device->controller_device_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    uint64_t ep0_trb_fa = ep_ctrl_frame->frame_address;
    uint64_t ep0_trb_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ep0_trb_fa);

    memory_paging_add_va_for_frame(ep0_trb_va, ep_ctrl_frame, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memclean((void*)ep0_trb_va, FRAME_SIZE);

    context->endpoints[0].ep_trb_fa = ep0_trb_fa;
    context->endpoints[0].ep_trb_va = ep0_trb_va;
    context->endpoints[0].ep_trb_index = 0;
    context->endpoints[0].ep_trb_frame = ep_ctrl_frame;

    usb_xhci_trb_t* ep0_trbs = (usb_xhci_trb_t*)ep0_trb_va;

    ep0_trbs[USB_XHCI_CMD_RING_SIZE].parameter = ep0_trb_fa;
    ep0_trbs[USB_XHCI_CMD_RING_SIZE].status = 0;
    ep0_trbs[USB_XHCI_CMD_RING_SIZE].control = ((uint64_t)USB_XHCI_TRB_TYPE_TR_LINK << 10) | 1 | (1 << 4);


    uint64_t* dbcbaa = metadata->dcbaa;
    // Set slot
    usb_xhci_trb_t* cur_cmd_ring = &cmd_rings[metadata->current_cmd_index];
    uint64_t cur_cmd_ring_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)cur_cmd_ring);

    cur_cmd_ring->parameter = 0;
    cur_cmd_ring->status = 0;
    cur_cmd_ring->control = ((uint64_t)USB_XHCI_TRB_TYPE_CR_ENABLE_SLOT << 10) | 1;

    doorbells[0].db = 0; // ring doorbell for slot 0 (command ring)

    metadata->current_cmd_index = (metadata->current_cmd_index + 1) % USB_XHCI_CMD_RING_SIZE;

    usb_xhci_trb_t* event_trb = NULL;
    if(usb_xhci_pool_event(usb_controller, cur_cmd_ring_fa, USB_XHCI_TRB_TYPE_ER_COMMAND_COMPLETE, &event_trb) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get event for command");
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        memory_free(device->controller_device_context);
        device->controller_device_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    device->slot_id = event_trb->control >> 24;
    memory_memclean(event_trb, sizeof(usb_xhci_trb_t));
    PRINTLOG(USB, LOG_TRACE, "new device slot id: %d", device->slot_id);

    uint64_t dcb_fa = dbcbaa[device->slot_id];
    uint64_t* dcb = (uint64_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dcb_fa);

    uint32_t* ictx = (uint32_t*)dcb;
    ictx[0] = 0;
    ictx[1] = 0x3;

    uint32_t* slot_ctx = &ictx[8]; // + metadata->context_size / 8;
    slot_ctx[0] = (1 << 27) | (device->speed << 20) | ((device->port + 1) << 16);
    slot_ctx[1] = (device->port + 1) << 16;

    uint32_t* ep0_ctx = &ictx[16]; // + 2 * metadata->context_size / 8;
    ep0_ctx[0] = 0;
    ep0_ctx[1] = 0x00400026; // Max Packet Size = 64, Endpoint Type = Control
    ep0_ctx[2] = ep0_trb_fa | 1;
    ep0_ctx[3] = 0;
    ep0_ctx[4] = 8;

    // Set Address
    cur_cmd_ring = &cmd_rings[metadata->current_cmd_index];
    cur_cmd_ring_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)cur_cmd_ring);
    uint64_t dev_ctx = metadata->dcbaa[device->slot_id];
    if(!dev_ctx) {
        PRINTLOG(USB, LOG_ERROR, "Device context not allocated for slot %d", device->slot_id);
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        device->slot_id = 0;
        memory_free(device->controller_device_context);
        device->controller_device_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }
    cur_cmd_ring->parameter = dcb_fa;
    cur_cmd_ring->status = 0;
    cur_cmd_ring->control = ((uint64_t)USB_XHCI_TRB_TYPE_CR_ADDRESS_DEVICE << 10) | 1 | (device->slot_id << 24);
    doorbells[0].db = 0; // ring doorbell for slot 0 (command ring)

    metadata->current_cmd_index = (metadata->current_cmd_index + 1) % USB_XHCI_CMD_RING_SIZE;

    event_trb = NULL; // reset event_trb
    if(usb_xhci_pool_event(usb_controller, cur_cmd_ring_fa, USB_XHCI_TRB_TYPE_ER_COMMAND_COMPLETE, &event_trb) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get event for command");
        memory_paging_delete_va_for_frame(ep0_trb_va, ep_ctrl_frame);
        fa->release_frame(fa, ep_ctrl_frame);
        device->slot_id = 0;
        memory_free(device->controller_device_context);
        device->controller_device_context = NULL;
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }

    device->address = ictx[3] & 0xFF;
    transfer->request->value = device->address;
    PRINTLOG(USB, LOG_TRACE, "new device address: %d", device->address);
    memory_memclean(event_trb, sizeof(usb_xhci_trb_t));

    transfer->complete = true;
    transfer->success = true;

    return 0;
}

static int8_t usb_xhci_control_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_device_t* device = transfer->device;
    usb_device_request_t* request = transfer->request;

    if(request->request == USB_REQUEST_SET_ADDRESS) {
        return usb_xhci_set_slot_and_address(usb_controller, transfer);
    }

    uint64_t dbc_fa = metadata->dcbaa[device->slot_id];

    usb_device_controller_context_t* context = device->controller_device_context;

    uint64_t transfer_ring_fa = context->endpoints[0].ep_trb_fa;
    uint64_t transfer_ring_va = context->endpoints[0].ep_trb_va;

    usb_xhci_trb_t* transfer_ring = (usb_xhci_trb_t*)transfer_ring_va;
    uint32_t trb_index = context->endpoints[0].ep_trb_index;

    PRINTLOG(USB, LOG_TRACE, "XHCI control transfer for slot %d, dev_ctx=0x%llx, transfer_ring=0x%llx",
             device->slot_id, dbc_fa, transfer_ring_fa);


    void* data = transfer->data;
    uint16_t length = transfer->length;


    transfer_ring[trb_index].parameter = (((uint64_t)request->length) << 48) |
                                         (((uint64_t)request->index) << 32) |
                                         (((uint64_t)request->value) << 16) |
                                         (((uint64_t)request->request) << 8) |
                                         ((uint64_t)request->type);
    transfer_ring[trb_index].status = 8;
    transfer_ring[trb_index].control = (USB_XHCI_TRB_TYPE_TR_SETUP << 10) | (1 << 6) | 1; // TRB Type 2 (Setup Stage), Immediate Data, Cycle bit
    trb_index = (trb_index + 1) % USB_XHCI_CMD_RING_SIZE;
    context->endpoints[0].ep_trb_index = trb_index;

    uint32_t dir_bit = (request->type & USB_REQUEST_DIRECTION_DEVICE_TO_HOST) ? (1 << 16) : 0;


    if(length) {
        uint32_t remaining_length = length;
        uint32_t max_packet_size = device->max_packet_size;

        uint64_t data_fa = 0;
        if(memory_paging_get_physical_address((uint64_t)data, &data_fa) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot get data buffer physical address 0x%p", data);

            // Clean up the transfer ring TRB we just added
            trb_index = (trb_index + USB_XHCI_CMD_RING_SIZE - 1) % USB_XHCI_CMD_RING_SIZE;
            context->endpoints[0].ep_trb_index = trb_index;

            memory_memclean(&transfer_ring[trb_index], sizeof(usb_xhci_trb_t));

            transfer->complete = true;
            transfer->success = false;
            return -1;
        }

        // Data Stage TRB (if length > 0)
        while (remaining_length > 0) {

            PRINTLOG(USB, LOG_TRACE, "data fa: 0x%llx length 0x%x", data_fa, length);

            uint32_t curr_length = MIN(remaining_length, max_packet_size);

            transfer_ring[trb_index].parameter = data_fa;
            transfer_ring[trb_index].status = curr_length;
            transfer_ring[trb_index].control = dir_bit | (USB_XHCI_TRB_TYPE_TR_DATA << 10)  | 1;
            trb_index = (trb_index + 1) % USB_XHCI_CMD_RING_SIZE;
            context->endpoints[0].ep_trb_index = trb_index;

            data_fa += curr_length;
            remaining_length -= curr_length;
        }

    }


    // Status Stage TRB
    transfer_ring[trb_index].parameter = 0;
    transfer_ring[trb_index].status = 0;
    transfer_ring[trb_index].control = dir_bit | (USB_XHCI_TRB_TYPE_TR_STATUS << 10) | (1 << 5) | 1;

    uint64_t cur_trb_fa = transfer_ring_fa + trb_index * sizeof(usb_xhci_trb_t);

    trb_index = (trb_index + 1) % USB_XHCI_CMD_RING_SIZE;
    context->endpoints[0].ep_trb_index = trb_index;

    transfer_ring[trb_index].parameter = 0;
    transfer_ring[trb_index].status = 0;
    transfer_ring[trb_index].control = 0;

    // Ring doorbell for transfer (Endpoint 0, DCI = 1)
    usb_xhci_doorbell_t* doorbell = metadata->doorbells;
    doorbell[device->slot_id].db = 1; // DCI = 1 for Endpoint 0

    PRINTLOG(USB, LOG_TRACE, "XHCI control transfer initiated for slot %d", device->slot_id);

    usb_xhci_trb_t* event_trb = NULL; // reset event_trb
    if(usb_xhci_pool_event(usb_controller, cur_trb_fa, USB_XHCI_TRB_TYPE_ER_TRANSFER, &event_trb) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get event for command");
        transfer->complete = true;
        transfer->success = false;
        return -1;
    }
    memory_memclean(event_trb, sizeof(usb_xhci_trb_t));

    transfer->complete = true;
    transfer->success = true;

    return 0;
}

static int8_t usb_xhci_bulk_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    UNUSED(usb_controller);
    UNUSED(transfer);
    PRINTLOG(USB, LOG_ERROR, "XHCI bulk transfer not implemented");
    return -1;
}


int8_t usb_xhci_init(usb_controller_t* usb_controller) {
    PRINTLOG(USB, LOG_INFO, "initializing XHCI controller");

    if(!usb_controller || !usb_controller->pci_dev) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");

        return -1;
    }

    if(usb_xhci_interrutpt_controller_mapping == NULL) {
        usb_xhci_interrutpt_controller_mapping = hashmap_integer(16);
        if(usb_xhci_interrutpt_controller_mapping == NULL) {
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

    usb_controller->metadata = metadata;

    PRINTLOG(USB, LOG_DEBUG, "XHCI controller found: %02x:%02x:%02x:%02x",
             pci_dev->group_number, pci_dev->bus_number, pci_dev->device_number, pci_dev->function_number);

    uint64_t bar_fa = pci_get_bar_address((pci_generic_device_t*)pci_dev->pci_header, 0);
    PRINTLOG(USB, LOG_DEBUG, "XHCI BAR address: 0x%016llx", bar_fa);

    uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);
    PRINTLOG(USB, LOG_DEBUG, "XHCI BAR virtual address: 0x%016llx", bar_va);

    usb_xhci_capabilities_t* xhci_cap = (usb_xhci_capabilities_t*)bar_va;

    usb_xhci_caplen_rev_t caplen_rev  = (usb_xhci_caplen_rev_t)xhci_cap->caplength_and_revision;

    PRINTLOG(USB, LOG_DEBUG, "XHCI capability length: %d", caplen_rev.bits.capability_length);
    PRINTLOG(USB, LOG_DEBUG, "XHCI revision: 0x%04x", caplen_rev.bits.usb_revision);

    usb_xhci_hcs_params_1_t hcs_params1 = (usb_xhci_hcs_params_1_t)xhci_cap->hcs_params_1;

    PRINTLOG(USB, LOG_DEBUG, "XHCI number of device slots: %d", hcs_params1.bits.max_slots);
    PRINTLOG(USB, LOG_DEBUG, "XHCI number of interrupter: %d", hcs_params1.bits.max_interrupts);
    PRINTLOG(USB, LOG_DEBUG, "XHCI number of ports: %d", hcs_params1.bits.max_ports);

    usb_xhci_hcs_params_2_t hcs_params2 = (usb_xhci_hcs_params_2_t)xhci_cap->hcs_params_2;

    PRINTLOG(USB, LOG_DEBUG, "XHCI max erst size: %d", hcs_params2.bits.erst_max);

    usb_xhci_hcc_params_1_t hcc_params1 = (usb_xhci_hcc_params_1_t)xhci_cap->hcc_params_1;

    PRINTLOG(USB, LOG_DEBUG, "XHCI ac64: %d", hcc_params1.bits.ac64);
    metadata->addr64 = hcc_params1.bits.ac64 ? true : false;
    PRINTLOG(USB, LOG_DEBUG, "XHCI context size: %d", hcc_params1.bits.csz);
    metadata->context_size = hcc_params1.bits.csz ? 64 : 32;
    uint64_t extended_caps_offset = hcc_params1.bits.xecp << 2;
    PRINTLOG(USB, LOG_DEBUG, "XHCI extended capabilities pointer: 0x%016llx", extended_caps_offset);

    PRINTLOG(USB, LOG_DEBUG, "XHCI doorbell offset: 0x%08x", xhci_cap->dboff);
    PRINTLOG(USB, LOG_DEBUG, "XHCI runtime register space offset: 0x%08x", xhci_cap->rtsoff);

    uint64_t opregs_va = bar_va + caplen_rev.bits.capability_length;

    usb_xhci_operational_registers_t* opregs = (usb_xhci_operational_registers_t*)opregs_va;

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

    if (memory_paging_add_va_for_frame(dcbaa_va, dcbaa_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
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
    uint64_t cmd_ring_size = USB_XHCI_CMD_RING_SIZE * sizeof(usb_xhci_trb_t);
    uint64_t cmd_ring_fa_count = (cmd_ring_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t* cmd_ring_frames = NULL;

    if (fa->allocate_frame_by_count(fa, cmd_ring_fa_count, fa_type, &cmd_ring_frames, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to allocate frames for Command Ring");
        return -1;
    }
    uint64_t cmd_ring_fa = cmd_ring_frames->frame_address;
    uint64_t cmd_ring_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(cmd_ring_fa);

    if (memory_paging_add_va_for_frame(cmd_ring_va, cmd_ring_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to map Command Ring frames to virtual address");
        fa->release_frame(fa, cmd_ring_frames);
        return -1;
    }

    memory_memclean((void*)cmd_ring_va, cmd_ring_fa_count * FRAME_SIZE);

    usb_xhci_trb_t* cmd_ring = (usb_xhci_trb_t*)cmd_ring_va;

    usb_xhci_crcr_t crcr = { .bits.ptr = cmd_ring_fa >> 6, .bits.rcs = 1 };
    opregs->crcr = crcr.raw;
    metadata->cmd_ring = cmd_ring;
    metadata->cmd_ring_size = USB_XHCI_CMD_RING_SIZE;
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

    if (memory_paging_add_va_for_frame(erst_va, erst_frame, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to map ERST frame to virtual address");
        fa->release_frame(fa, erst_frame);
        return -1;
    }

    memory_memclean((void*)erst_va, FRAME_SIZE);

    uint64_t* erst = (uint64_t*)erst_va;
    metadata->erst = erst;
    metadata->erst_size = FRAME_SIZE;
    metadata->erst_frame = erst_frame;

    uint64_t event_ring_size = USB_XHCI_EVENT_RING_SIZE * sizeof(usb_xhci_trb_t);
    uint64_t event_ring_fa_count = (event_ring_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t* event_ring_frames = NULL;

    if (fa->allocate_frame_by_count(fa, event_ring_fa_count, fa_type, &event_ring_frames, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to allocate frames for Event Ring");
        return -1;
    }
    uint64_t event_ring_fa = event_ring_frames->frame_address;
    uint64_t event_ring_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(event_ring_fa);

    if (memory_paging_add_va_for_frame(event_ring_va, event_ring_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(USB, LOG_ERROR, "Failed to map Event Ring frames to virtual address");
        fa->release_frame(fa, event_ring_frames);
        return -1;
    }

    memory_memclean((void*)event_ring_va, event_ring_fa_count * FRAME_SIZE);

    usb_xhci_trb_t* event_ring = (usb_xhci_trb_t*)event_ring_va;
    erst[0] = event_ring_fa;
    erst[1] = USB_XHCI_EVENT_RING_SIZE;

    metadata->event_ring = event_ring;
    metadata->event_ring_size = USB_XHCI_EVENT_RING_SIZE;
    metadata->event_ring_frame = event_ring_frames;

    // Configure primary interrupter
    usb_xhci_runtime_registers_t* runtime = (usb_xhci_runtime_registers_t*)(bar_va + xhci_cap->rtsoff);
    runtime->interrupters[0].erstsz = 1;
    runtime->interrupters[0].erstba = erst_fa;
    runtime->interrupters[0].erdp = event_ring_fa;
    runtime->interrupters[0].iman = 2; // Enable interrupter
    metadata->runtime = runtime;
    uint8_t vector = pci_msix_set_isr((pci_generic_device_t*)usb_controller->pci_dev->pci_header,
                                      usb_controller->msix_cap,
                                      0, // interrupter 0
                                      usb_xhci_isr);

    hashmap_put(usb_xhci_interrutpt_controller_mapping,
                (void*)(uintptr_t)vector, (void*)usb_controller);
    PRINTLOG(USB, LOG_DEBUG, "XHCI Event Ring initialized at 0x%016llx with vector 0x%02x",
             (uint64_t)event_ring, vector);

    metadata->doorbells = (usb_xhci_doorbell_t*)(bar_va + xhci_cap->dboff);

    // Start the controller
    usbcmd.raw = 0;
    usbcmd.bits.run_stop = 1;
    usbcmd.bits.int_enable = 1;
    opregs->usbcmd = usbcmd.raw;

    // Wait for controller to start
    timeout = 1000000;
    while (((usb_xhci_usbsts_t)opregs->usbsts).bits.hch && timeout--) {
        time_timer_spinsleep(100);
        usbsts = (usb_xhci_usbsts_t)opregs->usbsts;
    }
    if (timeout == 0) {
        PRINTLOG(USB, LOG_ERROR, "XHCI start timeout");
        return -1;
    }
    PRINTLOG(USB, LOG_DEBUG, "XHCI controller started");

    uint32_t port_count = hcs_params1.bits.max_ports;

    metadata->cap = xhci_cap;
    metadata->op_regs = opregs;
    metadata->port_count = port_count;
    metadata->cycle_bit = 1;
    usb_controller->reset_port = usb_xhci_reset_port;
    usb_controller->probe_port = usb_xhci_probe_port;
    usb_controller->probe_all_ports = usb_xhci_probe_all_ports;
    usb_controller->control_transfer = usb_xhci_control_transfer;
    usb_controller->bulk_transfer = usb_xhci_bulk_transfer;
    usb_controller->controller_type = USB_CONTROLLER_TYPE_XHCI;
    usb_controller->destroy_controller_device_context = usb_xhci_destroy_device_controller_context;

    usb_controller->initialized = true;

    PRINTLOG(USB, LOG_INFO, "XHCI controller initialized successfully with %d ports", port_count);

    usb_controller->probe_all_ports(usb_controller);

    return 0;
}
