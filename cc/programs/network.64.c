/**
 * @file network.64.c
 * @brief Network services main entry point.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network.h>
#include <driver/network_virtio.h>
#include <driver/network_e1000.h>
#include <pci.h>
#include <list.h>
#include <memory.h>
#include <logging.h>
#include <cpu/task.h>
#include <cpu.h>
#include <time/timer.h>
#include <network/network_protocols.h>
#include <network/network_info.h>

MODULE("turnstone.user.programs.network");

int8_t   network_process_rx(void);
uint64_t network_info_mke(const void* key);

list_t* network_received_packets = NULL;

map_t* network_info_map = NULL;

uint64_t network_info_mke(const void* key) {
    uint64_t x = 0;
    memory_memcopy(key, &x, sizeof(network_mac_address_t));

    return x;
}

int8_t network_transmit_packet_destroyer(memory_heap_t* heap, void* data) {
    UNUSED(heap);

    network_transmit_packet_t* t_pckt = data;

    memory_free(t_pckt->packet_data);
    memory_free(t_pckt);

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t network_set_return_queue(const network_mac_address_t* mac, list_t* return_queue) {
    network_info_t* ni = (network_info_t*)map_get(network_info_map, mac);

    if(!ni) {
        ni = memory_malloc(sizeof(network_info_t));

        if(!ni) {
            return -1;
        }

        memory_memcopy(mac, ni->mac, sizeof(network_mac_address_t));
        map_insert(network_info_map, ni->mac, ni);
    }

    if(!ni->return_queue) {
        ni->return_queue = return_queue;
    }

    return 0;
}

static int8_t network_send_packet_to_nic(network_transmit_packet_t* orginal_packet, list_t* return_queue) {
    network_transmit_packet_t* tx_packet = memory_malloc_ext(list_get_heap(return_queue), sizeof(network_transmit_packet_t), 0);

    if(tx_packet == NULL) {
        network_transmit_packet_destroyer(NULL, orginal_packet);

        task_yield();

        return -1;
    }

    uint8_t* tx_packet_data = memory_malloc_ext(list_get_heap(return_queue), orginal_packet->packet_len, 0);

    if(tx_packet_data == NULL) {
        network_transmit_packet_destroyer(NULL, orginal_packet);
        memory_free_ext(list_get_heap(return_queue), tx_packet);

        task_yield();

        return -1;
    }

    memory_memcopy(orginal_packet->packet_data, tx_packet_data, orginal_packet->packet_len);

    tx_packet->packet_len = orginal_packet->packet_len;
    tx_packet->packet_data = tx_packet_data;

    network_transmit_packet_destroyer(NULL, orginal_packet);

    if(list_queue_push(return_queue, tx_packet) == -1ULL) {
        memory_free_ext(list_get_heap(return_queue), tx_packet_data);
        memory_free_ext(list_get_heap(return_queue), tx_packet);

        task_yield();

        return -1;
    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "packet pushed to return queue");
    }

    return 0;
}
#pragma GCC diagnostic pop

extern uint64_t network_vnet_tx_task_id;

int8_t network_process_rx(void){
    network_received_packets = list_create_queue_with_heap(NULL);

    task_add_message_queue(network_received_packets);

    while(1) {
        if(list_size(network_received_packets) == 0) {
            PRINTLOG(NETWORK, LOG_TRACE, "no packet received, changing task");
            task_set_message_waiting();
            task_yield();
        }

        while(list_size(network_received_packets)) {
            const network_received_packet_t* packet = list_queue_pop(network_received_packets);

            if(packet) {
                PRINTLOG(NETWORK, LOG_TRACE, "network packet received with length 0x%llx", packet->packet_len);

                list_t* return_list = NULL;

                if(packet->network_type == NETWORK_TYPE_ETHERNET) {

                    network_set_return_queue(packet->network_info, packet->return_queue);

                    return_list = network_ethernet_process_packet((network_ethernet_t*)packet->packet_data, packet->network_info);
                }

                list_t* return_queue = packet->return_queue;

                memory_free(packet->packet_data);
                memory_free((void*)packet);

                if(return_list) {
                    if(return_queue) {
                        boolean_t notify_vnet_tx = list_size(return_list) > 0;

                        while(list_size(return_list)) {
                            network_transmit_packet_t* tx_packet = (network_transmit_packet_t*)list_queue_pop(return_list);

                            if(tx_packet) {
                                if(network_send_packet_to_nic(tx_packet, return_queue) == -1) {
                                    break;
                                }

                                if(notify_vnet_tx && network_vnet_tx_task_id) {
                                    task_set_message_received(network_vnet_tx_task_id);
                                    notify_vnet_tx = false;
                                }
                            }
                        }

                        list_destroy_with_type(return_list, LIST_DESTROY_WITH_DATA, &network_transmit_packet_destroyer);

                    } else {
                        PRINTLOG(NETWORK, LOG_TRACE, "there is no return queue");

                        list_destroy_with_type(return_list, LIST_DESTROY_WITH_DATA, &network_transmit_packet_destroyer);
                    }

                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "packet discarded");
                }

            }

            PRINTLOG(NETWORK, LOG_TRACE, "rx queue size 0x%llx", list_size(network_received_packets));
        }

    }

    return 0;
}

uint64_t network_rx_task_id = 0;

int8_t network_init(void) {
    PRINTLOG(NETWORK, LOG_INFO, "network devices starting");
    int8_t errors = 0;

    network_info_map = map_new(&network_info_mke);

    iterator_t* iter = list_iterator_create(pci_get_context()->network_controllers);

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

    network_rx_task_id = task_create_task(NULL, 2 << 20, 64 << 10, &network_process_rx, 0, NULL, "network rx task");

    PRINTLOG(NETWORK, LOG_INFO, "network devices started");

    return errors;
}
