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

MODULE ("turnstone.kernel.hw.console.virtio");

void video_text_print(const char_t* text);

list_t* virtio_console_list = NULL;

typedef struct virtio_console_port_t {
    uint8_t   id;
    boolean_t attached;
    boolean_t open;
    uint8_t   name[64];
} virtio_console_port_t;

typedef struct virtio_console_t {
    virtio_dev_t*          vdev;
    uint8_t                port_count;
    virtio_console_port_t* ports;
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
        PRINTLOG(VIRTIO, LOG_INFO, "Virtio console supports resizable console");
        PRINTLOG(VIRTIO, LOG_INFO, "Virtio console cols: %d rows: %d", cc->cols, cc->rows);
        selected_features |= VIRTIO_CONSOLE_F_SIZE;
    }

    if(avail_features & VIRTIO_CONSOLE_F_MULTIPORT) {
        PRINTLOG(VIRTIO, LOG_INFO, "Virtio console supports multiple ports");
        PRINTLOG(VIRTIO, LOG_INFO, "Virtio console max ports: %d", cc->max_nr_ports);
        selected_features |= VIRTIO_CONSOLE_F_MULTIPORT;
    }

    if(avail_features & VIRTIO_CONSOLE_F_EMERG_WRITE) {
        PRINTLOG(VIRTIO, LOG_INFO, "Virtio console supports emergency write");
        selected_features |= VIRTIO_CONSOLE_F_EMERG_WRITE;
    }

    PRINTLOG(VIRTIO, LOG_INFO, "max queues: %d", vdev->common_config->num_queues);
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

        virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);
        virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_rx->vq);
        virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_rx->vq);

        while(vq_rx->last_used_index < used->index) {

            uint64_t packet_len = used->ring[vq_rx->last_used_index % vdev->queue_size].length;
            uint16_t packet_desc_id = used->ring[vq_rx->last_used_index % vdev->queue_size].id;

            uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[packet_desc_id].address);

            virtio_console_control_t* control = (virtio_console_control_t*)offset;

            char_t buf[1025] = {0};

            switch(control->event) {
            case VIRTIO_CONSOLE_DEVICE_ADD: {
                if(control->id == 1) {
                    vconsole->ports[1].attached = true;
                    virtio_console_send_port_ready(vconsole, 1);
                } else {
                    video_text_print("only one port supported\n");
                }
            }
            break;
            case VIRTIO_CONSOLE_DEVICE_REMOVE:
                video_text_print("VIRTIO_CONSOLE_DEVICE_REMOVE\n");
                break;
            case VIRTIO_CONSOLE_CONSOLE_PORT:
                video_text_print("VIRTIO_CONSOLE_CONSOLE_PORT\n");
                break;
            case VIRTIO_CONSOLE_RESIZE:
                video_text_print("VIRTIO_CONSOLE_RESIZE\n");
                break;
            case VIRTIO_CONSOLE_PORT_OPEN: {
                if(control->id == 1) {
                    vconsole->ports[1].open = true;
                    virtio_console_send_port_open(vconsole, 1);
                } else {
                    video_text_print("only one port supported\n");
                }
            }
            break;
            case VIRTIO_CONSOLE_PORT_NAME: {
                uint8_t* name_offset = offset + sizeof(virtio_console_control_t);
                uint64_t name_len = packet_len - sizeof(virtio_console_control_t);

                for(uint64_t j = 0; j < name_len; j++) {
                    if(control->id == 1) {
                        vconsole->ports[1].name[j] = name_offset[j];
                    }
                }

                if(control->id == 1) {
                    vconsole->ports[1].name[name_len] = '\0';
                }
            }

            break;
            default: {
                buf[0] = control->id + '0';
                buf[1] = '\0';
                video_text_print("Port id: ");
                video_text_print(buf);
                video_text_print("\n");
                video_text_print("Unknown event\n");
                buf[0] = control->event + '0';
                buf[1] = '\0';
                video_text_print(buf);
                video_text_print("\n");
            }
            break;
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
   static int8_t virtio_console_isr_control_send(interrupt_frame_ext_t* frame) {
    UNUSED(frame);
    video_text_print("virtio_console_isr_control_send\n");

    apic_eoi();

    return 0;
   }
 */

static int8_t virtio_console_create_queues(virtio_dev_t* vdev) {
    uint64_t nr_ports = 2;

    virtio_console_config_t* cc = (virtio_console_config_t*)vdev->device_config;

    if(vdev->selected_features & VIRTIO_CONSOLE_F_MULTIPORT) {
        nr_ports = cc->max_nr_ports;
    }

    vdev->queues = memory_malloc(sizeof(virtio_queue_ext_t) * nr_ports);

    if(vdev->queues == NULL) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to allocate memory for virtio queues");
        return -1;
    }

    if(virtio_create_queue(vdev, 0, 4096, true, false, NULL, virtio_console_isr_data_receive, NULL) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to create virtio queues");
        return -1;
    }

    if(virtio_create_queue(vdev, 1, 4096, false, false, NULL, NULL, NULL) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to create virtio queues");
        return -1;
    }

    if(nr_ports > 2) {
        if(virtio_create_queue(vdev, 2, 4096, true, false, NULL, virtio_console_isr_control_receive, NULL) != 0) {
            PRINTLOG(VIRTIO, LOG_ERROR, "Failed to create virtio queues");
            return -1;
        }

        if(virtio_create_queue(vdev, 3, 4096, false, false, NULL, NULL, NULL) != 0) {
            PRINTLOG(VIRTIO, LOG_ERROR, "Failed to create virtio queues");
            return -1;
        }

        // TODO: clipboard at port 1 fix to all ports
        if(virtio_create_queue(vdev, 4, 4096, true, false, NULL, virtio_console_isr_data_receive, NULL) != 0) {
            PRINTLOG(VIRTIO, LOG_ERROR, "Failed to create virtio queues");
            return -1;
        }

        if(virtio_create_queue(vdev, 5, 4096, false, false, NULL, NULL, NULL) != 0) {
            PRINTLOG(VIRTIO, LOG_ERROR, "Failed to create virtio queues");
            return -1;
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

    virtio_dev_t* vc = virtio_get_device(pci_dev);

    if(vc == NULL) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to get virtio device");
        return -1;
    }

    vc->queue_size = VIRTIO_QUEUE_SIZE;

    virtio_console_t* vconsole = memory_malloc(sizeof(virtio_console_t));

    if(vconsole == NULL) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to allocate memory for virtio console");
        return -1;
    }

    vconsole->vdev = vc;
    vconsole->port_count = 2;
    vconsole->ports = memory_malloc(sizeof(virtio_console_port_t) * vconsole->port_count);

    if(vconsole->ports == NULL) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to allocate memory for virtio console ports");
        memory_free(vconsole);
        return -1;
    }

    if(list_insert_at_head(virtio_console_list, vconsole) == -1ULL) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to insert virtio console to list");
        return -1;
    }

    if(virtio_init_dev(vc, virtio_console_select_features, virtio_console_create_queues) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to initialize virtio device");
        return -1;
    }

    if(virtio_console_send_device_ready(vconsole) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to send device ready");
        return -1;
    }

    PRINTLOG(VIRTIO, LOG_INFO, "Virtio console initialized");

    return 0;
}
#pragma GCC diagnostic pop

int8_t console_virtio_init(void) {
    iterator_t* iter = list_iterator_create(pci_get_context()->other_devices);

    if (iter == NULL) {
        PRINTLOG(VIRTIO, LOG_ERROR, "Failed to create iterator");
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* pci_dev = iter->get_item(iter);

        pci_common_header_t* pci_header = pci_dev->pci_header;

        if(pci_header->vendor_id == VIRTIO_CONSOLE_VENDOR_ID && pci_header->device_id == VIRTIO_CONSOLE_DEVICE_ID) {
            PRINTLOG(VIRTIO, LOG_INFO, "Virtio console found");
            if(virtio_console_init_internal(pci_dev) != 0) {
                PRINTLOG(VIRTIO, LOG_ERROR, "Failed to initialize virtio console");
                return -1;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);


    return 0;
}
