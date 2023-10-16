/**
 * @file network_arp.64.c
 * @brief ARP protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_arp.h>
#include <network/network_ipv4.h>
#include <network/network_info.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>
#include <time.h>
#include <logging.h>

MODULE("turnstone.lib");

uint8_t*       network_arp_create_reply_from_packet(network_arp_t* src_arp_packet, network_mac_address_t mac, uint16_t* return_packet_len);
network_arp_t* network_arp_create_request(network_mac_address_t src_mac, network_ipv4_address_t src_ip, network_ipv4_address_t tgt_ip);

uint8_t* network_arp_process_packet(network_arp_t* recv_arp_packet, void* network_info, uint16_t* return_packet_len) {
    UNUSED(network_info);

    if(BYTE_SWAP16(recv_arp_packet->operation_code) == NETWORK_ARP_OPERATION_CODE_REQUEST) {
        return network_arp_create_reply_from_packet(recv_arp_packet, network_info, return_packet_len);
    }

    return NULL;
}


uint8_t* network_arp_create_reply_from_packet(network_arp_t* src_arp_packet, network_mac_address_t mac, uint16_t* return_packet_len) {
    if(return_packet_len == NULL) {
        return NULL;
    }

    const network_info_t* ni = map_get(network_info_map, mac);

    if(!ni) {
        return NULL;
    }

    if(!ni->is_ipv4_address_set) {
        return NULL;
    }


    if(!network_ipv4_is_address_eq(ni->ipv4_address, src_arp_packet->target_ip)) {
        return NULL;
    }

    network_arp_t* arp_packet = memory_malloc(sizeof(network_arp_t));

    if(arp_packet == NULL) {
        return NULL;
    }

    arp_packet->hardware_type = BYTE_SWAP16(NETWORK_ARP_HARDWARE_TYPE_ETHERNET);
    arp_packet->protocol_type = BYTE_SWAP16(NETWORK_ARP_PROTOCOL_TYPE_IP);
    arp_packet->hardware_address_length = NETWORK_ARP_HARDWARE_ADDRESS_LENGTH;
    arp_packet->protocol_address_length = NETWORK_ARP_PROTOCOL_ADDRESS_LENGTH;
    arp_packet->operation_code = BYTE_SWAP16(NETWORK_ARP_OPERATION_CODE_ANSWER);

    memory_memcopy(src_arp_packet->source_ip, arp_packet->target_ip, sizeof(network_ipv4_address_t));
    memory_memcopy(src_arp_packet->source_mac, arp_packet->target_mac, sizeof(network_mac_address_t));

    PRINTLOG(NETWORK, LOG_TRACE, "arp ip address %i.%i.%i.%i", arp_packet->target_ip[0], arp_packet->target_ip[1], arp_packet->target_ip[2], arp_packet->target_ip[3]);
    PRINTLOG(NETWORK, LOG_TRACE, "arp mac address %02x:%02x:%02x:%02x:%02x:%02x", arp_packet->target_mac[0], arp_packet->target_mac[1], arp_packet->target_mac[2], arp_packet->target_mac[3], arp_packet->target_mac[4], arp_packet->target_mac[5]);

    memory_memcopy(mac, arp_packet->source_mac, sizeof(network_mac_address_t));
    memory_memcopy(src_arp_packet->target_ip, arp_packet->source_ip, sizeof(network_ipv4_address_t));

    *return_packet_len = sizeof(network_arp_t);

    return (uint8_t*)arp_packet;
}

network_arp_t* network_arp_create_request(network_mac_address_t src_mac, network_ipv4_address_t src_ip, network_ipv4_address_t tgt_ip){
    network_arp_t* arp_packet = memory_malloc(sizeof(network_arp_t));

    if(arp_packet == NULL) {
        return NULL;
    }

    arp_packet->hardware_type = BYTE_SWAP16(NETWORK_ARP_HARDWARE_TYPE_ETHERNET);
    arp_packet->protocol_type = BYTE_SWAP16(NETWORK_ARP_PROTOCOL_TYPE_IP);
    arp_packet->hardware_address_length = NETWORK_ARP_HARDWARE_ADDRESS_LENGTH;
    arp_packet->protocol_address_length = NETWORK_ARP_PROTOCOL_ADDRESS_LENGTH;
    arp_packet->operation_code = BYTE_SWAP16(NETWORK_ARP_OPERATION_CODE_REQUEST);

    memory_memcopy(src_mac, arp_packet->source_mac, sizeof(network_mac_address_t));
    memory_memcopy(src_ip, arp_packet->source_ip, sizeof(network_ipv4_address_t));
    memory_memcopy(tgt_ip, arp_packet->target_ip, sizeof(network_ipv4_address_t));

    return arp_packet;
}
