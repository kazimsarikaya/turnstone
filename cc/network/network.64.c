/**
 * @file network.64.c
 * @brief Network services main entry point.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network.h>
#include <driver/network_igb.h>
#include <pci.h>
#include <list.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>
#include <cpu/task.h>
#include <cpu.h>
#include <time/timer.h>
#include <network/network_info.h>
#include <network/network_packet.h>
#include <network/network_filter.h>

MODULE("turnstone.lib.network");

list_t* network_received_packets = NULL;

memory_heap_t* network_packet_heap = NULL;

static int8_t network_process_rx(void){
    network_received_packets = list_create_queue_with_heap(NULL);

    task_add_message_queue(network_received_packets);

    while(true) {
        if(list_size(network_received_packets) == 0) {
            PRINTLOG(NETWORK, LOG_TRACE, "no packet received, changing task");
            task_set_message_waiting();
            task_yield();
        }

        while(list_size(network_received_packets)) {
            const network_received_packet_t* packet = list_queue_pop(network_received_packets);

            if(!packet) {
                PRINTLOG(NETWORK, LOG_WARNING, "null packet received, skipping");
                continue;
            }

            if(!packet->packet_data) {
                PRINTLOG(NETWORK, LOG_WARNING, "packet with null data received, skipping");
                memory_free((void*)packet);
                continue;
            }

            if(packet->packet_len == 0) {
                PRINTLOG(NETWORK, LOG_WARNING, "packet with zero length received, skipping");
                memory_free(packet->packet_data);
                memory_free((void*)packet);
                continue;
            }

            if(packet->network_type != NETWORK_TYPE_ETHERNET) {
                PRINTLOG(NETWORK, LOG_WARNING, "unsupported network type 0x%02x received, skipping", packet->network_type);
                memory_free(packet->packet_data);
                memory_free((void*)packet);
                continue;
            }

            network_info_t* ni = network_get_network_info(packet->mac);

            if(!ni) {
                PRINTLOG(NETWORK, LOG_WARNING, "no network info found for mac %02x:%02x:%02x:%02x:%02x:%02x, skipping",
                         packet->mac[0], packet->mac[1], packet->mac[2],
                         packet->mac[3], packet->mac[4], packet->mac[5]);
                memory_free(packet->packet_data);
                memory_free((void*)packet);
                continue;
            }

            uint8_t* response_packet_data = NULL;
            uint16_t response_packet_len  = 0;

            if(network_packet_process(ni, packet->packet_data, packet->packet_len, false, &response_packet_data, &response_packet_len) == -1) {
                PRINTLOG(NETWORK, LOG_TRACE, "packet discarded.");
                memory_free(packet->packet_data);
                memory_free((void*)packet);
                continue;
            }

            if(response_packet_data == NULL || response_packet_len == 0) {
                PRINTLOG(NETWORK, LOG_TRACE, "no response packet generated.");
                memory_free(packet->packet_data);
                memory_free((void*)packet);
                continue;
            }

            network_transmit_packet_t* tx_packet = memory_malloc_ext(list_get_heap(packet->return_queue), sizeof(network_transmit_packet_t), 0);

            if(tx_packet == NULL) {
                PRINTLOG(NETWORK, LOG_ERROR, "failed to allocate memory for transmit packet");
                memory_free(packet->packet_data);
                memory_free((void*)packet);
                continue;
            }

            tx_packet->packet_len     = response_packet_len;
            tx_packet->packet_data    = response_packet_data;
            tx_packet->is_vlan_tagged = ni->is_vlan_tagged;
            tx_packet->vlan_id        = ni->vlan_id;

            if(list_queue_push(packet->return_queue, tx_packet) == -1ULL) {
                PRINTLOG(NETWORK, LOG_ERROR, "failed to push packet to return queue");
                memory_free_ext(list_get_heap(packet->return_queue), response_packet_data);
                memory_free_ext(list_get_heap(packet->return_queue), tx_packet);

                continue;
            } else {
                PRINTLOG(NETWORK, LOG_TRACE, "packet pushed to return queue");
            }

        }

        PRINTLOG(NETWORK, LOG_TRACE, "rx queue size 0x%llx", list_size(network_received_packets));
    }

    return 0;
}

uint64_t network_rx_task_id = 0;

int8_t network_init(void) {
    PRINTLOG(NETWORK, LOG_INFO, "network devices starting");
    int8_t errors = 0;

    uint64_t heap_size = 256 << 20;
    frame_t* heap_frames;
    uint64_t heap_frames_cnt = (heap_size + FRAME_SIZE - 1) / FRAME_SIZE;
    heap_size = heap_frames_cnt * FRAME_SIZE;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), heap_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &heap_frames, NULL) != 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "cannot allocate heap with frame count 0x%llx", heap_frames_cnt);

        return -1;
    }

    uint64_t heap_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(heap_frames->frame_address);

    if(memory_paging_add_va_for_frame(heap_va, heap_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "cannot add heap va 0x%llx for frame at 0x%llx with count 0x%llx", heap_va, heap_frames->frame_address, heap_frames->frame_count);

        frame_get_allocator()->release_frame(frame_get_allocator(), heap_frames);

        return -1;
    }

    network_packet_heap = memory_create_heap_hash(heap_va, heap_va + heap_size);

    if(!network_packet_heap) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create network packet heap");

        memory_paging_delete_va_for_frame(heap_va, heap_frames);
        frame_get_allocator()->release_frame(frame_get_allocator(), heap_frames);

        return -1;
    }

    if(network_info_init() == -1) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to initialize network info");

        return -1;
    }

    if(network_filter_init(false, true, true) == -1) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to initialize network filter");

        return -1;
    }

    iterator_t* iter = list_iterator_create(pci_get_context()->network_controllers);

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* pci_netdev = iter->get_item(iter);

        pci_common_header_t* pci_header = pci_netdev->pci_header;

        if(pci_header->vendor_id == NETWORK_DEVICE_VENDOR_ID_INTEL && pci_header->device_id == NETWORK_DEVICE_DEVICE_ID_IGB) {
            errors += network_igb_init(pci_netdev);
        } else {
            PRINTLOG(NETWORK, LOG_ERROR, "unknown net device vendor 0x%04x device 0x%04x", pci_header->vendor_id, pci_header->device_id);
            errors += -1;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    network_rx_task_id = task_create_task(NULL, 2 << 20, 64 << 10, &network_process_rx, 0, NULL, "network rx task");

    PRINTLOG(NETWORK, LOG_INFO, "network devices started");

    return errors;
}
