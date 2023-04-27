/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_protocols.h>
#include <network/network_arp.h>
#include <network/network_ipv4.h>
#include <utils.h>
#include <memory.h>
#include <video.h>
#include <time.h>
#include <video.h>

MODULE("turnstone.lib");

network_mac_address_t BROADCAST_MAC = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

boolean_t network_ethernet_is_mac_address_eq(network_mac_address_t mac1, network_mac_address_t mac2) {
    for(uint64_t i = 0; i < sizeof(network_mac_address_t); i++) {
        if(mac1[i] != mac2[i]) {
            return false;
        }
    }

    return true;
}

uint8_t* network_ethernet_process_packet(network_ethernet_t* recv_eth_packet, void* network_info, uint16_t* return_packet_len) {
    if(recv_eth_packet == NULL || return_packet_len == NULL) {
        return NULL;
    }

    network_mac_address_t our_mac = {};
    memory_memcopy(network_info, our_mac, sizeof(network_mac_address_t));

    uint16_t packet_type = BYTE_SWAP16(recv_eth_packet->type);

    if(!network_ethernet_is_mac_address_eq(recv_eth_packet->destination, our_mac) &&
       !network_ethernet_is_mac_address_eq(recv_eth_packet->destination, BROADCAST_MAC)) {
        PRINTLOG(NETWORK, LOG_TRACE, "destination is not this machine, discarding packet");

        return NULL;
    }

    uint8_t* data = (uint8_t*)recv_eth_packet;
    uint8_t* data_inner_packet = (data + sizeof(network_ethernet_t));


    uint8_t* return_data = NULL;
    uint16_t return_data_len = 0;

    if(packet_type == NETWORK_PROTOCOL_ARP) {
        PRINTLOG(NETWORK, LOG_TRACE, "arp packet received");
        return_data = network_arp_process_packet((network_arp_t*)data_inner_packet, network_info, &return_data_len);
    } else if(packet_type == NETWORK_PROTOCOL_IPV4) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet received");
        return_data = network_ipv4_process_packet((network_ipv4_header_t*)data_inner_packet, network_info, &return_data_len);
    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "unimplemented packet type 0x%04x", packet_type);
    }

    if(return_data_len == 0 || return_data == NULL) {
        return NULL;
    } else {
        *return_packet_len = return_data_len + sizeof(network_ethernet_t);
    }

    uint8_t* res = memory_malloc(sizeof(network_ethernet_t) + *return_packet_len);

    if(res == NULL) {
        return NULL;
    }

    network_ethernet_t* eth_packet = (network_ethernet_t*)res;
    eth_packet->type = recv_eth_packet->type;
    memory_memcopy(recv_eth_packet->source, eth_packet->destination, sizeof(network_mac_address_t));
    memory_memcopy(our_mac, eth_packet->source, sizeof(network_mac_address_t));
    memory_memcopy(return_data, res + sizeof(network_ethernet_t), return_data_len);

    memory_free(return_data);

    return res;
}
uint8_t* network_ethernet_create_packet(network_mac_address_t dst, network_mac_address_t src, network_ethernet_type_t type, uint16_t data_len, uint8_t* data) {
    uint8_t* res = memory_malloc(sizeof(network_ethernet_t) + data_len);

    if(res == NULL) {
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
