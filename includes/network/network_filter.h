/**
 * @file network_filter.h
 * @brief Network filter header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_FILTER_H
#define ___NETWORK_FILTER_H 0

#include <types.h>
#include <network/network_packet.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef enum network_filter_action_t {
    NETWORK_FILTER_ACTION_ACCEPT,
    NETWORK_FILTER_ACTION_DROP,
} network_filter_action_t;

typedef enum network_filter_direction_t {
    NETWORK_FILTER_DIRECTION_INBOUND,
    NETWORK_FILTER_DIRECTION_FORWARD,
    NETWORK_FILTER_DIRECTION_OUTBOUND,
} network_filter_direction_t;

typedef struct network_filter_rule_t {
    network_filter_action_t    action;
    network_filter_direction_t direction;
    network_protocol_t         protocol;
    network_mac_address_t      source_mac;
    network_mac_address_t      destination_mac;
    network_ipv4_protocol_t    ipv4_protocol;
    network_ipv4_address_t     source_ip;
    network_ipv4_address_t     destination_ip;
    uint16_t                   source_port;
    uint16_t                   destination_port;
} network_filter_rule_t;

int8_t network_filter_init(boolean_t allow_forwarding,
                           boolean_t allow_client_listeners,
                           boolean_t allow_server_listeners);
int8_t                  network_filter_add_rule(network_filter_rule_t* rule);
network_filter_action_t network_filter_apply(const network_info_t* ni, network_packet_t* packet, network_filter_direction_t direction);


#ifdef __cplusplus
}
#endif

#endif
