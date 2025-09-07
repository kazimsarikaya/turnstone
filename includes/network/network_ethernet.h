/**
 * @file network_ethernet.h
 * @brief Ethernet header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_ETHERNET_H
#define ___NETWORK_ETHERNET_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum network_ethernet_type_t {
    NETWORK_ETHERNET_TYPE_ARP=NETWORK_PROTOCOL_ARP,
    NETWORK_ETHERNET_TYPE_IPV4=NETWORK_PROTOCOL_IPV4,
    NETWORK_ETHERNET_TYPE_IPV6=NETWORK_PROTOCOL_IPV6,
    NETWORK_ETHERNET_TYPE_VLAN=NETWORK_PROTOCOL_VLAN,
} network_ethernet_type_t;

typedef struct network_ethernet_t {
    network_mac_address_t   destination;
    network_mac_address_t   source;
    network_ethernet_type_t type : 16;
}__attribute__((packed)) network_ethernet_t;

_Static_assert(sizeof(network_ethernet_t) == 14, "invalid network_ethernet_t size");

typedef struct network_ethernet_with_vlan_t {
    network_mac_address_t   destination;
    network_mac_address_t   source;
    network_ethernet_type_t type       : 16;
    uint16_t                vlan_tag   : 16;
    network_ethernet_type_t inner_type : 16;
}__attribute__((packed)) network_ethernet_with_vlan_t;

_Static_assert(sizeof(network_ethernet_with_vlan_t) == 18, "invalid network_ethernet_with_vlan_t size");

#define NETWORK_ETHERNET_MIN_FRAME_SIZE 60

extern network_mac_address_t BROADCAST_MAC;

boolean_t network_ethernet_is_mac_address_eq(network_mac_address_t mac1, network_mac_address_t mac2);
list_t*   network_ethernet_process_packet(network_ethernet_t* recv_eth_packet, void* network_info);

uint8_t* network_ethernet_create_packet_with_vlan_tag(network_mac_address_t dest, network_mac_address_t src, network_ethernet_type_t type, boolean_t is_vlan_tagged, uint16_t vlan_id, uint16_t data_len, uint8_t* data);

#define network_ethernet_create_packet(dest, src, type, data_len, data) \
        network_ethernet_create_packet_with_vlan_tag(dest, src, type, false, 0, data_len, data)

#ifdef __cplusplus
}
#endif

#endif
