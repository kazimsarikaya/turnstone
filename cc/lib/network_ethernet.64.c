/**
 * @file network_ethernet.64.c
 * @brief Ethernet protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_protocols.h>
#include <network/network_arp.h>
#include <network/network_ipv4.h>
#include <network/network_info.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>
#include <time.h>
#include <logging.h>

MODULE("turnstone.lib.network");

network_mac_address_t BROADCAST_MAC = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

boolean_t network_ethernet_is_mac_address_eq(network_mac_address_t mac1, network_mac_address_t mac2) {
    for(uint64_t i = 0; i < sizeof(network_mac_address_t); i++) {
        if(mac1[i] != mac2[i]) {
            return false;
        }
    }

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
list_t* network_ethernet_process_packet(network_ethernet_t* recv_eth_packet, void* network_info) {
    if(recv_eth_packet == NULL) {
        return NULL;
    }

    network_info_t* ni = (network_info_t*)map_get(network_info_map, network_info);

    if(ni == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "no network info");
        return NULL;
    }

    uint16_t packet_type = BYTE_SWAP16(recv_eth_packet->type);

    if(!network_ethernet_is_mac_address_eq(recv_eth_packet->destination, ni->mac) &&
       !network_ethernet_is_mac_address_eq(recv_eth_packet->destination, BROADCAST_MAC)) {
        PRINTLOG(NETWORK, LOG_TRACE, "destination is not this machine, discarding packet");
        PRINTLOG(NETWORK, LOG_TRACE, "our mac: %02x:%02x:%02x:%02x:%02x:%02x",
                 ni->mac[0], ni->mac[1], ni->mac[2],
                 ni->mac[3], ni->mac[4], ni->mac[5]);
        PRINTLOG(NETWORK, LOG_TRACE, "destination mac: %02x:%02x:%02x:%02x:%02x:%02x",
                 recv_eth_packet->destination[0], recv_eth_packet->destination[1], recv_eth_packet->destination[2],
                 recv_eth_packet->destination[3], recv_eth_packet->destination[4], recv_eth_packet->destination[5]);

        return NULL;
    }

    uint8_t* data;
    uint8_t* data_inner_packet;

    if(packet_type == NETWORK_PROTOCOL_VLAN) {
        network_ethernet_with_vlan_t* vlan_packet = (network_ethernet_with_vlan_t*)recv_eth_packet;

        packet_type = BYTE_SWAP16(vlan_packet->inner_type);

        data = (uint8_t*)recv_eth_packet;
        data_inner_packet = (data + sizeof(network_ethernet_with_vlan_t));
    } else {
        data = (uint8_t*)recv_eth_packet;
        data_inner_packet = (data + sizeof(network_ethernet_t));
    }


    if(packet_type == NETWORK_PROTOCOL_ARP) {
        PRINTLOG(NETWORK, LOG_TRACE, "arp packet received");
        uint8_t* return_data = NULL;
        uint16_t return_data_len = 0;

        return_data = network_arp_process_packet((network_arp_t*)data_inner_packet, network_info, &return_data_len);

        if(return_data == NULL || return_data_len == 0) {
            return NULL;
        }

        boolean_t need_vlan_frame = false;

        if(!ni->has_hw_vlan_support && ni->is_vlan_tagged) {
            need_vlan_frame = true;
        }

        uint8_t* eth_packet = network_ethernet_create_packet_with_vlan_tag(recv_eth_packet->source, ni->mac,
                                                                           NETWORK_PROTOCOL_ARP,
                                                                           need_vlan_frame, ni->vlan_id,
                                                                           return_data_len, return_data);

        if(eth_packet == NULL) {
            return NULL;
        }

        list_t* res = list_create_list();

        if(res == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "cannot create return list");
            memory_free(eth_packet);
            return NULL;
        }

        network_transmit_packet_t* transmit_packet = memory_malloc(sizeof(network_transmit_packet_t));

        if(transmit_packet == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "cannot create transmit packet");
            memory_free(eth_packet);
            list_destroy(res);
            return NULL;
        }

        transmit_packet->is_vlan_tagged = ni->is_vlan_tagged;
        transmit_packet->vlan_id = ni->vlan_id;
        transmit_packet->packet_data = eth_packet;
        transmit_packet->packet_len = (need_vlan_frame ?
                                       sizeof(network_ethernet_with_vlan_t) : sizeof(network_ethernet_t))
                                      + return_data_len;

        if(transmit_packet->packet_len < NETWORK_ETHERNET_MIN_FRAME_SIZE) {
            transmit_packet->packet_len = NETWORK_ETHERNET_MIN_FRAME_SIZE;
        }

        list_queue_push(res, transmit_packet);

        return res;
    } else if(packet_type == NETWORK_PROTOCOL_IPV4) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet received");
        list_t* ip_pckts = network_ipv4_process_packet((network_ipv4_header_t*)data_inner_packet, network_info);

        if(ip_pckts == NULL) {
            return NULL;
        }

        list_t* res = list_create_list();

        if(res == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "cannot create return list");
            list_destroy_with_type(ip_pckts, LIST_DESTROY_WITH_DATA, network_transmit_packet_destroyer);
            return NULL;
        }

        while(list_size(ip_pckts)) {
            network_transmit_packet_t* ip_pckt = (network_transmit_packet_t*)list_queue_pop(ip_pckts);

            if(ip_pckt) {

                boolean_t need_vlan_frame = false;

                if(!ni->has_hw_vlan_support && ni->is_vlan_tagged) {
                    need_vlan_frame = true;
                }

                uint8_t* eth_packet = network_ethernet_create_packet_with_vlan_tag(recv_eth_packet->source,
                                                                                   ni->mac,
                                                                                   NETWORK_PROTOCOL_IPV4,
                                                                                   need_vlan_frame, ni->vlan_id,
                                                                                   ip_pckt->packet_len, ip_pckt->packet_data);

                if(eth_packet == NULL) {
                    memory_free(ip_pckt); // data deleted by network_ethernet_create_packet
                    break;
                }

                network_transmit_packet_t* transmit_packet = memory_malloc(sizeof(network_transmit_packet_t));

                if(transmit_packet == NULL) {
                    PRINTLOG(NETWORK, LOG_ERROR, "cannot create transmit packet");
                    memory_free(ip_pckt); // data deleted by network_ethernet_create_packet
                    memory_free(eth_packet);
                    break;
                }

                transmit_packet->is_vlan_tagged = ni->is_vlan_tagged;
                transmit_packet->vlan_id = ni->vlan_id;
                transmit_packet->packet_data = eth_packet;
                transmit_packet->packet_len = (need_vlan_frame ?
                                               sizeof(network_ethernet_with_vlan_t) : sizeof(network_ethernet_t))
                                              + ip_pckt->packet_len;

                if(transmit_packet->packet_len < NETWORK_ETHERNET_MIN_FRAME_SIZE) {
                    transmit_packet->packet_len = NETWORK_ETHERNET_MIN_FRAME_SIZE;
                }

                list_queue_push(res, transmit_packet);

                memory_free(ip_pckt); // data deleted by network_ethernet_create_packet
            }
        }

        list_destroy_with_type(ip_pckts, LIST_DESTROY_WITH_DATA, network_transmit_packet_destroyer);

        return res;
    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "unimplemented packet type 0x%04x", packet_type);
    }

    return NULL;
}
#pragma GCC diagnostic pop

uint8_t* network_ethernet_create_packet_with_vlan_tag(network_mac_address_t dst, network_mac_address_t src,
                                                      network_ethernet_type_t type,
                                                      boolean_t is_vlan_tagged, uint16_t vlan_id,
                                                      uint16_t data_len, uint8_t* data) {
    if(data == NULL || data_len == 0) {
        return NULL;
    }

    if(is_vlan_tagged) {
        uint16_t packet_len = sizeof(network_ethernet_with_vlan_t) + data_len;

        if(packet_len < NETWORK_ETHERNET_MIN_FRAME_SIZE) {
            packet_len = NETWORK_ETHERNET_MIN_FRAME_SIZE;
        }

        uint8_t* res = memory_malloc(packet_len);

        if(res == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "cannot allocate memory for ethernet packet with vlan tag");
            return NULL;
        }

        network_ethernet_with_vlan_t* eth_packet = (network_ethernet_with_vlan_t*)res;

        eth_packet->type = BYTE_SWAP16(NETWORK_PROTOCOL_VLAN);
        eth_packet->vlan_tag = BYTE_SWAP16(vlan_id & 0x0FFF); // priority 0, cfi 0
        eth_packet->inner_type = BYTE_SWAP16(type);

        memory_memcopy(dst, eth_packet->destination, sizeof(network_mac_address_t));
        memory_memcopy(src, eth_packet->source, sizeof(network_mac_address_t));
        memory_memcopy(data, res + sizeof(network_ethernet_with_vlan_t), data_len);

        memory_free(data);

        return res;
    }

    uint16_t packet_len = sizeof(network_ethernet_t) + data_len;

    if(packet_len < NETWORK_ETHERNET_MIN_FRAME_SIZE) {
        packet_len = NETWORK_ETHERNET_MIN_FRAME_SIZE;
    }

    uint8_t* res = memory_malloc(packet_len);

    if(res == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "cannot allocate memory for ethernet packet");
        return NULL;
    }

    network_ethernet_t* eth_packet = (network_ethernet_t*)res;

    eth_packet->type = BYTE_SWAP16(type);

    memory_memcopy(dst, eth_packet->destination, sizeof(network_mac_address_t));
    memory_memcopy(src, eth_packet->source, sizeof(network_mac_address_t));
    memory_memcopy(data, res + sizeof(network_ethernet_t), data_len);

    memory_free(data);

    return res;
}
