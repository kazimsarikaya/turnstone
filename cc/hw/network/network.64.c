/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/network.h>
#include <driver/network_virtio.h>
#include <driver/network_e1000.h>
#include <pci.h>
#include <linkedlist.h>
#include <memory.h>
#include <video.h>
#include <cpu/task.h>
#include <cpu.h>
#include <time/timer.h>
#include <driver/network_protocols.h>

int8_t network_process_rx();

linkedlist_t network_received_packets = NULL;

int8_t network_process_rx(){

    task_add_message_queue(network_received_packets);

    while(1) {
        if(linkedlist_size(network_received_packets) == 0) {
            task_set_message_waiting();
            task_yield();
        }

        while(linkedlist_size(network_received_packets)) {
            network_received_packet_t* packet = linkedlist_queue_pop(network_received_packets);

            if(packet) {
                PRINTLOG(NETWORK, LOG_TRACE, "network packet received with length 0x%llx", packet->packet_len);

                uint16_t return_packet_len = 0;
                network_ethernet_t* return_packet = network_process_packet(packet, &return_packet_len);

                linkedlist_t return_queue = packet->return_queue;

                memory_free(packet->packet_data);
                memory_free(packet);

                if(return_packet && return_packet_len) {
                    PRINTLOG(NETWORK, LOG_TRACE, "network packet proccessed with return len 0x%x", return_packet_len);

                    if(return_queue) {
                        network_transmit_packet_t* tx_packet = memory_malloc(sizeof(network_transmit_packet_t));
                        tx_packet->packet_len = return_packet_len;
                        tx_packet->packet_data = (uint8_t*)return_packet;

                        linkedlist_queue_push(return_queue, tx_packet);
                    } else {
                        PRINTLOG(NETWORK, LOG_TRACE, "there is no return packet 0x%x", return_packet_len);
                    }

                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "packet discarded");
                }

            }

            PRINTLOG(NETWORK, LOG_TRACE, "rx queue size 0x%llx", linkedlist_size(network_received_packets));
        }

    }

    return 0;
}


int8_t network_init() {
    PRINTLOG(NETWORK, LOG_INFO, "network devices starting");
    int8_t errors = 0;

    iterator_t* iter = linkedlist_iterator_create(PCI_CONTEXT->network_controllers);

    while(iter->end_of_iterator(iter) != 0) {
        pci_dev_t* pci_netdev = iter->get_item(iter);

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

    network_received_packets = linkedlist_create_queue_with_heap(NULL);
    task_create_task(NULL, 64 << 10, &network_process_rx);

    PRINTLOG(NETWORK, LOG_INFO, "network devices started");

    return errors;
}
