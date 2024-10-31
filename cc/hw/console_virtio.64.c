/**
 * @file console_virtio.64.c
 * @brief virtio console driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/console_virtio.h>
#include <iterator.h>
#include <logging.h>
#include <list.h>
#include <apic.h>
#include <memory/paging.h>
#include <utils.h>
#include <strings.h>
#include <cpu.h>
#include <cpu/task.h>
#include <buffer.h>

MODULE ("turnstone.kernel.hw.console.virtio");

void video_text_print(const char_t* text);

list_t* virtio_console_list = NULL;

typedef struct virtio_console_port_t {
    uint8_t   id;
    boolean_t attached;
    boolean_t open;
    char_t*   name;
    uint64_t  rx_task_id;
} virtio_console_port_t;

typedef struct virtio_console_t {
    virtio_dev_t*          vdev;
    uint8_t                port_count;
    virtio_console_port_t* ports;
    uint64_t               rx_task_id;
} virtio_console_t;

static int16_t virtio_console_get_rx_queue_number(uint16_t port_no) {
    if(port_no == 0) {
        return 0;
    }

    return 2 * port_no + 2;
}

static int16_t virtio_console_get_tx_queue_number(uint16_t port_no) {
    if(port_no == 0) {
        return 1;
    }

    return 2 * port_no + 3;
}

static int8_t vdagent_send_caps(const virtio_console_t* vconsole, uint8_t port_no) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = virtio_console_get_tx_queue_number(port_no);

    virtio_dev_t* vdev = vconsole->vdev;

    virtio_queue_ext_t* vq_tx = &vdev->queues[queue_index];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_tx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_tx->vq);

    uint16_t avail_tx_id = avail->index;

    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[avail_tx_id % vdev->queue_size].address);

    vdi_chunk_header_t* chunk_header = (vdi_chunk_header_t*)offset;

    chunk_header->port = VDP_CLIENT_PORT;

    vdagent_message_t* message = (vdagent_message_t*)(offset + sizeof(vdi_chunk_header_t));

    message->protocol = VD_AGENT_PROTOCOL;
    message->type = VD_AGENT_ANNOUNCE_CAPABILITIES;


    vdagent_cap_announce_t* cap_announce = (vdagent_cap_announce_t*)(offset + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));

    cap_announce->request = true;
    cap_announce->caps |= (1 << VD_AGENT_CAP_CLIPBOARD);
    cap_announce->caps |= (1 << VD_AGENT_CAP_CLIPBOARD_BY_DEMAND);
    cap_announce->caps |= (1 << VD_AGENT_CAP_CLIPBOARD_SELECTION);
    cap_announce->caps |= (1 << VD_AGENT_CAP_CLIPBOARD_GRAB_SERIAL);
    cap_announce->caps |= (1 << VD_AGENT_CAP_CLIPBOARD_NO_RELEASE_ON_REGRAB);
    cap_announce->caps |= (1 << VD_AGENT_CAP_MAX_CLIPBOARD);
    cap_announce->caps |= (1 << VD_AGENT_CAP_GUEST_LINEEND_LF);

    message->size = sizeof(vdagent_cap_announce_t);

    chunk_header->size = sizeof(vdagent_message_t) + message->size;

    descs[avail_tx_id % vdev->queue_size].length = sizeof(vdi_chunk_header_t) + chunk_header->size;

    avail->index++;
    vq_tx->nd->vqn = 1;

    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Caps sent");

    return 0;
}

static int8_t vdagent_send_clipboard_data(const virtio_console_t* vconsole, uint8_t port_no, const char_t* text_message) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = virtio_console_get_tx_queue_number(port_no);

    virtio_dev_t* vdev = vconsole->vdev;

    virtio_queue_ext_t* vq_tx = &vdev->queues[queue_index];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_tx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_tx->vq);

    uint16_t avail_tx_id = avail->index;

    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[avail_tx_id % vdev->queue_size].address);

    vdi_chunk_header_t* chunk_header = (vdi_chunk_header_t*)offset;

    chunk_header->port = VDP_CLIENT_PORT;

    vdagent_message_t* message = (vdagent_message_t*)(offset + sizeof(vdi_chunk_header_t));

    message->protocol = VD_AGENT_PROTOCOL;
    message->type = VD_AGENT_CLIPBOARD;


    vdagent_clipboard_t* clipboard = (vdagent_clipboard_t*)(offset + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));
    clipboard->type = VD_AGENT_CLIPBOARD_UTF8_TEXT;
    clipboard->selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;

    memory_memcopy(text_message, clipboard->data, strlen(text_message));

    message->size = sizeof(vdagent_clipboard_t) + strlen(text_message);

    chunk_header->size = sizeof(vdagent_message_t) + message->size;

    descs[avail_tx_id % vdev->queue_size].length = sizeof(vdi_chunk_header_t) + chunk_header->size;

    avail->index++;
    vq_tx->nd->vqn = 1;

    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Clipboard data sent");

    return 0;
}

int8_t clipboard_send_text(const char_t* text_message) {
    for(uint64_t i = 0; i < list_size(virtio_console_list); i++) {
        const virtio_console_t* vconsole = list_get_data_at_position(virtio_console_list, i);

        if(vconsole == NULL) {
            continue;
        }

        for(uint16_t port_no = 0; port_no < vconsole->port_count; port_no++) {
            if(vconsole->ports[port_no].open && strcmp(vconsole->ports[port_no].name, VIRTIO_CONSOLE_VDAGENT_PORT_NAME) == 0){
                if(vdagent_send_clipboard_data(vconsole, port_no, text_message) != 0) {
                    PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to send clipboard data");
                }
            }
        }
    }

    return 0;
}

static int8_t vdagent_request_clipboard_data(const virtio_console_t* vconsole, uint8_t port_no) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = virtio_console_get_tx_queue_number(port_no);

    virtio_dev_t* vdev = vconsole->vdev;

    virtio_queue_ext_t* vq_tx = &vdev->queues[queue_index];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_tx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_tx->vq);

    uint16_t avail_tx_id = avail->index;

    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[avail_tx_id % vdev->queue_size].address);

    vdi_chunk_header_t* chunk_header = (vdi_chunk_header_t*)offset;

    chunk_header->port = VDP_CLIENT_PORT;

    vdagent_message_t* message = (vdagent_message_t*)(offset + sizeof(vdi_chunk_header_t));

    message->protocol = VD_AGENT_PROTOCOL;
    message->type = VD_AGENT_CLIPBOARD_REQUEST;


    vdagent_clipboard_request_t* clipboard_request = (vdagent_clipboard_request_t*)(offset + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));

    clipboard_request->type = VD_AGENT_CLIPBOARD_UTF8_TEXT;
    clipboard_request->selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;

    message->size = sizeof(vdagent_clipboard_request_t);

    chunk_header->size = sizeof(vdagent_message_t) + message->size;

    descs[avail_tx_id % vdev->queue_size].length = sizeof(vdi_chunk_header_t) + chunk_header->size;

    avail->index++;
    vq_tx->nd->vqn = 1;

    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Request clipboard data sent");

    return 0;
}

static int8_t vdagent_send_grab_clipboard(const virtio_console_t* vconsole, uint8_t port_no) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = virtio_console_get_tx_queue_number(port_no);

    virtio_dev_t* vdev = vconsole->vdev;

    virtio_queue_ext_t* vq_tx = &vdev->queues[queue_index];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_tx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_tx->vq);

    uint16_t avail_tx_id = avail->index;

    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[avail_tx_id % vdev->queue_size].address);

    vdi_chunk_header_t* chunk_header = (vdi_chunk_header_t*)offset;

    chunk_header->port = VDP_CLIENT_PORT;

    vdagent_message_t* message = (vdagent_message_t*)(offset + sizeof(vdi_chunk_header_t));

    message->protocol = VD_AGENT_PROTOCOL;
    message->type = VD_AGENT_CLIPBOARD_GRAB;


    vdagent_clipboard_grab_t* clipboard_grab = (vdagent_clipboard_grab_t*)(offset + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));

    clipboard_grab->selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;
    clipboard_grab->types[0] = VD_AGENT_CLIPBOARD_UTF8_TEXT;

    message->size = sizeof(vdagent_clipboard_grab_t) + sizeof(uint32_t);

    chunk_header->size = sizeof(vdagent_message_t) + message->size;

    descs[avail_tx_id % vdev->queue_size].length = sizeof(vdi_chunk_header_t) + chunk_header->size;

    avail->index++;
    vq_tx->nd->vqn = 1;

    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Grab clipboard sent");

    return 0;
}

static int8_t vdagent_check_message(vdagent_message_t* message, const virtio_console_t* vconsole, uint8_t port_no) {
    if(message->protocol != VD_AGENT_PROTOCOL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid protocol");
        return -1;
    }

    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Message type: %d", message->type);

    if(message->type == VD_AGENT_ANNOUNCE_CAPABILITIES) {
        vdagent_cap_announce_t* cap_announce = (vdagent_cap_announce_t*)message->data;

        if(message->size != sizeof(vdagent_cap_announce_t)) {
            return -1;
        }

        int8_t checks = 0;

        if(cap_announce->caps & (1 << VD_AGENT_CAP_CLIPBOARD_BY_DEMAND)) {
            checks++;
        }

        if(cap_announce->caps & (1 << VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
            checks++;
        }

        if(cap_announce->caps & (1 << VD_AGENT_CAP_CLIPBOARD_GRAB_SERIAL)) {
            checks++;
        }

        if(checks < 3) {
            return -1;
        }

        if(vdagent_send_grab_clipboard(vconsole, port_no) != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to send grab clipboard");
        }
    } else if(message->type == VD_AGENT_CLIPBOARD_REQUEST) {
        vdagent_clipboard_request_t* clipboard_request = (vdagent_clipboard_request_t*)message->data;

        if(message->size != sizeof(vdagent_clipboard_request_t)) {
            return -1;
        }

        if(clipboard_request->type != VD_AGENT_CLIPBOARD_UTF8_TEXT) {
            return -1;
        }

        if(clipboard_request->selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
            return -1;
        }

        if(vdagent_send_clipboard_data(vconsole, port_no, "Clipboard data") != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to send clipboard data");
        }
    } else if(message->type == VD_AGENT_CLIPBOARD) {
        vdagent_clipboard_t* clipboard = (vdagent_clipboard_t*)message->data;

        if(clipboard->type != VD_AGENT_CLIPBOARD_UTF8_TEXT) {
            return -1;
        }

        if(clipboard->selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
            return -1;
        }

        char_t buf[1025] = {0};

        uint64_t buf_len = message->size - sizeof(vdagent_clipboard_t);

        if(buf_len > 1024) {
            buf_len = 1024;
        }

        memory_memcopy(clipboard->data, buf, buf_len);

        buf[buf_len] = '\0';

        PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Clipboard data: %s", buf);
    } else if(message->type == VD_AGENT_CLIPBOARD_GRAB) {
        vdagent_clipboard_grab_t* clipboard_grab = (vdagent_clipboard_grab_t*)message->data;

        if(message->size != sizeof(vdagent_clipboard_grab_t) + sizeof(uint32_t)) {
            return -1;
        }

        if(clipboard_grab->selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
            return -1;
        }

        if(clipboard_grab->types[0] != VD_AGENT_CLIPBOARD_UTF8_TEXT) {
            return -1;
        }

        if(vdagent_request_clipboard_data(vconsole, port_no) != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to request clipboard data");
        }
    } else {
        PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Unknown message");
        return -1;
    }


    return 0;
}


static inline void virtio_console_vdagent_advance_queue(virtio_dev_t* vdev, virtio_queue_ext_t* vq_rx, virtio_queue_avail_t* avail, uint16_t packet_desc_id) {
    avail->ring[avail->index % vdev->queue_size] = packet_desc_id;
    avail->index++;
    vq_rx->nd->vqn = 0;

    vq_rx->last_used_index++;
}

static int8_t virtio_console_vdagent_loop(const virtio_console_t * vconsole, uint64_t port_no) {
    virtio_dev_t* vdev = vconsole->vdev;

    uint16_t queue_index = virtio_console_get_rx_queue_number(port_no);

    virtio_queue_ext_t* vq_rx = &vdev->queues[queue_index];

    virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_rx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_rx->vq);

    buffer_t* message_buffer = NULL;
    uint64_t message_data_remaining = 0;

    while(vq_rx->last_used_index < used->index) {

        uint64_t packet_len = used->ring[vq_rx->last_used_index % vdev->queue_size].length;
        uint16_t packet_desc_id = used->ring[vq_rx->last_used_index % vdev->queue_size].id;

        uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[packet_desc_id].address);

        vdi_chunk_header_t* chunk_header = (vdi_chunk_header_t*)offset;

        if(chunk_header->port != VDP_CLIENT_PORT) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid port");
            virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
            continue;
        }

        if(chunk_header->size + sizeof(vdi_chunk_header_t) > packet_len) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid chunk size");
            virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
            continue;
        }

        if(message_buffer == NULL) {
            vdagent_message_t* message = (vdagent_message_t*)(offset + sizeof(vdi_chunk_header_t));
            message_data_remaining = 0;

            if(message->protocol != VD_AGENT_PROTOCOL) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid protocol");
                virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
                continue;
            }

            message_buffer = buffer_create_with_heap(NULL, sizeof(vdagent_message_t) + message->size + 1); // add one to prevent resizing when all data is appended

            if(message_buffer == NULL) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Cannot allocate buffer");
                virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
                continue;
            }

            buffer_append_bytes(message_buffer, offset + sizeof(vdi_chunk_header_t), sizeof(vdagent_message_t));

            uint32_t remaining = chunk_header->size - sizeof(vdagent_message_t);

            buffer_append_bytes(message_buffer, offset + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t), remaining);

            if(message->size > remaining) {
                message_data_remaining = message->size - remaining;
                PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Message fragmented. Remaining: %lli", message_data_remaining);
                virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
                continue;
            }
        } else {
            uint32_t remaining = chunk_header->size;

            buffer_append_bytes(message_buffer, offset + sizeof(vdi_chunk_header_t), remaining);

            message_data_remaining -= remaining;

            if(message_data_remaining) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Message fragmented. Remaining: %lli", message_data_remaining);
                virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
                continue;
            }
        }

        if(message_data_remaining) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid message remaining data not zero: %lli", message_data_remaining);
            buffer_destroy(message_buffer);
            message_buffer = NULL;
            virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
            continue;
        }

        vdagent_message_t* message = (vdagent_message_t*)buffer_get_all_bytes_and_destroy(message_buffer, &message_data_remaining);
        message_buffer = NULL;

        if(message == NULL) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid message. Cannot get message from buffer");
            virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
            continue;
        }

        if(message->size + sizeof(vdagent_message_t) != message_data_remaining) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid message message size %d != collected %lli", message->size, message_data_remaining);
            virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
            continue;
        }

        if(vdagent_check_message(message, vconsole, port_no) != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Invalid message");
        } else {
            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Valid message");
        }

        virtio_console_vdagent_advance_queue(vdev, vq_rx, avail, packet_desc_id);
    }

    return 0;
}

static int8_t virtio_console_vdagent_rx(uint64_t args_cnt, void** args) {
    if(args_cnt != 2) {
        return -1;
    }

    virtio_console_t* vconsole = (virtio_console_t*)args[0];
    uint64_t port_no = (uint64_t)args[1];

    uint16_t queue_index = virtio_console_get_rx_queue_number(port_no);


    virtio_dev_t* vdev = vconsole->vdev;
    pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)vdev->pci_dev->pci_header;
    pci_capability_msix_t* msix = vdev->msix_cap;

    PRINTLOG(VIRTIO_CONSOLE, LOG_TRACE, "clear pending bit send set interruptible");
    cpu_cli();
    pci_msix_update_lapic(pci_gen_dev, msix, queue_index);
    pci_msix_clear_pending_bit(pci_gen_dev, msix, queue_index);
    task_set_interruptible();
    cpu_sti();

    virtio_queue_ext_t* vq_rx = &vdev->queues[queue_index];

    virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);

    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Starting vdagent");

    while(true) {
        if(!vconsole->ports[port_no].open) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %lli closed. Ending vdagent", port_no);
            break;
        }

        if(vq_rx->last_used_index < used->index) {
            virtio_console_vdagent_loop(vconsole, port_no);
        }

        pci_msix_clear_pending_bit((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, queue_index);

        if(!vconsole->ports[port_no].open) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %lli closed. Ending vdagent", port_no);
            break;
        }

        task_set_message_waiting();
        task_yield();
    }

    return 0;
}

static int8_t virtio_console_isr_data_receive(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    for(uint64_t i = 0; i < list_size(virtio_console_list); i++) {
        const virtio_console_t* vconsole = list_get_data_at_position(virtio_console_list, i);

        if(vconsole == NULL) {
            continue;
        }

        virtio_dev_t* vdev = vconsole->vdev;
        pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)vdev->pci_dev->pci_header;
        pci_capability_msix_t* msix = vdev->msix_cap;

        for(uint16_t port_no = 0; port_no < vconsole->port_count; port_no++) {
            uint16_t queue_index = virtio_console_get_rx_queue_number(port_no);

            virtio_queue_ext_t* vq_rx = &vdev->queues[queue_index];

            if(vq_rx->isr_number != frame->interrupt_number - INTERRUPT_IRQ_BASE) {
                continue;
            }


            if(vconsole->rx_task_id) {
                task_set_interrupt_received(vconsole->ports[port_no].rx_task_id);
            } else {
                pci_msix_clear_pending_bit(pci_gen_dev, msix, queue_index);
            }
        }
    }

    apic_eoi();

    return 0;
}

/*
   static int8_t virtio_console_isr_data_send(interrupt_frame_ext_t* frame) {
    UNUSED(frame);
    video_text_print("virtio_console_isr_data_send\n");
    for(uint64_t i = 0; i < list_size(virtio_console_list); i++) {
        const virtio_console_t* vconsole = list_get_data_at_position(virtio_console_list, i);

        if(vconsole == NULL) {
            video_text_print("vconsole is NULL\n");
            continue;
        }

        virtio_dev_t* vdev = vconsole->vdev;
        pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)vdev->pci_dev->pci_header;
        pci_capability_msix_t* msix = vdev->msix_cap;

        virtio_queue_ext_t* vq_rx = &vdev->queues[0];

        if(vq_rx->isr_number == frame->interrupt_number - INTERRUPT_IRQ_BASE) {
            pci_msix_clear_pending_bit(pci_gen_dev, msix, 0);
        }

        int32_t queue_index = 3;

        for(uint8_t j = 1; j < vconsole->port_count; i++) {
            queue_index += 2;

            vq_rx = &vdev->queues[queue_index];

            if(vq_rx->isr_number == frame->interrupt_number - INTERRUPT_IRQ_BASE) {
                pci_msix_clear_pending_bit(pci_gen_dev, msix, queue_index);
            }

        }
    }

    apic_eoi();

    return 0;
   }
 */


static int8_t virtio_console_send_control_message(const virtio_console_t* console, virtio_console_control_t* control) {
    virtio_dev_t* vdev = console->vdev;
    virtio_queue_ext_t* vq_tx = &vdev->queues[3];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_tx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_tx->vq);

    uint16_t avail_tx_id = avail->index;

    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[avail_tx_id % vdev->queue_size].address);

    memory_memcopy(control, offset, sizeof(virtio_console_control_t));

    descs[avail_tx_id % vdev->queue_size].length = sizeof(virtio_console_control_t);

    avail->index++;
    vq_tx->nd->vqn = 1;

    return 0;
}

static int8_t virtio_console_send_device_ready(const virtio_console_t* console) {
    virtio_console_control_t control = {0};

    control.event = VIRTIO_CONSOLE_DEVICE_READY;
    control.value = 1;

    return virtio_console_send_control_message(console, &control);
}

static int8_t virtio_console_send_port_ready(const virtio_console_t* console, uint8_t port) {
    virtio_console_control_t control = {0};

    control.event = VIRTIO_CONSOLE_PORT_READY;
    control.id = port;
    control.value = 1;

    return virtio_console_send_control_message(console, &control);
}

static int8_t virtio_console_send_port_open(const virtio_console_t* console, uint8_t port) {
    virtio_console_control_t control = {0};

    control.event = VIRTIO_CONSOLE_PORT_OPEN;
    control.id = port;
    control.value = 1;

    if(virtio_console_send_control_message(console, &control) != 0) {
        return -1;
    }

    return vdagent_send_caps(console, port);
}

static int8_t virtio_console_isr_control_receive(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    for(uint64_t i = 0; i < list_size(virtio_console_list); i++) {
        const virtio_console_t* vconsole = list_get_data_at_position(virtio_console_list, i);

        if(vconsole == NULL) {
            video_text_print("vconsole is NULL\n");
            continue;
        }

        virtio_dev_t* vdev = vconsole->vdev;
        pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)vdev->pci_dev->pci_header;
        pci_capability_msix_t* msix = vdev->msix_cap;

        virtio_queue_ext_t* vq_rx = &vdev->queues[2];

        if(vq_rx->isr_number != frame->interrupt_number - INTERRUPT_IRQ_BASE) {
            continue;
        }

        if(vconsole->rx_task_id) {
            task_set_interrupt_received(vconsole->rx_task_id);
        } else {
            pci_msix_clear_pending_bit(pci_gen_dev, msix, 2);
        }
    }

    apic_eoi();

    return 0;
}

/*
   static int8_t virtio_console_isr_control_send(interrupt_frame_ext_t* frame) {
    UNUSED(frame);
    video_text_print("virtio_console_isr_control_send\n");

    apic_eoi();

    return 0;
   }
 */

static int8_t virtio_console_start_vdagent(const virtio_console_t* vconsole, uint8_t port_no) {
    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Starting vdagent");

    if(port_no >= vconsole->port_count) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Invalid port number");
        return -1;
    }

    memory_heap_t* heap = memory_get_default_heap();

    void** rx_args = memory_malloc_ext(heap, sizeof(void*) * 2, 0);

    if(rx_args == NULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "cannot allocate memory for rx task args");

        return -1;
    }

    rx_args[0] = (void*)vconsole;
    rx_args[1] = (void*)((uint64_t)port_no);

    uint64_t rx_task_id = task_create_task(heap, 16 << 20, 64 << 10, virtio_console_vdagent_rx, 2, rx_args, "vdagent");
    vconsole->ports[port_no].rx_task_id = rx_task_id;

    return 0;
}

static void virtio_console_control_queue_loop(const virtio_console_t* vconsole) {
    virtio_dev_t* vdev = vconsole->vdev;

    virtio_queue_ext_t* vq_rx = &vdev->queues[2];

    virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_rx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_rx->vq);

    while(vq_rx->last_used_index < used->index) {

        uint64_t packet_len = used->ring[vq_rx->last_used_index % vdev->queue_size].length;
        uint16_t packet_desc_id = used->ring[vq_rx->last_used_index % vdev->queue_size].id;

        uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[packet_desc_id].address);

        virtio_console_control_t* control = (virtio_console_control_t*)offset;

        switch(control->event) {
        case VIRTIO_CONSOLE_DEVICE_ADD: {
            vconsole->ports[control->id].attached = true;
            virtio_console_send_port_ready(vconsole, control->id);
            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %d ready", control->id);
        }
        break;
        case VIRTIO_CONSOLE_DEVICE_REMOVE: {
            vconsole->ports[control->id].attached = false;
            vconsole->ports[control->id].open = false;

            if(vconsole->ports[control->id].rx_task_id) {
                // port console should stop task, however still it is running, we should kill it forcefully
                task_kill_task(vconsole->ports[control->id].rx_task_id, true);
                vconsole->ports[control->id].rx_task_id = 0;
            }

            memory_free(vconsole->ports[control->id].name);
            vconsole->ports[control->id].name = NULL;

            memory_memclean(&vconsole->ports[control->id], sizeof(virtio_console_port_t)); // ensure that all fields are zero

            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %d removed", control->id);
        }
        break;
        case VIRTIO_CONSOLE_CONSOLE_PORT:
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Virtio console port not implemented");
            break;
        case VIRTIO_CONSOLE_RESIZE:
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Virtio console resize not implemented");
            break;
        case VIRTIO_CONSOLE_PORT_OPEN: {
            if(control->value) {
                if(strcmp(vconsole->ports[control->id].name, VIRTIO_CONSOLE_VDAGENT_PORT_NAME) == 0) {
                    vconsole->ports[control->id].open = true;

                    if(virtio_console_start_vdagent(vconsole, control->id) != 0) {
                        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to start vdagent");
                        vconsole->ports[control->id].open = false;
                    } else {
                        virtio_console_send_port_open(vconsole, control->id);
                        PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %d open", control->id);
                    }
                } else {
                    PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Unknown port %d", control->id);
                }
            } else {
                vconsole->ports[control->id].open = false;
                PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %d closed", control->id);
                if(vconsole->ports[control->id].rx_task_id) {
                    task_set_interrupt_received(vconsole->ports[control->id].rx_task_id);
                    vconsole->ports[control->id].rx_task_id = 0;
                }
            }
        }
        break;
        case VIRTIO_CONSOLE_PORT_NAME: {
            uint8_t* name_offset = offset + sizeof(virtio_console_control_t);
            uint64_t name_len = packet_len - sizeof(virtio_console_control_t);

            if(name_len > VIRTIO_CONSOLE_MAX_PORT_NAME) {
                name_len = VIRTIO_CONSOLE_MAX_PORT_NAME;
            }

            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %d name len %lli", control->id, name_len);

            vconsole->ports[control->id].name = memory_malloc(name_len + 1);

            if(vconsole->ports[control->id].name == NULL) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to allocate memory for port name");
                break;
            }

            memory_memcopy(name_offset, vconsole->ports[control->id].name, name_len);

            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Port %d name %s", control->id, vconsole->ports[control->id].name);
        }
        break;
        default: {
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Unknown event %d port %d", control->event, control->id);
        }
        break;
        }

        avail->ring[avail->index % vdev->queue_size] = packet_desc_id;
        avail->index++;
        vq_rx->nd->vqn = 0;

        vq_rx->last_used_index++;
    }

}

static int32_t virtio_console_control_rx(uint64_t args_cnt, void** args) {
    if(args_cnt != 1) {
        return -1;
    }

    virtio_console_t* vconsole = (virtio_console_t*)args[0];

    if(vconsole == NULL) {
        return -1;
    }

    if(virtio_console_send_device_ready(vconsole) != 0) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to send device ready");
        return -1;
    }

    virtio_dev_t* vdev = vconsole->vdev;
    pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)vdev->pci_dev->pci_header;
    pci_capability_msix_t* msix = vdev->msix_cap;

    uint16_t queue_index = 2;

    PRINTLOG(VIRTIO_CONSOLE, LOG_TRACE, "clear pending bit send set interruptible");
    cpu_cli();
    pci_msix_update_lapic(pci_gen_dev, msix, queue_index);
    pci_msix_clear_pending_bit(pci_gen_dev, msix, queue_index);
    task_set_interruptible();
    cpu_sti();

    virtio_queue_ext_t* vq_rx = &vdev->queues[2];

    virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);

    while(true) {
        if(vq_rx->last_used_index < used->index) {
            virtio_console_control_queue_loop(vconsole);
        }

        pci_msix_clear_pending_bit((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, queue_index);

        task_set_message_waiting();
        task_yield();
    }


    return 0;
}

static uint64_t virtio_console_select_features(virtio_dev_t* vdev, uint64_t avail_features) {
    uint64_t selected_features = 0;

    virtio_console_config_t* cc = (virtio_console_config_t*)vdev->device_config;

    if(avail_features & VIRTIO_CONSOLE_F_SIZE) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "Virtio console supports resizable console");
        PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "Virtio console cols: %d rows: %d", cc->cols, cc->rows);
        selected_features |= VIRTIO_CONSOLE_F_SIZE;
    }

    if(avail_features & VIRTIO_CONSOLE_F_MULTIPORT) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "Virtio console supports multiple ports");
        PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "Virtio console max ports: %d", cc->max_nr_ports);
        selected_features |= VIRTIO_CONSOLE_F_MULTIPORT;
    }

    if(avail_features & VIRTIO_CONSOLE_F_EMERG_WRITE) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "Virtio console supports emergency write");
        selected_features |= VIRTIO_CONSOLE_F_EMERG_WRITE;
    }

    PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "max queues: %d", vdev->common_config->num_queues);
    vdev->max_vq_count = vdev->common_config->num_queues;

    return selected_features;
}

static int8_t virtio_console_create_queues(virtio_dev_t* vdev) {
    uint64_t nr_ports = 1;

    virtio_console_config_t* cc = (virtio_console_config_t*)vdev->device_config;

    boolean_t has_multiport = false;

    if(vdev->selected_features & VIRTIO_CONSOLE_F_MULTIPORT) {
        nr_ports = cc->max_nr_ports;
        has_multiport = true;
    }

    vdev->queues = memory_malloc(sizeof(virtio_queue_ext_t) * nr_ports);

    if(vdev->queues == NULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to allocate memory for virtio queues");
        return -1;
    }

    PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Creating queues for port 0");

    if(virtio_create_queue(vdev, 0, VIRTIO_CONSOLE_DATA_VQ_SIZE, true, false, NULL, virtio_console_isr_data_receive, NULL) != 0) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
        return -1;
    }

    if(virtio_create_queue(vdev, 1, VIRTIO_CONSOLE_DATA_VQ_SIZE, false, false, NULL, NULL, NULL) != 0) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
        return -1;
    }

    if(has_multiport) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Creating queues for control");
        if(virtio_create_queue(vdev, 2, VIRTIO_CONSOLE_CONTROL_VQ_SIZE, true, false, NULL, virtio_console_isr_control_receive, NULL) != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
            return -1;
        }

        if(virtio_create_queue(vdev, 3, VIRTIO_CONSOLE_CONTROL_VQ_SIZE, false, false, NULL, NULL, NULL) != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
            return -1;
        }

        for(uint64_t i = 1; i < nr_ports; i++) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Creating queues for port %lli", i);
            if(virtio_create_queue(vdev, 2 * i + 2, VIRTIO_CONSOLE_DATA_VQ_SIZE, true, false, NULL, virtio_console_isr_data_receive, NULL) != 0) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
                return -1;
            }

            if(virtio_create_queue(vdev, 2 * i + 3, VIRTIO_CONSOLE_DATA_VQ_SIZE, false, false, NULL, NULL, NULL) != 0) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
                return -1;
            }
        }
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t virtio_console_init_internal(const pci_dev_t* pci_dev) {
    if(virtio_console_list == NULL) {
        virtio_console_list = list_create_list();
    }

    virtio_dev_t* vdev = virtio_get_device(pci_dev);

    if(vdev == NULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to get virtio device");
        return -1;
    }

    vdev->queue_size = VIRTIO_QUEUE_SIZE;

    virtio_console_t* vconsole = memory_malloc(sizeof(virtio_console_t));

    if(vconsole == NULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to allocate memory for virtio console");
        return -1;
    }

    vconsole->vdev = vdev;

    if(list_insert_at_head(virtio_console_list, vconsole) == -1ULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to insert virtio console to list");
        return -1;
    }

    if(virtio_init_dev(vdev, virtio_console_select_features, virtio_console_create_queues) != 0) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to initialize virtio device");
        return -1;
    }

    virtio_console_config_t* cc = (virtio_console_config_t*)vdev->device_config;

    uint64_t nr_ports = 1;

    if(vdev->selected_features & VIRTIO_CONSOLE_F_MULTIPORT) {
        nr_ports = cc->max_nr_ports;
    }

    vconsole->port_count = nr_ports;
    vconsole->ports = memory_malloc(sizeof(virtio_console_port_t) * nr_ports);

    if(vconsole->ports == NULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to allocate memory for virtio console ports");
        return -1;
    }

    void** rx_args = memory_malloc(sizeof(void*) * 1);

    if(rx_args == NULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "cannot allocate memory for rx task args");

        return -1;
    }

    rx_args[0] = (void*)vconsole;

    uint64_t rx_task_id = task_create_task(NULL, 2 << 20, 64 << 10, virtio_console_control_rx, 1, rx_args, "vconsole control");
    vconsole->rx_task_id = rx_task_id;

    PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "Virtio console initialized");

    return 0;
}
#pragma GCC diagnostic pop

int8_t console_virtio_init(void) {
    iterator_t* iter = list_iterator_create(pci_get_context()->other_devices);

    if (iter == NULL) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create iterator");
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* pci_dev = iter->get_item(iter);

        pci_common_header_t* pci_header = pci_dev->pci_header;

        if(pci_header->vendor_id == VIRTIO_CONSOLE_VENDOR_ID && pci_header->device_id == VIRTIO_CONSOLE_DEVICE_ID) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_INFO, "Virtio console found");
            if(virtio_console_init_internal(pci_dev) != 0) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to initialize virtio console");
                return -1;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);


    return 0;
}
