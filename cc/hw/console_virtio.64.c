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

static int8_t vdagent_send_caps(const virtio_console_t* vconsole, uint8_t port_no) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = 2 * port_no + 3;

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

    return 0;
}

static int8_t vdagent_send_clipboard_data(const virtio_console_t* vconsole, uint8_t port_no, const char_t* text_message) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = 2 * port_no + 3;

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

    return 0;
}

int8_t clipboard_send_text(const char_t* text_message) {
    for(uint64_t i = 0; i < list_size(virtio_console_list); i++) {
        const virtio_console_t* vconsole = list_get_data_at_position(virtio_console_list, i);

        if(vconsole == NULL) {
            continue;
        }

        if(vdagent_send_clipboard_data(vconsole, 1, text_message) != 0) {
            video_text_print("Failed to send clipboard data\n");
        }
    }

    return 0;
}

static int8_t vdagent_request_clipboard_data(const virtio_console_t* vconsole, uint8_t port_no) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = 2 * port_no + 3;

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

    return 0;
}

static int8_t vdagent_send_grab_clipboard(const virtio_console_t* vconsole, uint8_t port_no) {
    if(port_no != 1) {
        return -1;
    }

    int32_t queue_index = 2 * port_no + 3;

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

    return 0;
}

static int8_t vdagent_check_message(uint8_t* data, uint64_t len, const virtio_console_t* vconsole, uint8_t port_no) {
    if(len < sizeof(vdi_chunk_header_t)) {
        return -1;
    }

    vdi_chunk_header_t* chunk_header = (vdi_chunk_header_t*)data;

    if(chunk_header->size + sizeof(vdi_chunk_header_t) > len) {
        return -1;
    }

    vdagent_message_t* message = (vdagent_message_t*)(data + sizeof(vdi_chunk_header_t));

    if(message->protocol != VD_AGENT_PROTOCOL) {
        return -1;
    }

    if(message->size + sizeof(vdagent_message_t) > chunk_header->size) {
        return -1;
    }

    if(message->type == VD_AGENT_ANNOUNCE_CAPABILITIES) {
        vdagent_cap_announce_t* cap_announce = (vdagent_cap_announce_t*)(data + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));

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
            video_text_print("Failed to send grab clipboard\n");
        }
    } else if(message->type == VD_AGENT_CLIPBOARD_REQUEST) {
        vdagent_clipboard_request_t* clipboard_request = (vdagent_clipboard_request_t*)(data + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));

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
            video_text_print("Failed to send clipboard data\n");
        }
    } else if(message->type == VD_AGENT_CLIPBOARD) {
        vdagent_clipboard_t* clipboard = (vdagent_clipboard_t*)(data + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));

        if(clipboard->type != VD_AGENT_CLIPBOARD_UTF8_TEXT) {
            return -1;
        }

        if(clipboard->selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
            return -1;
        }

        char_t buf[1025] = {0};

        memory_memcopy(clipboard->data, buf, message->size - sizeof(vdagent_clipboard_t));

        buf[message->size - sizeof(vdagent_clipboard_t)] = '\0';

        video_text_print(buf);
    } else if(message->type == VD_AGENT_CLIPBOARD_GRAB) {
        vdagent_clipboard_grab_t* clipboard_grab = (vdagent_clipboard_grab_t*)(data + sizeof(vdi_chunk_header_t) + sizeof(vdagent_message_t));

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
            video_text_print("Failed to request clipboard data\n");
        }
    } else {
        video_text_print("Unknown message\n");
        return -1;
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

static int8_t virtio_console_isr_data_receive(interrupt_frame_ext_t* frame) {
    UNUSED(frame);
    video_text_print("virtio_console_isr_data_receive\n");

    for(uint64_t i = 0; i < list_size(virtio_console_list); i++) {
        const virtio_console_t* vconsole = list_get_data_at_position(virtio_console_list, i);

        if(vconsole == NULL) {
            continue;
        }

        virtio_dev_t* vdev = vconsole->vdev;
        pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)vdev->pci_dev->pci_header;
        pci_capability_msix_t* msix = vdev->msix_cap;

        virtio_queue_ext_t* vq_rx = &vdev->queues[4]; // TODO: clipboard at port 1 fix to all ports

        if(vq_rx->isr_number != frame->interrupt_number - INTERRUPT_IRQ_BASE) {
            continue;
        }

        virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);
        virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_rx->vq);
        virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_rx->vq);

        while(vq_rx->last_used_index < used->index) {

            uint64_t packet_len = used->ring[vq_rx->last_used_index % vdev->queue_size].length;
            uint16_t packet_desc_id = used->ring[vq_rx->last_used_index % vdev->queue_size].id;

            uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[packet_desc_id].address);

            if(vdagent_check_message(offset, packet_len, vconsole, 1) != 0) {
                video_text_print("Invalid message\n");
            } else {
                video_text_print("Valid message\n");
            }

            avail->ring[avail->index % vdev->queue_size] = packet_desc_id;
            avail->index++;
            vq_rx->nd->vqn = 0;

            vq_rx->last_used_index++;
        }

        pci_msix_clear_pending_bit(pci_gen_dev, msix, 2);
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
        }
        break;
        case VIRTIO_CONSOLE_DEVICE_REMOVE:
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Virtio console device removed not implemented");
            break;
        case VIRTIO_CONSOLE_CONSOLE_PORT:
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Virtio console port not implemented");
            break;
        case VIRTIO_CONSOLE_RESIZE:
            video_text_print("VIRTIO_CONSOLE_RESIZE\n");
            PRINTLOG(VIRTIO_CONSOLE, LOG_WARNING, "Virtio console resize not implemented");
            break;
        case VIRTIO_CONSOLE_PORT_OPEN: {
            vconsole->ports[control->id].open = true;
            virtio_console_send_port_open(vconsole, control->id);
        }
        break;
        case VIRTIO_CONSOLE_PORT_NAME: {
            uint8_t* name_offset = offset + sizeof(virtio_console_control_t);
            uint64_t name_len = packet_len - sizeof(virtio_console_control_t);

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

    PRINTLOG(VIRTIO_CONSOLE, LOG_TRACE, "clear pending bit send set interruptible");
    cpu_cli();
    pci_msix_update_lapic(pci_gen_dev, msix, 0);
    pci_msix_clear_pending_bit(pci_gen_dev, msix, 0);
    task_set_interruptible();
    cpu_sti();

    virtio_queue_ext_t* vq_rx = &vdev->queues[2];

    virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);

    while(true) {
        if(vq_rx->last_used_index < used->index) {
            virtio_console_control_queue_loop(vconsole);
        }

        pci_msix_clear_pending_bit((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 0);

        task_set_message_waiting();
        task_yield();
    }


    return 0;
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

    if(virtio_create_queue(vdev, 0, 4096, true, false, NULL, virtio_console_isr_data_receive, NULL) != 0) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
        return -1;
    }

    if(virtio_create_queue(vdev, 1, 4096, false, false, NULL, NULL, NULL) != 0) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
        return -1;
    }

    if(has_multiport) {
        PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Creating queues for control");
        if(virtio_create_queue(vdev, 2, 4096, true, false, NULL, virtio_console_isr_control_receive, NULL) != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
            return -1;
        }

        if(virtio_create_queue(vdev, 3, 4096, false, false, NULL, NULL, NULL) != 0) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
            return -1;
        }

        for(uint64_t i = 1; i < nr_ports; i++) {
            PRINTLOG(VIRTIO_CONSOLE, LOG_DEBUG, "Creating queues for port %lli", i);
            if(virtio_create_queue(vdev, 2 * i + 2, 4096, true, false, NULL, virtio_console_isr_data_receive, NULL) != 0) {
                PRINTLOG(VIRTIO_CONSOLE, LOG_ERROR, "Failed to create virtio queues");
                return -1;
            }

            if(virtio_create_queue(vdev, 2 * i + 3, 4096, false, false, NULL, NULL, NULL) != 0) {
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
