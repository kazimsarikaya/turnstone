/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_ETHERNET_H
#define ___NETWORK_ETHERNET_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>

typedef enum {
    NETWORK_ETHERNET_TYPE_ARP=NETWORK_PROTOCOL_ARP,
    NETWORK_ETHERNET_TYPE_IPV4=NETWORK_PROTOCOL_IPV4,
} network_ethernet_type_t;

typedef struct {
    network_mac_address_t   destination;
    network_mac_address_t   source;
    network_ethernet_type_t type : 16;
}__attribute__((packed)) network_ethernet_t;

extern network_mac_address_t BROADCAST_MAC;

boolean_t network_ethernet_is_mac_address_eq(network_mac_address_t mac1, network_mac_address_t mac2);
uint8_t*  network_ethernet_process_packet(network_ethernet_t* recv_eth_packet, void* network_info, uint16_t* return_packet_len);

uint8_t* network_ethernet_create_packet(network_mac_address_t dest, network_mac_address_t src, network_ethernet_type_t type, uint16_t data_len, uint8_t* data);

#endif
