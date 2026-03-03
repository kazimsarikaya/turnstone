/**
 * @file network_utils.h
 * @brief Network utilities header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_UTILS_H
#define ___NETWORK_UTILS_H 0

#include <types.h>
#include <network/network_packet.h>


#ifdef __cplusplus
extern "C" {
#endif

extern network_mac_address_t BROADCAST_MAC;
extern network_mac_address_t ZERO_MAC;
extern network_ipv4_address_t NETWORK_IPV4_GLOBAL_BROADCAST_IP;
extern network_ipv4_address_t NETWORK_IPV4_ZERO_IP;

boolean_t          network_ethernet_is_mac_address_eq(const network_mac_address_t mac1, const network_mac_address_t mac2);
boolean_t          network_ipv4_is_address_eq(const network_ipv4_address_t ipv4_addr1, const network_ipv4_address_t ipv4_addr2);
network_protocol_t network_packet_get_ethernet_ethertype(const network_packet_t* packet);
boolean_t          network_ipv4_is_address_in_same_subnet(const network_ipv4_address_t ip1, const network_ipv4_address_t ip2, const network_ipv4_address_t subnet_mask);

#ifdef __cplusplus
}
#endif

#endif
