/**
 * @file network_info.h
 * @brief Network info header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_INFO_H
#define ___NETWORK_INFO_H 0

#include <network/network_protocols.h>
#include <list.h>
#include <map.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct network_info_t {
    network_mac_address_t  mac;
    list_t*                return_queue;
    network_ipv4_address_t ipv4_address;
    network_ipv4_address_t ipv4_subnetmask;
    network_ipv4_address_t ipv4_broadcast;
    network_ipv4_address_t ipv4_gateway;
    network_ipv4_address_t ipv4_nameserver;
    network_ipv4_address_t ipv4_dhcpserver;
    int64_t                lease_time;
    int64_t                rebind_time;
    int64_t                renewal_time;
    boolean_t              is_ipv4_address_set;
    boolean_t              is_ipv4_address_requested;
    uint32_t               ipv4_address_next_request_time;
    boolean_t              has_hw_vlan_support;
    boolean_t              is_vlan_tagged;
    uint16_t               vlan_id;
} network_info_t;

extern map_t* network_info_map;

int8_t network_register_network_info(network_info_t* ni);

#ifdef __cplusplus
}
#endif

#endif
