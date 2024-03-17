/**
 * @file usb_ehci.64.c
 * @brief USB EHCI driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <driver/usb_ehci.h>
#include <pci.h>
#include <logging.h>
#include <memory/paging.h>
#include <apic.h>
#include <time/timer.h>
#include <hashmap.h>
#include <cpu/task.h>
#include <cpu/sync.h>
#include <future.h>
#include <cpu.h>

MODULE("turnstone.kernel.hw.usb");

typedef struct usb_controller_metadata_t {
    usb_ehci_controller_capabilties_t*          cap;
    usb_ehci_op_regs_t*                         op_regs;
    volatile usb_legacy_support_capabilities_t* legacy_cap;
    boolean_t                                   addr64;
    uint64_t                                    framelist_fa;
    uint64_t                                    framelist_va;
    uint64_t                                    qh_count;
    uint64_t*                                   qh_used_mask;
    uint64_t                                    qh_fa;
    uint64_t                                    qh_va;
    uint64_t                                    qtd_count;
    uint64_t*                                   qtd_used_mask;
    uint64_t                                    qtd_fa;
    uint64_t                                    qtd_va;
    uint64_t                                    itd_pool_count;
    uint64_t*                                   itd_pool_used_mask;
    uint64_t                                    itd_pool_fa;
    uint64_t                                    itd_pool_va;
    usb_ehci_qh_t*                              qh_asynclist_head;
    usb_ehci_qh_t*                              qh_periodiclist_head;
    usb_ehci_framelist_item_t*                  framelist;
    uint8_t                                     port_count;
    uint64_t                                    int_frame_entries_count;
    uint64_t                                    int_frame_entries_fa;
    uint64_t                                    int_frame_entries_va;
    uint64_t                                    v_frame_entries_count;
    uint64_t                                    itd_fa;
    uint64_t                                    itd_va;
    uint64_t                                    sitd_fa;
    uint64_t                                    sitd_va;
    uint64_t                                    framelist_entries_count;
    lock_t*                                     async_list_lock;
    uint64_t                                    current_async_transfer_count;
    uint64_t                                    async_list_tid;
    lock_t*                                     periodic_list_lock;
    uint64_t                                    current_periodic_transfer_count;
    uint64_t                                    periodic_list_tid;
} usb_controller_metadata_t;

hashmap_t* usb_ehci_controllers = NULL;

usb_ehci_qh_t* usb_ehci_find_qh(usb_controller_t* usb_controller);
int8_t         usb_ehci_init_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* qh, usb_ehci_qtd_t* head, usb_ehci_qtd_t* tail, usb_transfer_t* transfer, boolean_t ioc);
int8_t         usb_ehci_link_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* list, usb_ehci_qh_t* qh);
int8_t         usb_ehci_unlink_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* list, usb_ehci_qh_t* qh);
int8_t         usb_ehci_free_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* qh);


usb_ehci_qtd_t* usb_ehci_find_qtd(usb_controller_t* usb_controller);
int8_t          usb_ehci_free_qtd(usb_controller_t* usb_controller, usb_ehci_qtd_t* qtd);
int8_t          usb_ehci_free_qtd_chain(usb_controller_t* usb_controller, usb_ehci_qtd_t* qtd);
int8_t          usb_ehci_init_qtd(usb_controller_t* usb_controller, usb_ehci_qtd_t* qtd, usb_ehci_qtd_t* prev,
                                  uint32_t data_toggle, uint32_t packet_type, void* data, uint32_t data_size);

usb_ehci_itd_t* usb_ehci_find_itd(usb_controller_t* usb_controller);
int8_t          usb_ehci_free_itd(usb_controller_t* usb_controller, usb_ehci_itd_t* itd);


int8_t usb_ehci_isr(interrupt_frame_ext_t* frame);

int8_t usb_ehci_probe_all_ports(usb_controller_t* usb_controller);
int8_t usb_ehci_probe_port(usb_controller_t* usb_controller, uint8_t port);
int8_t usb_ehci_reset_port(usb_controller_t* usb_controller, uint8_t port);

int8_t usb_ehci_control_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer);
int8_t usb_ehci_isochronous_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer);
int8_t usb_ehci_bulk_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer);

int8_t usb_ehci_periodiclist_lookup_task(int32_t argc, void** argv);
int8_t usb_ehci_asynclist_lookup_task(int32_t argc, void** argv);

void usb_ehci_wait_for_transfer(usb_controller_t* usb_controller, usb_ehci_qh_t* qh, usb_transfer_t* transfer);
void usb_ehci_poll(usb_controller_t* usb_controller, usb_ehci_qh_t* qh, usb_transfer_t* transfer);


void video_text_print(char_t* string);

int8_t usb_ehci_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    // PRINTLOG(USB, LOG_DEBUG, "EHCI ISR %x", irq);

    boolean_t handled = false;

    iterator_t* it = hashmap_iterator_create(usb_ehci_controllers);

    while(it->end_of_iterator(it) != 0) {
        const usb_controller_t* usb_controller = it->get_item(it);
        usb_controller_metadata_t* metadata = usb_controller->metadata;

        uint32_t usbsts = metadata->op_regs->usb_sts;
        uint32_t usbintr = metadata->op_regs->usb_intr;

        if(usbsts & usbintr) {
            handled = true;
        } else {
            it = it->next(it);

            continue;
        }

        /*
           char_t buffer[256] = {0};
           memory_memclean(buffer, 256);

           utoh_with_buffer(buffer, usbsts);

           video_text_print((char_t*)"EHCI ISR usb_sts: 0x");
           video_text_print(buffer);
           video_text_print((char_t*)"\n");
         */

        usb_ehci_sts_reg_t sts_reg = { .raw = usbsts };
        usb_ehci_int_enable_reg_t intr_reg = { .raw = usbintr };

        if(sts_reg.bits.port_change && intr_reg.bits.port_change_enable) {
            sts_reg.bits.port_change = 1;
        }

        if(sts_reg.bits.frame_list_rollover && intr_reg.bits.frame_list_rollover_enable) {
            video_text_print((char_t*)"EHCI frame list rollover\n");
        }

        if(sts_reg.bits.usb_interrupt && intr_reg.bits.usb_interrupt_enable) {
            if(metadata->current_periodic_transfer_count) {
                task_set_interrupt_received(metadata->periodic_list_tid);
            }

            if(metadata->current_async_transfer_count) {
                task_set_interrupt_received(metadata->async_list_tid);
            }
        }

        metadata->op_regs->usb_sts = sts_reg.raw;

        it = it->next(it);
    }


    it->destroy(it);

    if(handled) {
        apic_eoi();

        return 0;
    }

    return -1;
}

usb_ehci_itd_t* usb_ehci_find_itd(usb_controller_t* usb_controller) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    usb_ehci_itd_t* itds = (usb_ehci_itd_t*)metadata->itd_pool_va;


    uint32_t idx = 0;
    boolean_t found = false;

    for(uint32_t i = 0; i < metadata->itd_pool_count; i++) {
        uint32_t part_idx = i / 64;
        uint32_t bit_idx = i % 64;

        if((metadata->itd_pool_used_mask[part_idx] & (1 << bit_idx)) == 0) {
            idx = i;
            found = true;
            metadata->itd_pool_used_mask[part_idx] |= (1 << bit_idx);

            itds[idx].id = idx;
            itds[idx].this_raw = (uint32_t)(uint64_t)&itds[idx];

            break;
        }

    }

    if(!found) {
        return NULL;
    }

    return &itds[idx];
}

int8_t usb_ehci_free_itd(usb_controller_t* usb_controller, usb_ehci_itd_t* itd) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    uint64_t id = itd->id;

    if(id >= metadata->itd_pool_count) {
        return -1;
    }

    uint32_t part_idx = id / 64;
    uint32_t bit_idx = id % 64;

    metadata->itd_pool_used_mask[part_idx] &= ~(1 << bit_idx);

    memory_memclean(itd, sizeof(usb_ehci_itd_t));

    return 0;
}

usb_ehci_qh_t* usb_ehci_find_qh(usb_controller_t * usb_controller) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_ehci_qh_t* qhs = (usb_ehci_qh_t*)metadata->qh_va;

    for(uint64_t i = 0; i < metadata->qh_count; i++) {
        uint32_t part_idx = i / 64;
        uint32_t bit_idx = i % 64;

        if((metadata->qh_used_mask[part_idx] & (1 << bit_idx)) == 0) {
            metadata->qh_used_mask[part_idx] |= (1 << bit_idx);

            qhs[i].id = i;
            qhs[i].this_raw = (uint32_t)(uint64_t)&qhs[i];

            return &qhs[i];
        }
    }

    return NULL;
}

int8_t usb_ehci_free_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* qh) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    if(usb_ehci_free_qtd_chain(usb_controller, qh->qtd_tail) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot free qtd chain for qh %d", qh->id);
    }

    if(qh->id >= metadata->qh_count) {
        return -1;
    }

    uint32_t part_idx = qh->id / 64;
    uint32_t bit_idx = qh->id % 64;

    metadata->qh_used_mask[part_idx] &= ~(1 << bit_idx);

    memory_memclean(qh, sizeof(usb_ehci_qh_t));

    PRINTLOG(USB, LOG_TRACE, "freed qh %d", qh->id);

    return 0;
}

usb_ehci_qtd_t* usb_ehci_find_qtd(usb_controller_t* usb_controller) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;
    usb_ehci_qtd_t* qtds = (usb_ehci_qtd_t*)metadata->qtd_va;

    for(uint64_t i = 0; i < metadata->qtd_count; i++) {
        uint32_t part_idx = i / 64;
        uint32_t bit_idx = i % 64;

        if((metadata->qtd_used_mask[part_idx] & (1 << bit_idx)) == 0) {
            metadata->qtd_used_mask[part_idx] |= (1 << bit_idx);

            qtds[i].id = i;

            return &qtds[i];
        }
    }

    return NULL;
}

int8_t usb_ehci_free_qtd(usb_controller_t* usb_controller, usb_ehci_qtd_t* qtd) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    if(qtd->id >= metadata->qtd_count) {
        return -1;
    }

    uint32_t part_idx = qtd->id / 64;
    uint32_t bit_idx = qtd->id % 64;

    metadata->qtd_used_mask[part_idx] &= ~(1 << bit_idx);

    memory_memclean(qtd, sizeof(usb_ehci_qtd_t));

    PRINTLOG(USB, LOG_TRACE, "freed qtd %d", qtd->id);

    return 0;
}
int8_t usb_ehci_init_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* qh, usb_ehci_qtd_t* head, usb_ehci_qtd_t* tail, usb_transfer_t* transfer, boolean_t ioc) {
    UNUSED(usb_controller);

    uint32_t endpoint_number = 0;
    uint32_t max_packet_length = 0;

    if(transfer->endpoint && transfer->length) {
        endpoint_number = transfer->endpoint->desc->endpoint_address;
        max_packet_length = transfer->endpoint->desc->max_packet_size;
    } else {
        endpoint_number = 0;
        max_packet_length = transfer->device->max_packet_size;
    }


    qh->endpoint_characteristics.bits.endpoint_speed = transfer->device->speed;
    qh->endpoint_characteristics.bits.endpoint_number = endpoint_number;
    qh->endpoint_characteristics.bits.device_address = transfer->device->address;
    qh->endpoint_characteristics.bits.max_packet_length = max_packet_length;
    qh->endpoint_characteristics.bits.data_toggle_control = 1;


    qh->endpoint_capabilities.bits.mult = 1;

    if(transfer->device->parent && transfer->device->speed != USB_ENDPOINT_SPEED_HIGH) {
        if(ioc) {
            qh->endpoint_capabilities.bits.split_completion_mask = 0x1c;
        } else {
            qh->endpoint_characteristics.bits.control_endpoint_flag = 1;
        }

        qh->endpoint_capabilities.bits.hub_address = transfer->device->parent->address;
        qh->endpoint_capabilities.bits.port_number = transfer->device->parent->port;
    }

    if(ioc) {
        qh->endpoint_capabilities.bits.interrupt_schedule_mask = 1;
    } else {
        qh->endpoint_characteristics.bits.nak_count_reload = 5;
    }

    uint64_t qtd_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)head);

    qh->next_qtd_pointer.raw = (uint32_t)qtd_fa;
    qh->token.raw = 0;
    qh->qtd_head = head;
    qh->qtd_tail = tail;

    return 0;
}

int8_t usb_ehci_link_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* list, usb_ehci_qh_t* qh) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    usb_ehci_qh_t* prev = NULL;
    uint64_t mem_hi = 0;

    if(metadata->addr64) {
        mem_hi = metadata->op_regs->ctrl_ds_segment;
        mem_hi <<= 32;
    }

    boolean_t has_prev = false;

    uint64_t prev_addr = 0;

    uint32_t list_prev_raw = list->prev_raw;

    if(list->prev_raw) {
        prev_addr = mem_hi | (list->prev_raw & 0xFFFFFFE0);
        has_prev = true;
    }

    qh->horizontal_link_pointer.raw = (uint32_t)(uint64_t)list;
    qh->horizontal_link_pointer.bits.terminate = 0;
    qh->horizontal_link_pointer.bits.qtd_type = USB_EHCI_QTD_TYPE_QH;

    qh->prev_raw = list_prev_raw;

    list->prev_raw = qh->this_raw;

    if(has_prev) {
        prev = (usb_ehci_qh_t*)prev_addr;
        prev = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(prev);
        prev->horizontal_link_pointer.raw = (uint32_t)(uint64_t)qh | (USB_EHCI_QTD_TYPE_QH << 1);
    }

    PRINTLOG(USB, LOG_TRACE, "linked pre raw 0x%x qh 0x%x next 0x%x", qh->prev_raw, qh->this_raw, qh->horizontal_link_pointer.raw);
    PRINTLOG(USB, LOG_TRACE, "aqh 0x%x next: 0x%x prev 0x%x", list->this_raw, list->horizontal_link_pointer.raw, list->prev_raw);

    return 0;
}

int8_t usb_ehci_unlink_qh(usb_controller_t* usb_controller, usb_ehci_qh_t* list, usb_ehci_qh_t* qh) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    usb_ehci_qh_t* prev = NULL;
    uint64_t mem_hi = 0;

    if(metadata->addr64) {
        mem_hi = metadata->op_regs->ctrl_ds_segment;
        mem_hi <<= 32;
    }

    boolean_t has_prev = false;

    uint64_t prev_addr = 0;

    if(qh->prev_raw) {
        prev_addr = mem_hi | (qh->prev_raw & 0xFFFFFFE0);
        has_prev = true;
    }

    PRINTLOG(USB, LOG_TRACE, "removing qh 0x%p 0x%x prev 0x%x next 0x%x", qh, qh->this_raw, qh->prev_raw, qh->horizontal_link_pointer.raw);

    if(has_prev) {
        prev = (usb_ehci_qh_t*)prev_addr;
        prev = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(prev);
        prev->horizontal_link_pointer.raw = qh->horizontal_link_pointer.raw;
        uint64_t next_addr = mem_hi | (qh->horizontal_link_pointer.raw & 0xFFFFFFE0);
        next_addr = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(next_addr);
        usb_ehci_qh_t* next = (usb_ehci_qh_t*)next_addr;
        next->prev_raw = qh->prev_raw;

        PRINTLOG(USB, LOG_TRACE, "prev: 0x%p 0x%x next 0x%x list 0x%x", prev, prev->this_raw, prev->horizontal_link_pointer.raw, list->horizontal_link_pointer.raw);
    }

    uint64_t aqh = metadata->op_regs->async_list_addr;
    aqh |= mem_hi;

    usb_ehci_qh_t* a_qh = (usb_ehci_qh_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((void*)aqh);

    PRINTLOG(USB, LOG_TRACE, "aqh 0x%p 0x%x next: 0x%x prev 0x%x", a_qh, a_qh->this_raw, a_qh->horizontal_link_pointer.raw, a_qh->prev_raw);

    qh->horizontal_link_pointer.raw = a_qh->horizontal_link_pointer.raw;

    time_timer_spinsleep(500);

    return 0;
}

int8_t usb_ehci_free_qtd_chain(usb_controller_t* usb_controller, usb_ehci_qtd_t* qtd) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    PRINTLOG(USB, LOG_TRACE, "freeing qtd chain %p", qtd);

    usb_ehci_qtd_t* prev = NULL;
    uint64_t mem_hi = 0;

    if(metadata->addr64) {
        mem_hi = metadata->op_regs->ctrl_ds_segment;
        mem_hi <<= 32;
    }

    boolean_t has_prev = false;

    uint64_t prev_addr = 0;

    PRINTLOG(USB, LOG_TRACE, "is prev? 0x%x", qtd->prev_raw);

    if(qtd->prev_raw) {
        prev_addr = mem_hi | qtd->prev_raw;
        has_prev = true;
    }

    uint8_t id = qtd->id;
    memory_memclean(qtd, sizeof(usb_ehci_qtd_t));
    qtd->id = id;

    usb_ehci_free_qtd(usb_controller, qtd);

    while(has_prev) {
        prev = (usb_ehci_qtd_t*)prev_addr;
        prev = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(prev);
        id = prev->id;

        if(prev->prev_raw) {
            prev_addr = mem_hi | prev->prev_raw;
        } else {
            has_prev = false;
        }

        memory_memclean(prev, sizeof(usb_ehci_qtd_t));
        prev->id = id;

        usb_ehci_free_qtd(usb_controller, prev);

    }

    return 0;
}

int8_t usb_ehci_init_qtd(usb_controller_t* usb_controller, usb_ehci_qtd_t* qtd, usb_ehci_qtd_t* prev,
                         uint32_t data_toggle, uint32_t packet_type, void* data, uint32_t data_size) {

    usb_controller_metadata_t* metadata = usb_controller->metadata;

    if(prev) {
        uint64_t this_qtd_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)qtd);
        prev->next_qtd_pointer.raw = (uint32_t)this_qtd_fa;
        qtd->prev_raw = (uint32_t)(uint64_t)prev;
        PRINTLOG(USB, LOG_TRACE, "EHCI qtd prev %p next qtd %x", prev, prev->next_qtd_pointer.raw);
    }

    qtd->next_qtd_pointer.bits.next_qtd_terminate = 1;
    qtd->alternate_next_qtd_pointer.bits.next_qtd_terminate = 1;

    qtd->token.bits.active = 1;
    qtd->token.bits.data_toggle = data_toggle;
    qtd->token.bits.error_counter = 3;
    qtd->token.bits.bytes_to_transfer = data_size;
    qtd->token.bits.pid_code = packet_type;

    uint64_t data_phy_addr = 0;

    if(data_size && memory_paging_get_physical_address((uint64_t)data, &data_phy_addr) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get read buffer physical address 0x%p", data);

        return NULL;
    }

    uint32_t buffer_lo = (uint32_t)(uint64_t)data_phy_addr;
    uint32_t buffer_hi = (uint32_t)((uint64_t)data_phy_addr >> 32);

    PRINTLOG(USB, LOG_TRACE, "EHCI qtd buffer 0x%x:%x len 0x%x", buffer_hi, buffer_lo, data_size);
    PRINTLOG(USB, LOG_TRACE, "EHCI qtd packet_type 0x%x", packet_type);
    PRINTLOG(USB, LOG_TRACE, "EHCI qtd 0x%p", qtd);

    uint32_t remaining = data_size;

    qtd->buffer_page_pointer[0] = buffer_lo;

    if(metadata->addr64) {
        qtd->ext_buffer_page_pointer[0] = buffer_hi;
    }

    uint32_t used = FRAME_SIZE - (buffer_lo & 0xFFF);

    if(used > remaining) {
        used = remaining;
    }

    remaining -= used;
    buffer_lo += used;

    if(metadata->addr64) {
        qtd->ext_buffer_page_pointer[0] = buffer_hi;
    }

    if(remaining > 4 * FRAME_SIZE) {
        PRINTLOG(USB, LOG_ERROR, "EHCI qtd buffer too large");
        usb_ehci_free_qtd_chain(usb_controller, qtd);

        return -1;
    }

    int32_t idx = 1;

    while(remaining > 0) {
        qtd->buffer_page_pointer[idx] = buffer_lo;

        if(metadata->addr64) {
            qtd->ext_buffer_page_pointer[idx] = buffer_hi;
        }

        used = MIN(remaining, FRAME_SIZE);

        remaining -= used;
        buffer_lo += used;
        idx++;
    }

    return 0;
}

int8_t usb_ehci_isochronous_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    logging_set_level(USB, LOG_TRACE);
    PRINTLOG(USB, LOG_TRACE, "EHCI isochronous transfer");

    usb_controller_metadata_t* metadata = (usb_controller_metadata_t*)usb_controller->metadata;

    uint32_t data_size = transfer->length;
    uint8_t* data = transfer->data;
    uint64_t data_phy_addr = 0;

    PRINTLOG(USB, LOG_TRACE, "EHCI isochronous transfer 0x%p data 0x%p len 0x%x", transfer, data, data_size);

    if(data_size && memory_paging_get_physical_address((uint64_t)data, &data_phy_addr) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get read buffer physical address 0x%p", data);

        return NULL;
    }

    uint32_t buffer_lo = (uint32_t)data_phy_addr;
    uint32_t buffer_hi = (uint32_t)(data_phy_addr >> 32);

    usb_ehci_itd_t* itd = usb_ehci_find_itd(usb_controller);

    if(itd == NULL) {
        PRINTLOG(USB, LOG_ERROR, "no free itd found");

        return -1;
    }

    PRINTLOG(USB, LOG_TRACE, "EHCI itd buffer 0x%x:%x len 0x%x", buffer_hi, buffer_lo, data_size);
    PRINTLOG(USB, LOG_TRACE, "EHCI itd 0x%p", itd);
    PRINTLOG(USB, LOG_TRACE, "EHCI itd id 0x%x", itd->id);

    itd->transfer = transfer;

    itd->next_link_pointer.bits.type = USB_EHCI_QTD_TYPE_ITD;
    itd->next_link_pointer.bits.terminate_bit = 1;
    itd->this_raw = (uint32_t)(uint64_t)itd | (USB_EHCI_QTD_TYPE_ITD << 1);

    itd->buffer_page_0.bits.device_address = transfer->device->address;
    itd->buffer_page_0.bits.endpoint = transfer->endpoint->desc->endpoint_address & 0x7F;
    itd->buffer_page_1.bits.max_packet_length = transfer->endpoint->desc->max_packet_size;
    itd->buffer_page_1.bits.direction = transfer->endpoint->in;
    itd->buffer_page_2.bits.multi_count = 1;

    PRINTLOG(USB, LOG_TRACE, "EHCI itd %x metadata builded, buffer filling...", itd->id);

    uint32_t offset = buffer_lo & 0xFFF;

    if(data_size > 0) {
        itd->transactions[0].bits.active = 1;
        itd->transactions[0].bits.offset = offset;
        itd->transactions[0].bits.length = MIN(data_size, FRAME_SIZE - offset);

        data_size -= itd->transactions[0].bits.length;
        buffer_lo += itd->transactions[0].bits.length;

        itd->buffer_page_0.bits.buffer_address = buffer_lo >> 12;

        if(metadata->addr64) {
            itd->ext_buffer_pages[0] = buffer_hi;
        }

        itd->last_transaction = 0;
    }



    if(data_size > 0) {
        itd->transactions[1].bits.active = 1;
        itd->transactions[1].bits.offset = 0;
        itd->transactions[1].bits.length = MIN(data_size, FRAME_SIZE);

        data_size -= itd->transactions[1].bits.length;
        buffer_lo += itd->transactions[1].bits.length;

        itd->transactions[1].bits.page_select = 1;

        itd->buffer_page_1.bits.buffer_address = buffer_lo >> 12;

        if(metadata->addr64) {
            itd->ext_buffer_pages[1] = buffer_hi;
        }

        itd->last_transaction = 1;
    }


    if(data_size > 0) {
        itd->transactions[2].bits.active = 1;
        itd->transactions[2].bits.offset = 0;
        itd->transactions[2].bits.length = MIN(data_size, FRAME_SIZE);

        data_size -= itd->transactions[2].bits.length;
        buffer_lo += itd->transactions[2].bits.length;

        itd->transactions[2].bits.page_select = 2;

        itd->buffer_page_2.bits.buffer_address = buffer_lo >> 12;

        if(metadata->addr64) {
            itd->ext_buffer_pages[2] = buffer_hi;
        }

        itd->last_transaction = 2;
    }

    itd->transactions[itd->last_transaction].bits.ioc = 1;

    // TODO: remaining length for testing keyboard it is enough

    PRINTLOG(USB, LOG_TRACE, "EHCI itd %x buffer filled, frame linking...", itd->id);

    uint32_t frame_index = ((metadata->op_regs->frame_index + 31) / 8) % metadata->v_frame_entries_count;

    PRINTLOG(USB, LOG_DEBUG, "frame index %d", frame_index);

    lock_acquire(metadata->periodic_list_lock);

    usb_ehci_itd_t* frame_itds = (usb_ehci_itd_t*)metadata->itd_va;

    usb_ehci_itd_t* frame_itd = &frame_itds[frame_index];

    itd->prev = frame_itd;
    itd->next_link_pointer.raw = frame_itd->next_link_pointer.raw;

    if(itd->next_link_pointer.bits.next_link_pointer == 0) {
        itd->next_link_pointer.bits.terminate_bit = 1;
    }

    frame_itd->next_link_pointer.raw = itd->this_raw;
    frame_itd->need_seek = true;

    metadata->current_periodic_transfer_count++;

    lock_release(metadata->periodic_list_lock);


    PRINTLOG(USB, LOG_TRACE, "EHCI itd %x frame linked to 0x%p nlp 0x%x this raw 0x%x", itd->id, frame_itd, itd->next_link_pointer.raw, itd->this_raw);
    logging_set_level(USB, LOG_DEBUG);
    return 0;
}

int8_t usb_ehci_bulk_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    PRINTLOG(USB, LOG_TRACE, "EHCI bulk transfer");

    usb_controller_metadata_t* metadata = usb_controller->metadata;

    usb_ehci_qtd_t* qtd = NULL;
    usb_ehci_qtd_t* head = NULL;
    usb_ehci_qtd_t* prev = NULL;

    uint32_t toggle = transfer->endpoint->toggle;
    uint32_t max_packet_size = transfer->endpoint->desc->max_packet_size;

    uint32_t packet_type = transfer->endpoint->in;
    uint32_t remaining = transfer->length;
    uint8_t* data = transfer->data;

    PRINTLOG(USB, LOG_TRACE, "remaining: 0x%x", remaining);

    while(remaining) {
        lock_acquire(metadata->async_list_lock);
        qtd = usb_ehci_find_qtd(usb_controller);
        lock_release(metadata->async_list_lock);

        if(qtd == NULL) {
            PRINTLOG(USB, LOG_ERROR, "no free qtds");

            return -1;
        }

        if(head == NULL) {
            head = qtd;
        }

        toggle ^= 1;

        uint32_t data_size = MIN(remaining, max_packet_size);

        if(usb_ehci_init_qtd(usb_controller, qtd, prev, toggle, packet_type, data, data_size) != 0) {
            PRINTLOG(USB, LOG_ERROR, "failed to init qtd");

            return -1;
        }

        prev = qtd;

        remaining -= data_size;
        data += data_size;
    }

    qtd->token.bits.interrupt_on_complete = 1;


    lock_acquire(metadata->async_list_lock);
    usb_ehci_qh_t* qh = usb_ehci_find_qh(usb_controller);
    lock_release(metadata->async_list_lock);

    if(qh == NULL) {
        PRINTLOG(USB, LOG_ERROR, "no free qhs");

        return -1;
    }

    if(usb_ehci_init_qh(usb_controller, qh, head, qtd, transfer, false) != 0) {
        PRINTLOG(USB, LOG_ERROR, "failed to init qh");

        return -1;
    }

    lock_acquire(metadata->async_list_lock);

    qh->transfer = transfer;

    if(usb_ehci_link_qh(usb_controller, metadata->qh_asynclist_head, qh) != 0) {
        PRINTLOG(USB, LOG_ERROR, "failed to link qh");
        lock_release(metadata->async_list_lock);

        return -1;
    }

    metadata->current_async_transfer_count++;

    lock_release(metadata->async_list_lock);

    PRINTLOG(USB, LOG_TRACE, "EHCI qh 0x%p qtd 0x%p", qh, qtd);

    if(transfer->need_future) {
        qh->transfer_lock = lock_create_for_future(task_get_id());
        transfer->transfer_future = future_create(qh->transfer_lock);
    }

    return 0;
}

int8_t usb_ehci_control_transfer(usb_controller_t* usb_controller, usb_transfer_t* transfer) {
    PRINTLOG(USB, LOG_TRACE, "EHCI control transfer");

    usb_controller_metadata_t* metadata = usb_controller->metadata;


    usb_device_request_t* request = transfer->request;
    usb_device_t* device = transfer->device;

    lock_acquire(metadata->async_list_lock);
    usb_ehci_qtd_t* qtd = usb_ehci_find_qtd(usb_controller);
    lock_release(metadata->async_list_lock);

    if(qtd == NULL) {
        PRINTLOG(USB, LOG_ERROR, "no free qtds");

        return -1;
    }

    usb_ehci_qtd_t* head = qtd;
    usb_ehci_qtd_t* prev = NULL;

    uint32_t toggle = 0;

    uint32_t packet_type = USB_EHCI_PID_SETUP;
    uint32_t max_packet_size = device->max_packet_size;

    if(usb_ehci_init_qtd(usb_controller, qtd, prev, toggle, packet_type, request, sizeof(usb_device_request_t)) != 0) {
        PRINTLOG(USB, LOG_ERROR, "failed to init qtd");

        return -1;
    }

    prev = qtd;

    packet_type = request->type & USB_REQUEST_DIRECTION_DEVICE_TO_HOST ? USB_EHCI_PID_IN : USB_EHCI_PID_OUT;
    uint32_t remaining = transfer->length;
    uint8_t* data = transfer->data;

    PRINTLOG(USB, LOG_TRACE, "remaining: 0x%x", remaining);

    while(remaining) {
        lock_acquire(metadata->async_list_lock);
        qtd = usb_ehci_find_qtd(usb_controller);
        lock_release(metadata->async_list_lock);

        if(qtd == NULL) {
            PRINTLOG(USB, LOG_ERROR, "no free qtds");

            return -1;
        }

        toggle ^= 1;

        uint32_t data_size = MIN(remaining, max_packet_size);

        if(usb_ehci_init_qtd(usb_controller, qtd, prev, toggle, packet_type, data, data_size) != 0) {
            PRINTLOG(USB, LOG_ERROR, "failed to init qtd");

            return -1;
        }

        prev = qtd;

        remaining -= data_size;
        data += data_size;
    }

    lock_acquire(metadata->async_list_lock);
    qtd = usb_ehci_find_qtd(usb_controller);
    lock_release(metadata->async_list_lock);

    if(qtd == NULL) {
        PRINTLOG(USB, LOG_ERROR, "no free qtds");

        return -1;
    }

    toggle = 1;

    packet_type = request->type & USB_REQUEST_DIRECTION_DEVICE_TO_HOST ? USB_EHCI_PID_OUT : USB_EHCI_PID_IN;

    if(usb_ehci_init_qtd(usb_controller, qtd, prev, toggle, packet_type, NULL, 0) != 0) {
        PRINTLOG(USB, LOG_ERROR, "failed to init qtd");

        return -1;
    }

    lock_acquire(metadata->async_list_lock);
    usb_ehci_qh_t* qh = usb_ehci_find_qh(usb_controller);
    lock_release(metadata->async_list_lock);

    if(qh == NULL) {
        PRINTLOG(USB, LOG_ERROR, "no free qhs");

        return -1;
    }

    if(usb_ehci_init_qh(usb_controller, qh, head, qtd, transfer, false) != 0) {
        PRINTLOG(USB, LOG_ERROR, "failed to init qh");

        return -1;
    }

    lock_acquire(metadata->async_list_lock);

    if(usb_ehci_link_qh(usb_controller, metadata->qh_asynclist_head, qh) != 0) {
        PRINTLOG(USB, LOG_ERROR, "failed to link qh");
        lock_release(metadata->async_list_lock);

        return -1;
    }

    usb_ehci_wait_for_transfer(usb_controller, qh, transfer);

    lock_release(metadata->async_list_lock);

    return 0;
}

void usb_ehci_poll(usb_controller_t* usb_controller, usb_ehci_qh_t* qh, usb_transfer_t* transfer) {
    if(qh->token.bits.halted) {
        PRINTLOG(USB, LOG_ERROR, "transfer halted");
        transfer->complete = true;
        transfer->success = false;
    } else if(qh->next_qtd_pointer.bits.next_qtd_terminate) {
        if(qh->token.bits.data_buffer_err) {
            PRINTLOG(USB, LOG_ERROR, "data buffer error");
        } else if(qh->token.bits.babble_err) {
            PRINTLOG(USB, LOG_ERROR, "babble error");
        } else if(qh->token.bits.xact_err) {
            PRINTLOG(USB, LOG_ERROR, "transaction error");
        } else if(qh->token.bits.missed_microframe) {
            PRINTLOG(USB, LOG_ERROR, "missed microframe");
        } else {
            transfer->success = true;
            PRINTLOG(USB, LOG_TRACE, "transfer successful");
        }

        transfer->complete = true;
        PRINTLOG(USB, LOG_TRACE, "transfer complete");
    }

    if(transfer->complete) {
        usb_controller_metadata_t* metadata = usb_controller->metadata;

        if(transfer->success && transfer->endpoint) {
            transfer->endpoint->toggle ^= 1;
        }

        usb_ehci_unlink_qh(usb_controller, metadata->qh_asynclist_head, qh);
        usb_ehci_free_qh(usb_controller, qh);
    }
}

void usb_ehci_wait_for_transfer(usb_controller_t* usb_controller, usb_ehci_qh_t* qh, usb_transfer_t* transfer) {
    while(!transfer->complete) {
        PRINTLOG(USB, LOG_TRACE, "waiting for transfer to complete");
        usb_ehci_poll(usb_controller, qh, transfer);
        task_yield();
    }
}


int8_t usb_ehci_reset_port(usb_controller_t* usb_controller, uint8_t port) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    PRINTLOG(USB, LOG_TRACE, "resetting port %d", port);

    uint32_t portsc = metadata->op_regs->port_status_and_control[port];

    usb_ehci_port_sts_and_ctrl_t portsc_struct = { .raw = portsc };

    portsc_struct.bits.reset = 1;
    portsc_struct.bits.connect_status_change = 1;
    portsc_struct.bits.enable_status_change = 1;

    metadata->op_regs->port_status_and_control[port] = portsc_struct.raw;

    time_timer_spinsleep(100);

    portsc_struct.bits.reset = 0;

    metadata->op_regs->port_status_and_control[port] = portsc_struct.raw;

    time_timer_spinsleep(100);

    portsc = metadata->op_regs->port_status_and_control[port];
    portsc_struct.raw = portsc;

    if(portsc_struct.bits.reset == 1) {
        PRINTLOG(USB, LOG_ERROR, "port %d reset failed", port);
        return -1;
    }

    portsc = metadata->op_regs->port_status_and_control[port];
    portsc_struct.raw = portsc;

    if(portsc_struct.bits.connect_status_change && portsc_struct.bits.enable_status_change) {

        portsc_struct.bits.connect_status_change = 1;
        portsc_struct.bits.enable_status_change = 1;
        metadata->op_regs->port_status_and_control[port] = portsc_struct.raw;
    }

    if(portsc_struct.bits.port_enabled) {
        PRINTLOG(USB, LOG_TRACE, "port %d enabled", port);
    } else {
        PRINTLOG(USB, LOG_TRACE, "port %d disabled", port);
    }

    PRINTLOG(USB, LOG_TRACE, "port %d reset complete", port);

    return portsc_struct.bits.port_enabled;
}


int8_t usb_ehci_probe_port(usb_controller_t* usb_controller, uint8_t port) {
    // usb_controller_metadata_t* metadata = usb_controller->metadata;

    int8_t res = usb_ehci_reset_port(usb_controller, port);

    if(res < 0) {
        return -1;
    }

    if(res == 1) {
        PRINTLOG(USB, LOG_TRACE, "port %d enabled. we will initialize driver", port);

        if(usb_device_init(NULL, usb_controller, port, USB_ENDPOINT_SPEED_HIGH) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot initialize device on port %d", port);

            return -1;
        }
    }

    return 0;
}

int8_t usb_ehci_probe_all_ports(usb_controller_t* usb_controller) {
    usb_controller_metadata_t* metadata = usb_controller->metadata;

    boolean_t failed = false;

    for(int16_t i = 0; i < metadata->port_count; i++) {
        if(usb_ehci_probe_port(usb_controller, i) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot probe port %d", i);
            failed = true;
        }
    }

    return failed ? -1 : 0;
}

int8_t usb_ehci_init(usb_controller_t* usb_controller) {
    if(usb_ehci_controllers == NULL) {
        usb_ehci_controllers = hashmap_integer(16);

        if(!usb_ehci_controllers) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb ehci controllers hashmap");

            return -1;
        }
    }


    hashmap_put(usb_ehci_controllers, (void*)usb_controller->controller_id, usb_controller);

    const pci_dev_t* pci_dev = usb_controller->pci_dev;

    usb_controller_metadata_t* metadata = memory_malloc(sizeof(usb_controller_metadata_t));

    if(!metadata) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb controller metadata");

        return -1;
    }

    metadata->async_list_lock = lock_create();

    if(!metadata->async_list_lock) {
        PRINTLOG(USB, LOG_ERROR, "cannot create lock for async list");
        memory_free(metadata);

        return -1;
    }

    metadata->periodic_list_lock = lock_create();

    if(!metadata->periodic_list_lock) {
        PRINTLOG(USB, LOG_ERROR, "cannot create lock for periodic list");
        lock_destroy(metadata->async_list_lock);
        memory_free(metadata);

        return -1;
    }

    usb_controller->metadata = metadata;
    usb_controller->probe_all_ports = usb_ehci_probe_all_ports;
    usb_controller->probe_port = usb_ehci_probe_port;
    usb_controller->reset_port = usb_ehci_reset_port;
    usb_controller->control_transfer = usb_ehci_control_transfer;
    usb_controller->isochronous_transfer = usb_ehci_isochronous_transfer;
    usb_controller->bulk_transfer = usb_ehci_bulk_transfer;

    PRINTLOG(USB, LOG_INFO, "EHCI controller found: %02x:%02x:%02x:%02x at mem 0x%p",
             pci_dev->group_number, pci_dev->bus_number, pci_dev->device_number, pci_dev->function_number, pci_dev->pci_header);

    uint64_t bar_fa = pci_get_bar_address((pci_generic_device_t*)pci_dev->pci_header, 0);
    uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

    usb_ehci_controller_capabilties_t* cap = (usb_ehci_controller_capabilties_t*)bar_va;

    usb_ehci_hcsparams_t hcsparams = (usb_ehci_hcsparams_t)cap->hcsparams;
    usb_ehci_hccparams_t hccparams = (usb_ehci_hccparams_t)cap->hccparams;
    PRINTLOG(USB, LOG_DEBUG, "EHCI controller capabilities:");
    PRINTLOG(USB, LOG_DEBUG, "  Port count: %d", hcsparams.bits.n_ports);
    PRINTLOG(USB, LOG_DEBUG, "  64-bit addressing: %d", hccparams.bits.addr64);
    PRINTLOG(USB, LOG_DEBUG, "  EECP: %d", hccparams.bits.eecp);

    metadata->port_count = hcsparams.bits.n_ports;

    metadata->addr64 = hccparams.bits.addr64;

    if(hccparams.bits.eecp != 0 && hccparams.bits.eecp >= 0x40) {
        uint64_t pci_base = (uint64_t)pci_dev->pci_header;
        uint32_t raw_l_caps = read_memio(pci_base + hccparams.bits.eecp, 32);
        usb_legacy_support_capabilities_t l_caps = (usb_legacy_support_capabilities_t)raw_l_caps;
        PRINTLOG(USB, LOG_DEBUG, "EHCI controller legacy support capabilities: 0x%x", raw_l_caps);


        if(l_caps.bits.capability_id == 1) {
            PRINTLOG(USB, LOG_TRACE, "  Bios owned: %d", l_caps.bits.bios_owned_semaphore);
            PRINTLOG(USB, LOG_TRACE, "  OS owned: %d", l_caps.bits.os_owned_semaphore);

            if(l_caps.bits.os_owned_semaphore == 0 || l_caps.bits.bios_owned_semaphore) {
                PRINTLOG(USB, LOG_WARNING, "  Releasing BIOS ownership of EHCI controller");
                l_caps.bits.os_owned_semaphore = 1;
                l_caps.bits.bios_owned_semaphore = 0;

                PRINTLOG(USB, LOG_WARNING, "  Writing 0x%x to EHCI controller, original 0x%x", l_caps.raw, raw_l_caps);

                write_memio(pci_base + hccparams.bits.eecp, 32, l_caps.raw);

                time_timer_spinsleep(500);

                raw_l_caps = read_memio(pci_base + hccparams.bits.eecp, 32);

                PRINTLOG(USB, LOG_WARNING, "result 0x%x", raw_l_caps);
            }
        }
    }

    if(usb_controller->msix_cap == NULL) {
        pci_generic_device_t* pci_generic_device = (pci_generic_device_t*)pci_dev->pci_header;

        uint8_t irq = pci_generic_device->interrupt_line;
        irq = apic_get_irq_override(irq);

        interrupt_irq_set_handler(irq, usb_ehci_isr);
        apic_ioapic_setup_irq(irq, APIC_IOAPIC_TRIGGER_MODE_LEVEL);

        apic_ioapic_enable_irq(irq);
        pci_enable_interrupt(pci_generic_device);
    } else {
        PRINTLOG(USB, LOG_TRACE, "TODO: MSI-X enabled");
    }

    usb_ehci_op_regs_t* op_regs = (usb_ehci_op_regs_t*)(bar_va + cap->caplength);
    metadata->op_regs = op_regs;

    usb_ehci_cmd_reg_t cmd_reg = (usb_ehci_cmd_reg_t)op_regs->usb_cmd;
    // cmd_reg.bits.host_controller_reset = 1;
    cmd_reg.bits.run_stop = 0;
    op_regs->usb_cmd = cmd_reg.raw;
    usb_ehci_sts_reg_t sts_reg;

    /*
       while(true) {
        cmd_reg = (usb_ehci_cmd_reg_t)op_regs->usb_cmd;

        if(cmd_reg.bits.host_controller_reset == 0) {
            break;
        }
       }
     */
    while(true) {
        sts_reg = (usb_ehci_sts_reg_t)op_regs->usb_sts;

        if(sts_reg.bits.host_cotroller_halted == 1) {
            break;
        }
    }

    metadata->qh_count = 128;
    metadata->qh_used_mask = memory_malloc(metadata->qh_count / (sizeof(uint64_t) * 8));
    metadata->qtd_count = 1024;
    metadata->qtd_used_mask = memory_malloc(metadata->qtd_count / (sizeof(uint64_t) * 8));
    metadata->int_frame_entries_count = 8;
    metadata->v_frame_entries_count = 128;
    metadata->framelist_entries_count = 1024;
    metadata->itd_pool_count = 256;
    metadata->itd_pool_used_mask = memory_malloc(metadata->itd_pool_count / (sizeof(uint64_t) * 8));

    if(!metadata->itd_pool_used_mask) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for ITD pool used mask");
        lock_destroy(metadata->async_list_lock);
        lock_destroy(metadata->periodic_list_lock);
        memory_free(metadata);

        return -1;
    }

    uint64_t data_fa_size = sizeof(uint32_t) * 1024;
    data_fa_size += (FRAME_SIZE - (data_fa_size % FRAME_SIZE));
    uint64_t qh_offset = data_fa_size;
    data_fa_size += sizeof(usb_ehci_qh_t) * metadata->qh_count;
    data_fa_size += (FRAME_SIZE - (data_fa_size % FRAME_SIZE));
    uint64_t qtd_offset = data_fa_size;
    data_fa_size += sizeof(usb_ehci_qtd_t) * metadata->qtd_count;
    data_fa_size += (FRAME_SIZE - (data_fa_size % FRAME_SIZE));
    uint64_t int_qh_offset = data_fa_size;
    data_fa_size += sizeof(usb_ehci_qh_t) * metadata->int_frame_entries_count;
    data_fa_size += (FRAME_SIZE - (data_fa_size % FRAME_SIZE));
    uint64_t itd_offset = data_fa_size;
    data_fa_size += sizeof(usb_ehci_itd_t) * metadata->v_frame_entries_count;
    data_fa_size += (FRAME_SIZE - (data_fa_size % FRAME_SIZE));
    uint64_t sitd_offset = data_fa_size;
    data_fa_size += sizeof(usb_ehci_sitd_t) * metadata->v_frame_entries_count;
    data_fa_size += (FRAME_SIZE - (data_fa_size % FRAME_SIZE));
    uint64_t itd_pool_offset = data_fa_size;
    data_fa_size += sizeof(usb_ehci_itd_t) * metadata->itd_pool_count;
    data_fa_size += (FRAME_SIZE - (data_fa_size % FRAME_SIZE));

    uint64_t data_fa_count = data_fa_size / FRAME_SIZE;

    frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK;

    if(!metadata->addr64) {
        fa_type |= FRAME_ALLOCATION_TYPE_UNDER_4G;
    }

    frame_t* data_frames = NULL;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), data_fa_count, fa_type, &data_frames, NULL) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb controller data");

        return -1;
    }

    uint64_t data_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(data_frames->frame_address);

    if(memory_paging_add_va_for_frame(data_va, data_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot add memory for usb controller data to paging");

        return -1;
    }

    memory_memclean((void*)data_va, data_fa_size);

    metadata->framelist_fa = data_frames->frame_address;
    metadata->framelist_va = data_va;
    metadata->framelist = (usb_ehci_framelist_item_t*)data_va;
    PRINTLOG(USB, LOG_TRACE, "framelist: 0x%llx", metadata->framelist_fa);

    metadata->qh_fa = data_frames->frame_address + qh_offset;
    metadata->qh_va = data_va + qh_offset;

    metadata->qtd_fa = data_frames->frame_address + qtd_offset;
    metadata->qtd_va = data_va + qtd_offset;

    metadata->int_frame_entries_fa = data_frames->frame_address + int_qh_offset;
    metadata->int_frame_entries_va = data_va + int_qh_offset;

    metadata->itd_fa = data_frames->frame_address + itd_offset;
    metadata->itd_va = data_va + itd_offset;

    metadata->sitd_fa = data_frames->frame_address + sitd_offset;
    metadata->sitd_va = data_va + sitd_offset;

    metadata->itd_pool_fa = data_frames->frame_address + itd_pool_offset;
    metadata->itd_pool_va = data_va + itd_pool_offset;

    if(metadata->addr64) {
        op_regs->ctrl_ds_segment = data_frames->frame_address >> 32;
    }

    usb_ehci_qh_t* qh = NULL;

    PRINTLOG(USB, LOG_TRACE, "build async list");
    qh = usb_ehci_find_qh(usb_controller);

    if(qh == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot find free qh");

        return -1;
    }

    uint64_t qh_asynclist_head_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)qh);

    qh->horizontal_link_pointer.raw = qh_asynclist_head_fa;
    qh->horizontal_link_pointer.bits.qtd_type = USB_EHCI_QTD_TYPE_QH;
    qh->endpoint_characteristics.bits.head_of_reclamation_list_flag = 1;
    qh->endpoint_characteristics.bits.endpoint_speed = USB_ENDPOINT_SPEED_HIGH;
    qh->endpoint_capabilities.bits.mult = 1;
    qh->next_qtd_pointer.bits.next_qtd_terminate = 1;
    qh->this_raw = qh->horizontal_link_pointer.raw;
    qh->prev_raw = qh->horizontal_link_pointer.raw;

    metadata->qh_asynclist_head = qh;
    op_regs->async_list_addr = qh_asynclist_head_fa & 0xFFFFFFF0;

    PRINTLOG(USB, LOG_TRACE, "aync list addr: 0x%llx", qh_asynclist_head_fa);

    PRINTLOG(USB, LOG_TRACE, "build periodic list");

    void** plt_args = memory_malloc(sizeof(void*) * 2);

    if(plt_args == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for periodic list task args");
        memory_free(metadata);

        return -1;
    }

    plt_args[0] = usb_controller;
    metadata->periodic_list_tid = task_create_task(NULL, 128 << 10, 64 << 10, usb_ehci_periodiclist_lookup_task, 1, plt_args, "usb_ehci_periodiclist_task");

    if(metadata->periodic_list_tid == -1ULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot create periodic list task");
        memory_free(plt_args);
        memory_free(metadata);

        return -1;
    }

    metadata->async_list_tid = task_create_task(NULL, 128 << 10, 64 << 10, usb_ehci_asynclist_lookup_task, 1, plt_args, "usb_ehci_asynclist_task");

    if(metadata->async_list_tid == -1ULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot create async list task");
        memory_free(plt_args);
        memory_free(metadata);

        return -1;
    }

    usb_ehci_qh_t* int_qh = (usb_ehci_qh_t*)metadata->int_frame_entries_va;
    uint64_t int_qh_phys = metadata->int_frame_entries_fa;

    for(uint64_t i = 0; i < metadata->int_frame_entries_count; i++) {
        int_qh[i].horizontal_link_pointer.raw = int_qh_phys;
        int_qh[i].horizontal_link_pointer.bits.qtd_type = USB_EHCI_QTD_TYPE_QH;

        int_qh[i].endpoint_characteristics.bits.nak_count_reload = 3;
        int_qh[i].endpoint_characteristics.bits.endpoint_speed = USB_ENDPOINT_SPEED_HIGH;
        int_qh[i].endpoint_characteristics.bits.data_toggle_control = 1;
        int_qh[i].endpoint_characteristics.bits.max_packet_length = 64;

        int_qh[i].endpoint_capabilities.bits.mult = 1;
        int_qh[i].endpoint_capabilities.bits.interrupt_schedule_mask = 0xff;

        int_qh[i].next_qtd_pointer.bits.next_qtd_terminate = 1;
        int_qh[i].alternate_qtd_pointer.bits.next_qtd_terminate = 1;

        int_qh[i].token.bits.halted = 1;

        int_qh[i].this_raw = int_qh[i].horizontal_link_pointer.raw;
        int_qh[i].prev_raw = int_qh[i].horizontal_link_pointer.raw;

        int_qh_phys += sizeof(usb_ehci_qh_t);
    }

    usb_ehci_itd_t* itds = (usb_ehci_itd_t*)metadata->itd_va;
    uint64_t itds_phys = metadata->itd_fa;

    usb_ehci_sitd_t* sitds = (usb_ehci_sitd_t*)metadata->sitd_va;
    uint64_t sitds_phys = metadata->sitd_fa;

    for(uint64_t i = 0; i < metadata->v_frame_entries_count; i++) {
        sitds[i].back_link_pointer.bits.terminate_bit = 1;
        sitds[i].this_raw = sitds_phys |  (USB_EHCI_QTD_TYPE_SITD << 1);


        itds[i].next_link_pointer.raw = sitds[i].this_raw;
        itds[i].this_raw = itds_phys | (USB_EHCI_QTD_TYPE_ITD << 1);

        sitds_phys += sizeof(usb_ehci_sitd_t);
        itds_phys += sizeof(usb_ehci_itd_t);
    }

    uint64_t interval = metadata->v_frame_entries_count;
    uint64_t interval_index = metadata->int_frame_entries_count - 1;

    while(interval > 1) {
        for(uint64_t inserted_index = interval / 2; inserted_index < metadata->v_frame_entries_count; inserted_index += interval) {
            sitds[inserted_index].next_link_pointer.raw = int_qh[interval_index].this_raw;
        }

        interval_index--;
        interval >>= 1;
    }

    sitds[0].next_link_pointer.raw = int_qh[0].this_raw;

    for(uint64_t i = 1; i < metadata->int_frame_entries_count; i++) {
        int_qh[i].next_qtd_pointer.raw = int_qh[0].this_raw;
    }

    int_qh[0].next_qtd_pointer.raw = 0;
    int_qh[0].next_qtd_pointer.bits.next_qtd_terminate = 1;

    for(uint64_t i = 0; i < metadata->framelist_entries_count; i++) {
        metadata->framelist[i].raw = itds[i % metadata->v_frame_entries_count].this_raw;
        metadata->framelist[i].bits.type = USB_EHCI_QTD_TYPE_ITD;
    }

    op_regs->frame_index = 0;
    op_regs->periodic_list_base = metadata->framelist_fa;

    sts_reg.raw = 0x3f;
    op_regs->usb_sts = sts_reg.raw;

    usb_ehci_int_enable_reg_t int_enable_reg;
    int_enable_reg.bits.usb_interrupt_enable = 1;
    int_enable_reg.bits.usb_error_enable = 1;
    int_enable_reg.bits.port_change_enable = 1;
    // int_enable_reg.bits.frame_list_rollover_enable = 1;
    int_enable_reg.bits.host_system_error_enable = 1;
    int_enable_reg.bits.interrupt_on_async_advance_enable = 1;

    op_regs->usb_intr = int_enable_reg.raw;

    cmd_reg.raw = 0;
    cmd_reg.bits.run_stop = 1;
    cmd_reg.bits.interrupt_on_async_advance_enable = 1;
    cmd_reg.bits.async_schedule_enable = 1;
    cmd_reg.bits.periodic_schedule_enable = 1;
    cmd_reg.bits.interrupt_threshold_control = 8;

    op_regs->usb_cmd = cmd_reg.raw;

    while(true) {
        sts_reg = (usb_ehci_sts_reg_t)op_regs->usb_sts;

        if(sts_reg.bits.host_cotroller_halted == 0) {
            break;
        }
    }

    op_regs->config_flag = 1;

    usb_controller->initialized = true;

    PRINTLOG(USB, LOG_INFO, "usb controller initialized");

    usb_controller->probe_all_ports(usb_controller);


    return 0;
}

int8_t usb_ehci_asynclist_lookup_task(int32_t argc, void** argv) {
    if(argc != 1 || argv == NULL || argv[0] == NULL) {
        PRINTLOG(USB, LOG_ERROR, "invalid argument count");

        return -1;
    }

    cpu_sti();
    task_set_interruptible();
    cpu_cli();


    usb_controller_t* usb_controller = (usb_controller_t*)argv[0];
    usb_controller_metadata_t* metadata = (usb_controller_metadata_t*)usb_controller->metadata;

    uint64_t mem_hi = 0;

    if(metadata->addr64) {
        mem_hi = metadata->op_regs->ctrl_ds_segment;
        mem_hi <<= 32;
    }

    boolean_t need_yield = false;

    while(true) {
        uint64_t transfer_count = 0;

        lock_acquire(metadata->periodic_list_lock);
        transfer_count = metadata->current_async_transfer_count;
        lock_release(metadata->periodic_list_lock);

        if(transfer_count == 0 || need_yield) {
            task_set_message_waiting();
            task_yield();

            need_yield = false;

            continue;
        }

        PRINTLOG(USB, LOG_TRACE, "transfer count: %llx", transfer_count);

        uint64_t async_list_head_fa = mem_hi | metadata->op_regs->async_list_addr;
        uint64_t async_list_head_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(async_list_head_fa);

        usb_ehci_qh_t* async_list_head = (usb_ehci_qh_t*)async_list_head_va;
        usb_ehci_qh_t* qh = async_list_head;

        lock_acquire(metadata->async_list_lock);

        while(transfer_count) {
            if(qh->horizontal_link_pointer.bits.terminate) {
                PRINTLOG(USB, LOG_TRACE, "async list finished");
                break;
            }



            if(qh->transfer && !qh->token.bits.active) {
                usb_transfer_t* transfer = qh->transfer;
                PRINTLOG(USB, LOG_TRACE, "transfer: 0x%p", transfer);

                if(qh->token.bits.halted) {
                    PRINTLOG(USB, LOG_ERROR, "transfer halted");
                    transfer->complete = true;
                    transfer->success = false;
                } else if(qh->next_qtd_pointer.bits.next_qtd_terminate) {
                    if(qh->token.bits.data_buffer_err) {
                        PRINTLOG(USB, LOG_ERROR, "data buffer error");
                    } else if(qh->token.bits.babble_err) {
                        PRINTLOG(USB, LOG_ERROR, "babble error");
                    } else if(qh->token.bits.xact_err) {
                        PRINTLOG(USB, LOG_ERROR, "transaction error");
                    } else if(qh->token.bits.missed_microframe) {
                        PRINTLOG(USB, LOG_ERROR, "missed microframe");
                    } else {
                        transfer->success = true;
                        PRINTLOG(USB, LOG_TRACE, "transfer successful");
                    }

                    transfer->complete = true;
                    PRINTLOG(USB, LOG_TRACE, "transfer completed.");
                }

                if(transfer->complete) {
                    PRINTLOG(USB, LOG_TRACE, "success: %d", transfer->success);

                    if(transfer->success && transfer->endpoint) {
                        transfer->endpoint->toggle ^= 1;
                    }

                    lock_t* transfer_lock = qh->transfer_lock;

                    usb_ehci_unlink_qh(usb_controller, metadata->qh_asynclist_head, qh);
                    usb_ehci_free_qh(usb_controller, qh);

                    if(transfer->need_future) {
                        lock_release(transfer_lock);
                    }

                    if(transfer->transfer_callback) {
                        transfer->transfer_callback(usb_controller, transfer);
                    }

                    metadata->current_async_transfer_count--;
                    transfer_count--;
                }
            }

            uint64_t next_qh_fa = mem_hi | (qh->horizontal_link_pointer.raw & 0xFFFFFFE0);

            if(next_qh_fa == 0) {
                PRINTLOG(USB, LOG_TRACE, "next qh fa is 0");
                break;
            }

            uint64_t next_qh_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(next_qh_fa);

            qh = (usb_ehci_qh_t*)next_qh_va;
            PRINTLOG(USB, LOG_TRACE, "next qh: 0x%p", qh);

            if(qh == async_list_head) {
                PRINTLOG(USB, LOG_TRACE, "async list looped");
                need_yield = true;
                break;
            }
        }

        lock_release(metadata->async_list_lock);
    }

}

int8_t usb_ehci_periodiclist_lookup_task(int32_t argc, void** argv) {
    if(argc != 1 || argv == NULL || argv[0] == NULL) {
        PRINTLOG(USB, LOG_ERROR, "invalid argument count");

        return -1;
    }

    cpu_sti();
    task_set_interruptible();
    cpu_cli();


    usb_controller_t* usb_controller = (usb_controller_t*)argv[0];
    usb_controller_metadata_t* metadata = (usb_controller_metadata_t*)usb_controller->metadata;

    uint64_t mem_hi = 0;

    if(metadata->addr64) {
        mem_hi = metadata->op_regs->ctrl_ds_segment;
        mem_hi <<= 32;
    }

    while(true) {
        uint64_t transfer_count = 0;

        lock_acquire(metadata->periodic_list_lock);
        transfer_count = metadata->current_periodic_transfer_count;
        lock_release(metadata->periodic_list_lock);

        if(transfer_count == 0) {
            task_set_message_waiting();
            task_yield();

            continue;
        }

        PRINTLOG(USB, LOG_DEBUG, "transfer count: %llx", transfer_count);

        usb_ehci_itd_t* itds = (usb_ehci_itd_t*)metadata->itd_va;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        volatile uint32_t* v_current_frame_index = (volatile uint32_t*)&metadata->op_regs->frame_index;
#pragma GCC diagnostic pop

        uint32_t current_frame_index = *v_current_frame_index / 8 % metadata->v_frame_entries_count;
        uint32_t current_frame = (current_frame_index + metadata->v_frame_entries_count - 5) % metadata->v_frame_entries_count;

        uint32_t idx = 0;

        while(transfer_count && idx++ < metadata->v_frame_entries_count) {
            while(current_frame == (*v_current_frame_index / 8 % metadata->v_frame_entries_count)) {
                PRINTLOG(USB, LOG_TRACE, "we reached the current frame, need to wait");

                time_timer_spinsleep(500);
            }

            usb_ehci_itd_t* itd = &itds[current_frame];
            usb_ehci_itd_t* itd_head = itd;

            lock_acquire(metadata->periodic_list_lock);

            if(!itd->need_seek) {
                lock_release(metadata->periodic_list_lock);

                current_frame = (current_frame + 1) % metadata->v_frame_entries_count;

                continue;
            }

            while (itd->next_link_pointer.bits.terminate_bit != 1) {
                uint32_t this_raw = (uint32_t)(uint64_t)itd & 0xFFFFFFE0;
                uint32_t next_raw = itd->next_link_pointer.raw & 0xFFFFFFE0;
                PRINTLOG(USB, LOG_TRACE, "itd: 0x%p 0x%x 0x%x nlp 0x%x", itd, this_raw, next_raw, itd->next_link_pointer.raw);

                if(this_raw == next_raw) {
                    break;
                }

                uint64_t mem_lo = mem_hi | (itd->next_link_pointer.raw & 0xFFFFFFE0);
                mem_lo = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(mem_lo);

                usb_ehci_itd_t* itd_entry = (usb_ehci_itd_t*)mem_lo;

                if(itd->next_link_pointer.bits.type != USB_EHCI_QTD_TYPE_ITD) {
                    itd = itd_entry;

                    continue;
                }

                if(itd_entry->transfer) {
                    usb_transfer_t* transfer = itd_entry->transfer;

                    PRINTLOG(USB, LOG_TRACE, "we have a transfer 0x%p", itd_entry->transfer);

                    int8_t finished_tran_count = 0;

                    for(int32_t i = 0; i <= itd_entry->last_transaction; i++) {
                        if(itd_entry->transactions[i].bits.active == 0) {
                            PRINTLOG(USB, LOG_TRACE, "transaction %d finished", i);

                            PRINTLOG(USB, LOG_TRACE, "status %d %d %d",
                                     itd_entry->transactions[i].bits.data_buffer_err,
                                     itd_entry->transactions[i].bits.babble_err,
                                     itd_entry->transactions[i].bits.xact_err);


                            if(itd_entry->transactions[i].bits.data_buffer_err) {
                                transfer->error_count++;
                            }

                            if(itd_entry->transactions[i].bits.babble_err) {
                                transfer->error_count++;
                            }

                            if(itd_entry->transactions[i].bits.xact_err) {
                                transfer->error_count++;
                            }

                            finished_tran_count++;
                        }
                    }

                    if(finished_tran_count == itd_entry->last_transaction + 1) {
                        PRINTLOG(USB, LOG_DEBUG, "all transactions finished");

                        transfer->complete = true;

                        if(transfer->error_count) {
                            transfer->success = false;
                            transfer->error_count = 0;
                        } else {
                            transfer->success = true;
                        }

                        if(metadata->current_periodic_transfer_count == 0) {
                            PRINTLOG(USB, LOG_ERROR, "we have no more transfers");

                            break;
                        }

                        metadata->current_periodic_transfer_count--;

                        itd_head->need_seek = false;

                        // TODO: remove idt_entry


                        // inform the transfer callback
                        if(transfer->transfer_callback) {
                            PRINTLOG(USB, LOG_TRACE, "calling transfer callback 0x%p", transfer->transfer_callback);
                            transfer->transfer_callback(usb_controller, transfer);
                        }
                    }

                }

                if(itd->next_link_pointer.bits.next_link_pointer == 0) {
                    PRINTLOG(USB, LOG_TRACE, "we reached the end of the list");

                    break;
                }

                itd = itd_entry;
            }


            lock_release(metadata->periodic_list_lock);

            current_frame = (current_frame + 1) % metadata->v_frame_entries_count;
        }
    }

    return 0;
}
