/**
 * @file network_utils.64.c
 * @brief Network utilities implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <network/network_utils.h>

MODULE("turnstone.lib.network");

network_mac_address_t BROADCAST_MAC                     = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
network_mac_address_t ZERO_MAC                          = {0, 0, 0, 0, 0, 0};
network_ipv4_address_t NETWORK_IPV4_GLOBAL_BROADCAST_IP = { .as_bytes = {255, 255, 255, 255} };
network_ipv4_address_t NETWORK_IPV4_ZERO_IP             = { .as_bytes = {0, 0, 0, 0} };

boolean_t network_ethernet_is_mac_address_eq(const network_mac_address_t mac1, const network_mac_address_t mac2) {
    for(uint64_t i = 0; i < sizeof(network_mac_address_t); i++) {
        if(mac1[i] != mac2[i]) {
            return false;
        }
    }

    return true;
}

boolean_t network_ipv4_is_address_eq(const network_ipv4_address_t ipv4_addr1, const network_ipv4_address_t ipv4_addr2) {
    return (ipv4_addr1.as_dword == ipv4_addr2.as_dword);
}

network_protocol_t network_packet_get_ethernet_ethertype(const network_packet_t* packet) {
    if(packet->is_vlan_tagged) {
        return packet->ether_inner_ethertype;
    }

    return packet->ether_ethertype;
}

boolean_t network_ipv4_is_address_in_same_subnet(const network_ipv4_address_t ip1, const network_ipv4_address_t ip2, const network_ipv4_address_t subnet_mask) {
    return (ip1.as_dword & subnet_mask.as_dword) == (ip2.as_dword & subnet_mask.as_dword);
}
