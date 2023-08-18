/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network.h>
#include <driver/network_virtio.h>
#include <driver/network_e1000.h>
#include <pci.h>
#include <linkedlist.h>
#include <memory.h>
#include <video.h>
#include <cpu/task.h>
#include <cpu.h>
#include <time/timer.h>
#include <network/network_protocols.h>
#include <network/network_info.h>

MODULE("turnstone.lib");

int8_t   network_process_rx(void);
uint64_t network_info_mke(const void* key);

linkedlist_t network_received_packets = NULL;

map_t network_info_map = NULL;

uint64_t network_info_mke(const void* key) {
    uint64_t x = 0;
    memory_memcopy(key, &x, sizeof(network_mac_address_t));

    return x;
}


int8_t network_process_rx(void){
    network_received_packets = linkedlist_create_queue_with_heap(NULL);

    task_add_message_queue(network_received_packets);

    while(1) {
        if(linkedlist_size(network_received_packets) == 0) {
            PRINTLOG(NETWORK, LOG_TRACE, "no packet received, changing task");
            task_set_message_waiting();
            task_yield();
        }

        while(linkedlist_size(network_received_packets)) {
            const network_received_packet_t* packet = linkedlist_queue_peek(network_received_packets);

            if(packet) {
                PRINTLOG(NETWORK, LOG_TRACE, "network packet received with length 0x%llx", packet->packet_len);

                uint16_t return_data_len = 0;
                uint8_t* return_data = NULL;

                if(packet->network_type == NETWORK_TYPE_ETHERNET) {

                    network_info_t* ni = (network_info_t*)map_get(network_info_map, packet->network_info);

                    if(!ni) {
                        ni = memory_malloc(sizeof(network_info_t));
                        memory_memcopy(packet->network_info, ni->mac, sizeof(network_mac_address_t));
                        map_insert(network_info_map, ni->mac, ni);
                    } else {
                        if(!ni->return_queue) {
                            ni->return_queue = packet->return_queue;
                        }
                    }

                    return_data = network_ethernet_process_packet((network_ethernet_t*)packet->packet_data, packet->network_info, &return_data_len);
                }

                linkedlist_t return_queue = packet->return_queue;

                memory_free(packet->packet_data);
                memory_free((void*)packet);

                if(return_data && return_data_len) {
                    if(return_queue) {
                        network_transmit_packet_t* tx_packet = memory_malloc_ext(linkedlist_get_heap(return_queue), sizeof(network_transmit_packet_t), 0);

                        if(tx_packet == NULL) {
                            memory_free(return_data);

                            linkedlist_queue_pop(network_received_packets);

                            task_yield();

                            continue;
                        }

                        uint8_t* tx_packet_data = memory_malloc_ext(linkedlist_get_heap(return_queue), return_data_len, 0);

                        if(tx_packet_data == NULL) {
                            memory_free(return_data);
                            memory_free_ext(linkedlist_get_heap(return_queue), tx_packet);

                            linkedlist_queue_pop(network_received_packets);

                            task_yield();

                            continue;
                        }

                        memory_memcopy(return_data, tx_packet_data, return_data_len);

                        memory_free(return_data);

                        tx_packet->packet_len = return_data_len;
                        tx_packet->packet_data = tx_packet_data;

                        if(linkedlist_queue_push(return_queue, tx_packet) == -1ULL) {
                            memory_free_ext(linkedlist_get_heap(return_queue), tx_packet_data);
                            memory_free_ext(linkedlist_get_heap(return_queue), tx_packet);

                            linkedlist_queue_pop(network_received_packets);

                            task_yield();

                            continue;
                        } else {
                            PRINTLOG(NETWORK, LOG_TRACE, "packet pushed to return queue");
                        }
                    } else {
                        PRINTLOG(NETWORK, LOG_TRACE, "there is no return queue");

                        memory_free(return_data);
                    }

                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "packet discarded");
                }

                linkedlist_queue_pop(network_received_packets);
            }

            PRINTLOG(NETWORK, LOG_TRACE, "rx queue size 0x%llx", linkedlist_size(network_received_packets));
        }

    }

    return 0;
}


int8_t network_init(void) {
    PRINTLOG(NETWORK, LOG_INFO, "network devices starting");
    int8_t errors = 0;

    network_info_map = map_new(&network_info_mke);

    iterator_t* iter = linkedlist_iterator_create(PCI_CONTEXT->network_controllers);

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* pci_netdev = iter->get_item(iter);

        pci_common_header_t* pci_header = pci_netdev->pci_header;



        if(pci_header->vendor_id == NETWORK_DEVICE_VENDOR_ID_VIRTIO && (pci_header->device_id == NETWORK_DEVICE_DEVICE_ID_VIRTNET1 || pci_header->device_id == NETWORK_DEVICE_DEVICE_ID_VIRTNET2)) {
            errors += network_virtio_init(pci_netdev);
        } else if(pci_header->vendor_id == NETWORK_DEVICE_VENDOR_ID_INTEL && ((pci_header->device_id | NETWORK_DEVICE_DEVICE_ID_E1000) == NETWORK_DEVICE_DEVICE_ID_E1000)) {
            errors += network_e1000_init(pci_netdev);
        } else {
            PRINTLOG(NETWORK, LOG_ERROR, "unknown net device vendor 0x%04x device 0x%04x", pci_header->vendor_id, pci_header->device_id);
            errors += -1;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    task_create_task(NULL, 64 << 10, 2 << 20, &network_process_rx, 0, NULL, "network rx task");

    PRINTLOG(NETWORK, LOG_INFO, "network devices started");

    return errors;
}
