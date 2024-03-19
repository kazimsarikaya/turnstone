/**
 * @file network_virtio.64.c
 * @brief Virtio network driver.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/network_virtio.h>
#include <driver/virtio.h>
#include <logging.h>
#include <list.h>
#include <pci.h>
#include <ports.h>
#include <network.h>
#include <network/network_ethernet.h>
#include <network/network_dhcpv4.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <time/timer.h>
#include <cpu/interrupt.h>
#include <cpu.h>
#include <apic.h>
#include <utils.h>
#include <cpu/task.h>

MODULE("turnstone.kernel.hw.network.virtnet");

list_t* virtio_net_devs = NULL;

int8_t   network_virtio_rx_isr(interrupt_frame_ext_t* frame);
int8_t   network_virtio_tx_isr(interrupt_frame_ext_t* frame);
int8_t   network_virtio_ctrl_isr(interrupt_frame_ext_t* frame);
int8_t   network_virtio_config_isr(interrupt_frame_ext_t* frame);
int8_t   network_virtio_combined_isr(interrupt_frame_ext_t* frame);
int8_t   network_virtio_send_packet(network_transmit_packet_t* packet, virtio_dev_t* vdev, virtio_queue_ext_t* vq_tx, virtio_queue_avail_t* avail, virtio_queue_descriptor_t* descs);
int8_t   network_virtio_process_tx(void);
int32_t  network_virtio_process_rx(uint64_t args_cnt, void** args);
int8_t   network_virtio_ctrl_set_mac(virtio_dev_t* vdev);
uint64_t network_virtio_select_features(virtio_dev_t* vdev, uint64_t avail_features);
int8_t   network_rx_tx_queue_item_builder(virtio_dev_t* vdev, void* queue_item);
int8_t   network_virtio_create_queues(virtio_dev_t* vdev);


int8_t network_virtio_send_packet(network_transmit_packet_t* packet, virtio_dev_t* vdev, virtio_queue_ext_t* vq_tx, virtio_queue_avail_t* avail, virtio_queue_descriptor_t* descs) {
    PRINTLOG(VIRTIONET, LOG_TRACE, "network packet will be sended with length 0x%llx", packet->packet_len);

    uint16_t avail_tx_id = avail->index;

    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[avail_tx_id % vdev->queue_size].address);
    virtio_network_header_t* hdr = (virtio_network_header_t*)offset;

    memory_memclean(hdr, sizeof(virtio_network_header_t));

    offset += sizeof(virtio_network_header_t);

    memory_memcopy(packet->packet_data, offset, packet->packet_len);

    descs[avail_tx_id % vdev->queue_size].length = sizeof(virtio_network_header_t) + packet->packet_len;

    memory_free(packet->packet_data);
    memory_free(packet);

    avail->index++;
    vq_tx->nd->vqn = 1;

    return 0;
}

int8_t network_virtio_process_tx(void){

    for(uint64_t dev_idx = 0; dev_idx < list_size(virtio_net_devs); dev_idx++) {
        virtio_dev_t* vdev = (virtio_dev_t*)list_get_data_at_position(virtio_net_devs, dev_idx);
        pci_msix_update_lapic((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 1);

        vdev->return_queue = list_create_queue_with_heap(NULL);
        task_add_message_queue(vdev->return_queue);

        void** args = memory_malloc(sizeof(void*) * 2);

        if(args == NULL) {
            return -1;
        }

        args[0] = vdev->extra_data;
        args[1] = vdev->return_queue;

        task_create_task(NULL, 1 << 20, 64 << 10, &network_dhcpv4_send_discover, 2, args, "dhcp");
    }

    while(1) {
        boolean_t packet_exists = 0;

        for(uint64_t dev_idx = 0; dev_idx < list_size(virtio_net_devs); dev_idx++) {
            virtio_dev_t* vdev = (virtio_dev_t*)list_get_data_at_position(virtio_net_devs, dev_idx);

            if(!vdev->is_legacy) {
                if(vdev->selected_features & VIRTIO_NETWORK_F_STATUS) {
                    uint32_t link_status = ((virtio_network_config_t*)vdev->device_config)->status;
                    PRINTLOG(VIRTIONET, LOG_TRACE, "virtnet device link status %i", link_status);
                }
            }

            virtio_queue_ext_t* vq_tx = &vdev->queues[1];
            virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_tx->vq);
            virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_tx->vq);

            while(list_size(vdev->return_queue)) {
                network_transmit_packet_t* packet = (network_transmit_packet_t*)list_queue_peek(vdev->return_queue);

                if(packet) {
                    packet_exists = 1;
                    network_virtio_send_packet(packet, vdev, vq_tx, avail, descs);

                    list_queue_pop(vdev->return_queue);
                }

                PRINTLOG(VIRTIONET, LOG_TRACE, "tx queue size 0x%llx", list_size(vdev->return_queue));
            }

        }

        if(packet_exists == 0) {
            task_set_message_waiting();
            task_yield();
        }

    }

    return 0;
}

extern uint64_t network_rx_task_id;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int32_t network_virtio_process_rx(uint64_t args_cnt, void** args){
    UNUSED(args_cnt);

    virtio_dev_t* vdev = (virtio_dev_t*)args[0];

    virtio_queue_ext_t* vq_rx = &vdev->queues[0];
    virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq_rx->vq);
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_rx->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_rx->vq);

    PRINTLOG(VIRTIONET, LOG_TRACE, "virtio network rx clear pending bit send set interruptible");
    cpu_cli();
    pci_msix_update_lapic((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 0);
    pci_msix_clear_pending_bit((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 0);
    task_set_interruptible();
    cpu_sti();

    while(true) {
        boolean_t notify_network_rx = true;

        if(network_received_packets != NULL && vdev->return_queue != NULL) {

            while(vq_rx->last_used_index < used->index) {
                PRINTLOG(VIRTIONET, LOG_TRACE, "packet received. last used index %i", vq_rx->last_used_index);

                network_received_packet_t* packet = memory_malloc_ext(list_get_heap(network_received_packets), sizeof(network_received_packet_t), 0);

                if(packet == NULL) {
                    PRINTLOG(VIRTIONET, LOG_ERROR, "failed to allocate packet");
                    notify_network_rx = true;

                    task_yield();

                    continue;
                }

                uint64_t packet_len = used->ring[vq_rx->last_used_index % vdev->queue_size].length;
                uint16_t packet_desc_id = used->ring[vq_rx->last_used_index % vdev->queue_size].id;

                uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[packet_desc_id].address);
                virtio_network_header_t* hdr = (virtio_network_header_t*)offset;

                if(vdev->features & VIRTIO_NETWORK_F_MRG_RXBUF) {
                    hdr->header_length = sizeof(virtio_network_header_t);
                } else {
                    hdr->header_length = sizeof(virtio_network_header_t) - 2;
                }

                offset += hdr->header_length;
                packet_len -= hdr->header_length;

                packet->packet_len = packet_len;
                packet->return_queue = vdev->return_queue;
                packet->network_info = vdev->extra_data;
                packet->network_type = NETWORK_TYPE_ETHERNET;

                packet->packet_data = memory_malloc_ext(list_get_heap(network_received_packets), packet_len, 0);

                if(packet->packet_data == NULL) {
                    PRINTLOG(VIRTIONET, LOG_ERROR, "failed to allocate packet data. packet len 0x%llx", packet_len);
                    memory_free_ext(list_get_heap(network_received_packets), packet);
                    notify_network_rx = true;

                    task_yield();

                    continue;
                }

                memory_memcopy(offset, packet->packet_data, packet_len);

                descs[packet_desc_id].flags = VIRTIO_QUEUE_DESC_F_WRITE;

                avail->ring[avail->index % vdev->queue_size] = packet_desc_id;
                avail->index++;
                vq_rx->nd->vqn = 0;

                uint8_t* mac = vdev->extra_data;

                PRINTLOG(VIRTIONET, LOG_TRACE, "packet received with length 0x%llx", packet_len);
                PRINTLOG(VIRTIONET, LOG_TRACE, "dst mac %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

                if(list_queue_push(network_received_packets, packet) == -1ULL) {
                    PRINTLOG(VIRTIONET, LOG_ERROR, "failed to queue packet");
                    memory_free_ext(list_get_heap(network_received_packets), packet->packet_data);
                    memory_free_ext(list_get_heap(network_received_packets), packet);
                } else {
                    PRINTLOG(VIRTIONET, LOG_TRACE, "packet queued");

                    if(notify_network_rx && network_rx_task_id) {
                        task_set_message_received(network_rx_task_id);
                        PRINTLOG(VIRTIONET, LOG_TRACE, "cleared message waiting for rx task 0x%llx", network_rx_task_id);
                        notify_network_rx = false;
                    }
                }

                vq_rx->last_used_index++;
            }

            pci_msix_clear_pending_bit((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 0);

        }

        task_set_message_waiting();
        task_yield();

    }

    PRINTLOG(VIRTIONET, LOG_ERROR, "exited rx task");

    return 0;
}
#pragma GCC diagnostic pop

int8_t network_virtio_rx_isr(interrupt_frame_ext_t* frame) {
    uint8_t intnum = frame->interrupt_number;

    PRINTLOG(VIRTIONET, LOG_TRACE, "packet received int 0x%02x", intnum);

    for(uint64_t dev_idx = 0; dev_idx < list_size(virtio_net_devs); dev_idx++) {
        virtio_dev_t* vdev = (virtio_dev_t*)list_get_data_at_position(virtio_net_devs, dev_idx);

        if(vdev->rx_task_id) {
            task_set_interrupt_received(vdev->rx_task_id);
            PRINTLOG(VIRTIONET, LOG_TRACE, "cleared message waiting for rx task 0x%llx", vdev->rx_task_id);
        } else {
            pci_msix_clear_pending_bit((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 0);
        }
    }

    apic_eoi();

    return 0;
}

int8_t network_virtio_tx_isr(interrupt_frame_ext_t* frame) {
    uint8_t intnum = frame->interrupt_number;

    PRINTLOG(VIRTIONET, LOG_TRACE, "packet sended int 0x%02x", intnum);

    const virtio_dev_t* vdev = list_get_data_at_position(virtio_net_devs, 0);

    pci_msix_clear_pending_bit((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 1);
    apic_eoi();

    return 0;
}

int8_t network_virtio_ctrl_isr(interrupt_frame_ext_t* frame) {
    uint8_t intnum = frame->interrupt_number;

    PRINTLOG(VIRTIONET, LOG_TRACE, "control int 0x%02x", intnum);

    apic_eoi();

    return 0;
}

int8_t network_virtio_config_isr(interrupt_frame_ext_t* frame) {
    uint8_t intnum = frame->interrupt_number;

    PRINTLOG(VIRTIONET, LOG_TRACE, "config int 0x%02x", intnum);

    return -1;
}

int8_t network_virtio_combined_isr(interrupt_frame_ext_t* frame) {
    uint8_t intnum = frame->interrupt_number;

    PRINTLOG(VIRTIONET, LOG_TRACE, "combined int 0x%02x", intnum);

    return -1;
}

int8_t network_virtio_ctrl_set_mac(virtio_dev_t* vdev){

    virtio_queue_ext_t* vq_ctrl = &vdev->queues[2];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq_ctrl->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq_ctrl->vq);

    uint16_t empty_desc = avail->index;

    uint64_t va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((descs[empty_desc].address));

    uint8_t* offset = (uint8_t*)va;

    virtio_network_control_t* ctrl = (virtio_network_control_t*)(offset);

    ctrl->class = VIRTIO_NETWORK_CTRL_MAC;
    ctrl->command = VIRTIO_NETWORK_CTRL_MAC_ADDR_SET;
    memory_memcopy((network_mac_address_t*)vdev->extra_data, offset + 2, sizeof(network_mac_address_t));


    descs[empty_desc].length = 2 + sizeof(network_mac_address_t);

    descs[empty_desc].flags = VIRTIO_QUEUE_DESC_F_NEXT;
    descs[empty_desc].next = empty_desc + 1;


    descs[empty_desc + 1].length = 1;

    descs[empty_desc + 1].flags = VIRTIO_QUEUE_DESC_F_WRITE;
    descs[empty_desc + 1].next = empty_desc + 1;


    avail->ring[avail->index] = empty_desc;

    avail->index++;

    vq_ctrl->nd->vqn = 2;

    return 0;
}

uint64_t network_virtio_select_features(virtio_dev_t* vdev, uint64_t avail_features){
    uint64_t req_features = 0;

    if(avail_features & VIRTIO_NETWORK_F_MAC) {
        PRINTLOG(VIRTIONET, LOG_TRACE, "device has mac feature");
        req_features |= VIRTIO_NETWORK_F_MAC;

        network_mac_address_t* mac = memory_malloc(sizeof(network_mac_address_t));

        if(vdev->is_legacy) {
            int16_t offset = 0;

            if(vdev->has_msix != 1) {
                offset = -2;
            }

            uint8_t mac_bytes[6];

            mac_bytes[0] = inb(vdev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
            offset++;
            mac_bytes[1] = inb(vdev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
            offset++;
            mac_bytes[2] = inb(vdev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
            offset++;
            mac_bytes[3] = inb(vdev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
            offset++;
            mac_bytes[4] = inb(vdev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
            offset++;
            mac_bytes[5] = inb(vdev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);

            memory_memcopy(mac_bytes, mac, sizeof(network_mac_address_t));
        } else {
            memory_memcopy(((virtio_network_config_t*)vdev->device_config)->mac, mac, sizeof(network_mac_address_t));
        }

        vdev->extra_data = mac;
    }

    if(avail_features & VIRTIO_NETWORK_F_STATUS) {
        PRINTLOG(VIRTIONET, LOG_TRACE, "device has link status feature");
        req_features |= VIRTIO_NETWORK_F_STATUS;
    }

    if(avail_features & VIRTIO_NETWORK_F_MRG_RXBUF) {
        PRINTLOG(VIRTIONET, LOG_TRACE, "device has merge receive buffers feature");
        req_features |= VIRTIO_NETWORK_F_MRG_RXBUF;
    }

    if(avail_features & VIRTIO_NETWORK_F_CTRL_VQ) {
        PRINTLOG(VIRTIONET, LOG_TRACE, "device has control vq feature");
        req_features |= VIRTIO_NETWORK_F_CTRL_VQ;

        if(avail_features & VIRTIO_NETWORK_F_CTRL_RX) {
            PRINTLOG(VIRTIONET, LOG_TRACE, "device has control rx feature");
            req_features |= VIRTIO_NETWORK_F_CTRL_RX;
        }

        if(avail_features & VIRTIO_NETWORK_F_CTRL_VLAN) {
            PRINTLOG(VIRTIONET, LOG_TRACE, "device has control vlan feature");
            req_features |= VIRTIO_NETWORK_F_CTRL_VLAN;
        }

        if(avail_features & VIRTIO_NETWORK_F_GUEST_ANNOUNCE) {
            PRINTLOG(VIRTIONET, LOG_TRACE, "device has control announce feature");
            req_features |= VIRTIO_NETWORK_F_GUEST_ANNOUNCE;
        }

        if(avail_features & VIRTIO_NETWORK_F_MQ) {
            PRINTLOG(VIRTIONET, LOG_TRACE, "device has control max vq count feature");
            req_features |= VIRTIO_NETWORK_F_MQ;

            if(vdev->is_legacy) {
                vdev->max_vq_count = inw(vdev->iobase + VIRTIO_NETWORK_IOPORT_MAX_VQ_COUNT);
            } else {
                vdev->max_vq_count = vdev->common_config->num_queues;
            }

        } else {
            vdev->max_vq_count = 3;
        }

        if(avail_features & VIRTIO_NETWORK_F_CTRL_MAC_ADDR) {
            PRINTLOG(VIRTIONET, LOG_TRACE, "device has control mac feature");
            req_features |= VIRTIO_NETWORK_F_CTRL_MAC_ADDR;
        }

        if(avail_features & VIRTIO_NETWORK_F_NOTF_COALESCE) {
            PRINTLOG(VIRTIONET, LOG_INFO, "device has guest coalesce feature");

            req_features |= VIRTIO_NETWORK_F_NOTF_COALESCE;
        }
    }

    if(avail_features & VIRTIO_NETWORK_F_MTU) {
        PRINTLOG(VIRTIONET, LOG_TRACE, "device has mtu feature");

        req_features |= VIRTIO_NETWORK_F_MTU;

    }

    if(avail_features & VIRTIO_F_NOTIFICATION_DATA) {
        PRINTLOG(VIRTIONET, LOG_TRACE, "device has notification data feature");

        req_features |= VIRTIO_F_NOTIFICATION_DATA;
    }

    if(avail_features & VIRTIO_NETWORK_F_GUEST_HDRLEN) {
        PRINTLOG(VIRTIONET, LOG_TRACE, "device has guest hdr len feature");

        req_features |= VIRTIO_NETWORK_F_GUEST_HDRLEN;
    }

    if(avail_features & VIRTIO_NETWORK_F_SPEED_DUPLEX) {
        PRINTLOG(VIRTIONET, LOG_INFO, "device has speed duplex feature");

        req_features |= VIRTIO_NETWORK_F_SPEED_DUPLEX;
    }

    return req_features;
}

int8_t network_rx_tx_queue_item_builder(virtio_dev_t* vdev, void* queue_item) {
    virtio_network_header_t* hdr = (virtio_network_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_item);

    if(vdev->selected_features & VIRTIO_NETWORK_F_MRG_RXBUF) {
        hdr->header_length = sizeof(virtio_network_header_t);
    } else {
        hdr->header_length = sizeof(virtio_network_header_t) - 2;
    }

    return 0;
}

int8_t network_virtio_create_queues(virtio_dev_t* vdev){
    PRINTLOG(VIRTIONET, LOG_TRACE, "vq count %i", vdev->max_vq_count);

    vdev->queues = memory_malloc(sizeof(virtio_queue_ext_t) * vdev->max_vq_count);

    // TODO create only one rx,tx queue

    if(virtio_create_queue(vdev, 0, VIRTIO_NETWORK_QUEUE_ITEM_LENGTH, 1, 0, &network_rx_tx_queue_item_builder, &network_virtio_rx_isr, &network_virtio_combined_isr) != 0) {
        PRINTLOG(VIRTIONET, LOG_ERROR, "cannot create rx queue");

        return -1;
    }

    if(virtio_create_queue(vdev, 1, VIRTIO_NETWORK_QUEUE_ITEM_LENGTH, 0, 0, &network_rx_tx_queue_item_builder, &network_virtio_tx_isr, &network_virtio_combined_isr) != 0) {
        PRINTLOG(VIRTIONET, LOG_ERROR, "cannot create tx queue");

        return -1;
    }

    if(((vdev->selected_features & VIRTIO_NETWORK_F_CTRL_VQ) == VIRTIO_NETWORK_F_CTRL_VQ) && virtio_create_queue(vdev, 2, VIRTIO_NETWORK_CTRL_QUEUE_ITEM_LENGTH, 0, 1, NULL, &network_virtio_ctrl_isr, &network_virtio_combined_isr) != 0) {
        PRINTLOG(VIRTIONET, LOG_ERROR, "cannot create ctrl queue");

        return -1;
    }

    if(vdev->is_legacy) {
        if(vdev->has_msix) {
            outw(vdev->iobase + VIRTIO_IOPORT_CFG_MSIX_VECTOR, 3);
            time_timer_spinsleep(1000);

            while(inw(vdev->iobase + VIRTIO_IOPORT_CFG_MSIX_VECTOR) != 3) {
                PRINTLOG(VIRTIONET, LOG_WARNING, "config msix not configured re-trying");
                outw(vdev->iobase + VIRTIO_IOPORT_CFG_MSIX_VECTOR, 3);
                time_timer_spinsleep(1000);
            }

            pci_msix_set_isr((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 3, &network_virtio_config_isr);
        }
    } else if(vdev->has_msix) {
        vdev->common_config->msix_config = 3;
        time_timer_spinsleep(1000);

        while(vdev->common_config->msix_config != 3) {
            PRINTLOG(VIRTIONET, LOG_WARNING, "config msix not configured re-trying");
            vdev->common_config->msix_config = 3;
            time_timer_spinsleep(1000);
        }

        pci_msix_set_isr((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, 3, &network_virtio_config_isr);
    }

    PRINTLOG(VIRTIONET, LOG_TRACE, "queue configuration completed");

    return 0;
}

uint64_t network_vnet_tx_task_id = 0;

int8_t network_virtio_init(const pci_dev_t* pci_netdev){
    PRINTLOG(VIRTIONET, LOG_INFO, "virtnet device starting");

    virtio_net_devs = list_create_list_with_heap(NULL);

    if(virtio_net_devs == NULL) {
        PRINTLOG(VIRTIONET, LOG_ERROR, "cannot create virtio network devices list");

        return -1;
    }

    virtio_dev_t* vdev_net = virtio_get_device(pci_netdev);

    if(vdev_net == NULL) {
        return -1;
    }

    list_list_insert(virtio_net_devs, vdev_net);

    vdev_net->queue_size = VIRTIO_QUEUE_SIZE;

    if(virtio_init_dev(vdev_net, &network_virtio_select_features, &network_virtio_create_queues) != 0) {
        PRINTLOG(VIRTIONET, LOG_ERROR, "virtnet device starting failed.");

        return -1;
    }

    void** rx_args = memory_malloc(sizeof(void*) * 1);

    if(rx_args == NULL) {
        PRINTLOG(VIRTIONET, LOG_ERROR, "cannot allocate memory for rx task args");

        return -1;
    }

    rx_args[0] = (void*)vdev_net;

    uint64_t rx_task_id = task_create_task(NULL, 2 << 20, 64 << 10, &network_virtio_process_rx, 1, rx_args, "vnet rx");
    vdev_net->rx_task_id = rx_task_id;

    network_vnet_tx_task_id = task_create_task(NULL, 2 << 20, 64 << 10, &network_virtio_process_tx, 0, NULL, "vnet tx");

    if(!vdev_net->is_legacy) {
        if(vdev_net->selected_features & VIRTIO_NETWORK_F_STATUS) {
            uint32_t link_status = ((virtio_network_config_t*)vdev_net->device_config)->status;
            PRINTLOG(VIRTIONET, LOG_INFO, "virtnet device link status %i", link_status);
        }
    }

    uint8_t* mac = (uint8_t*)vdev_net->extra_data;
    PRINTLOG(VIRTIONET, LOG_INFO, "virtnet device mac %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    PRINTLOG(VIRTIONET, LOG_INFO, "virtnet device started");

    return 0;
}
