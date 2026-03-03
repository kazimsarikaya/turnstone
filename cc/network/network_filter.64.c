/**
 * @file network_filter.64.c
 * @brief Network filter implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_filter.h>
#include <network/network_utils.h>
#include <network/network_connection.h>
#include <list.h>
#include <memory.h>
#include <logging.h>

MODULE("turnstone.lib.network");

typedef struct network_rule_list_t {
    list_t* input_rules;
    list_t* forward_rules;
    list_t* output_rules;
} network_rule_list_t;

extern memory_heap_t* network_packet_heap;
network_rule_list_t* network_rules              = NULL;
boolean_t network_filter_allow_forwarding       = false;
boolean_t network_filter_allow_client_listeners = false;
boolean_t network_filter_allow_server_listeners = false;


int8_t network_filter_init(boolean_t allow_forwarding,
                           boolean_t allow_client_listeners,
                           boolean_t allow_server_listeners) {
    network_rules = memory_malloc_ext(network_packet_heap, sizeof(network_rule_list_t), 0);

    if(network_rules == NULL) {
        return -1;
    }

    network_rules->input_rules   = list_create_list_with_heap(network_packet_heap);
    network_rules->forward_rules = list_create_list_with_heap(network_packet_heap);
    network_rules->output_rules  = list_create_list_with_heap(network_packet_heap);

    if(network_rules->input_rules == NULL ||
       network_rules->forward_rules == NULL ||
       network_rules->output_rules == NULL) {
        list_destroy(network_rules->input_rules);
        list_destroy(network_rules->forward_rules);
        list_destroy(network_rules->output_rules);
        memory_free(network_rules);
        return -1;
    }

    network_filter_allow_forwarding       = allow_forwarding;
    network_filter_allow_client_listeners = allow_client_listeners;
    network_filter_allow_server_listeners = allow_server_listeners;

    return 0;
}

int8_t network_filter_add_rule(network_filter_rule_t* rule) {
    if(rule == NULL) {
        return -1;
    }

    network_filter_rule_t* new_rule = memory_malloc_ext(network_packet_heap, sizeof(network_filter_rule_t), 0);
    if(new_rule == NULL) {
        return -1;
    }

    memory_memcopy(rule, new_rule, sizeof(network_filter_rule_t));

    list_t* rules_list = NULL;

    switch(rule->direction) {
    case NETWORK_FILTER_DIRECTION_INBOUND:
        rules_list = network_rules->input_rules;
        break;
    case NETWORK_FILTER_DIRECTION_FORWARD:
        rules_list = network_rules->forward_rules;
        break;
    case NETWORK_FILTER_DIRECTION_OUTBOUND:
        rules_list = network_rules->output_rules;
        break;
    default:
        return -1;
    }

    if(list_queue_push(rules_list, new_rule) == -1ULL) {
        return -1;
    }

    return 0;
}

static boolean_t network_filter_rule_matches_packet(const network_filter_rule_t* rule, const network_info_t* ni, const network_packet_t* packet) {
    UNUSED(ni);

    if(rule->protocol != 0 && rule->protocol != network_packet_get_ethernet_ethertype(packet)) {
        return false;
    }

    if(!network_ethernet_is_mac_address_eq(rule->source_mac, ZERO_MAC) &&
       !network_ethernet_is_mac_address_eq(rule->source_mac, packet->ether_source_mac)) {
        return false;
    }

    if(!network_ethernet_is_mac_address_eq(rule->destination_mac, ZERO_MAC) &&
       !network_ethernet_is_mac_address_eq(rule->destination_mac, packet->ether_destination_mac)) {
        return false;
    }

    if(rule->ipv4_protocol != 0 && rule->ipv4_protocol != packet->ipv4_protocol) {
        return false;
    }

    if(!network_ipv4_is_address_eq(rule->source_ip, NETWORK_IPV4_ZERO_IP) &&
       !network_ipv4_is_address_eq(rule->source_ip, packet->ipv4_source_ip)) {
        return false;
    }

    if(!network_ipv4_is_address_eq(rule->destination_ip, NETWORK_IPV4_ZERO_IP) &&
       !network_ipv4_is_address_eq(rule->destination_ip, packet->ipv4_destination_ip)) {
        return false;
    }

    if((rule->source_port != 0 && packet->is_tcpv4_packet && rule->source_port != packet->tcpv4_source_port) ||
       (rule->source_port != 0 && packet->is_udpv4_packet && rule->source_port != packet->udpv4_source_port)) {
        return false;
    }

    if((rule->destination_port != 0 && packet->is_tcpv4_packet && rule->destination_port != packet->tcpv4_destination_port) ||
       (rule->destination_port != 0 && packet->is_udpv4_packet && rule->destination_port != packet->udpv4_destination_port)) {
        return false;
    }

    return true;
}

network_filter_action_t network_filter_apply(const network_info_t* ni, network_packet_t* packet, network_filter_direction_t direction) {
    if(ni == NULL || packet == NULL) {
        return NETWORK_FILTER_ACTION_DROP;
    }

    if(direction == NETWORK_FILTER_DIRECTION_FORWARD && !network_filter_allow_forwarding) {
        return NETWORK_FILTER_ACTION_DROP;
    }

    // default input rules
    if(direction == NETWORK_FILTER_DIRECTION_INBOUND) {
        // drop if packet if destination mac is not this machine and not broadcast
        if(!network_ethernet_is_mac_address_eq(packet->ether_destination_mac, ni->mac) &&
           !network_ethernet_is_mac_address_eq(packet->ether_destination_mac, BROADCAST_MAC)) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping inbound packet. destination mac is not this machine and not broadcast. destination mac: %02x:%02x:%02x:%02x:%02x:%02x, nic mac: %02x:%02x:%02x:%02x:%02x:%02x",
                     packet->ether_destination_mac[0], packet->ether_destination_mac[1], packet->ether_destination_mac[2],
                     packet->ether_destination_mac[3], packet->ether_destination_mac[4], packet->ether_destination_mac[5],
                     ni->mac[0], ni->mac[1], ni->mac[2], ni->mac[3], ni->mac[4], ni->mac[5]);
            return NETWORK_FILTER_ACTION_DROP;
        }

        // drop if source mac is broadcast
        if(network_ethernet_is_mac_address_eq(packet->ether_source_mac, BROADCAST_MAC)) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping inbound packet. source mac is broadcast. source mac: %02x:%02x:%02x:%02x:%02x:%02x",
                     packet->ether_source_mac[0], packet->ether_source_mac[1], packet->ether_source_mac[2],
                     packet->ether_source_mac[3], packet->ether_source_mac[4], packet->ether_source_mac[5]);
            return NETWORK_FILTER_ACTION_DROP;
        }

        // drop arp packets if we dont have an ip address set.
        if(!ni->is_ipv4_address_set &&
           network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_ARP) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping inbound ARP packet. no ip address set on nic. target ip: %u.%u.%u.%u",
                     packet->arp_target_ip.as_bytes[0],
                     packet->arp_target_ip.as_bytes[1],
                     packet->arp_target_ip.as_bytes[2],
                     packet->arp_target_ip.as_bytes[3]);
            return NETWORK_FILTER_ACTION_DROP;
        }

        // alow arp packets if they are for this machine
        if(ni->is_ipv4_address_set &&
           network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_ARP &&
           network_ipv4_is_address_eq(packet->arp_target_ip, ni->ipv4_address)) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }

        // drop arp packets if they are not for this machine and not broadcast
        if(ni->is_ipv4_address_set &&
           network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_ARP &&
           !network_ipv4_is_address_eq(packet->arp_target_ip, ni->ipv4_address) &&
           !network_ipv4_is_address_eq(packet->arp_target_ip, ni->ipv4_broadcast)) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping inbound ARP packet. target ip is not this machine and not broadcast. target ip: %u.%u.%u.%u, nic ip: %u.%u.%u.%u, nic broadcast ip: %u.%u.%u.%u",
                     packet->arp_target_ip.as_bytes[0],
                     packet->arp_target_ip.as_bytes[1],
                     packet->arp_target_ip.as_bytes[2],
                     packet->arp_target_ip.as_bytes[3],
                     ni->ipv4_address.as_bytes[0],
                     ni->ipv4_address.as_bytes[1],
                     ni->ipv4_address.as_bytes[2],
                     ni->ipv4_address.as_bytes[3],
                     ni->ipv4_broadcast.as_bytes[0],
                     ni->ipv4_broadcast.as_bytes[1],
                     ni->ipv4_broadcast.as_bytes[2],
                     ni->ipv4_broadcast.as_bytes[3]);
            return NETWORK_FILTER_ACTION_DROP;
        }

        // accept if we dont have an ip address set and the packet is ipv4 broadcast
        if(!ni->is_ipv4_address_set &&
           network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_IPV4 &&
           network_ipv4_is_address_eq(packet->ipv4_destination_ip, NETWORK_IPV4_GLOBAL_BROADCAST_IP)) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }

        // drop if destination ip is not this machine and not broadcast
        if(ni->is_ipv4_address_set &&
           network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_IPV4 &&
           !network_ipv4_is_address_eq(packet->ipv4_destination_ip, ni->ipv4_address) &&
           !network_ipv4_is_address_eq(packet->ipv4_destination_ip, ni->ipv4_broadcast)) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping inbound packet. destination ip is not this machine and not broadcast. destination ip: %u.%u.%u.%u, nic ip: %u.%u.%u.%u, nic broadcast ip: %u.%u.%u.%u",
                     packet->ipv4_destination_ip.as_bytes[0],
                     packet->ipv4_destination_ip.as_bytes[1],
                     packet->ipv4_destination_ip.as_bytes[2],
                     packet->ipv4_destination_ip.as_bytes[3],
                     ni->ipv4_address.as_bytes[0],
                     ni->ipv4_address.as_bytes[1],
                     ni->ipv4_address.as_bytes[2],
                     ni->ipv4_address.as_bytes[3],
                     ni->ipv4_broadcast.as_bytes[0],
                     ni->ipv4_broadcast.as_bytes[1],
                     ni->ipv4_broadcast.as_bytes[2],
                     ni->ipv4_broadcast.as_bytes[3]);
            return NETWORK_FILTER_ACTION_DROP;
        }

        // if packet icmpv4 and destination ip is this machine, allow it
        if(network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_IPV4 &&
           packet->is_icmpv4_packet &&
           ni->is_ipv4_address_set &&
           network_ipv4_is_address_eq(packet->ipv4_destination_ip, ni->ipv4_address)) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }

        // if direction is inbound and there is client listener for the packet, accept it
        if(network_filter_allow_client_listeners &&
           network_listener_exists(NETWORK_LISTENER_ROLE_CLIENT,
                                   network_packet_get_ethernet_ethertype(packet),
                                   packet->ipv4_protocol,
                                   packet->ipv4_destination_ip,
                                   packet->is_tcpv4_packet ? packet->tcpv4_destination_port :
                                   (packet->is_udpv4_packet ? packet->udpv4_destination_port : 0))) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }

        // if direction is inbound and general policy allows server listeners
        // and there is a server listener for the packet, accept it
        if(network_filter_allow_server_listeners &&
           network_listener_exists(NETWORK_LISTENER_ROLE_SERVER,
                                   network_packet_get_ethernet_ethertype(packet),
                                   packet->ipv4_protocol,
                                   packet->ipv4_destination_ip,
                                   packet->is_tcpv4_packet ? packet->tcpv4_destination_port :
                                   (packet->is_udpv4_packet ? packet->udpv4_destination_port : 0))) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }
    }

    // default output rules
    if(direction == NETWORK_FILTER_DIRECTION_OUTBOUND) {
        // drop if source mac is not this machine
        if(!network_ethernet_is_mac_address_eq(packet->ether_source_mac, ni->mac)) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping outbound packet. source mac is not this machine.");
            return NETWORK_FILTER_ACTION_DROP;
        }

        // drop if destination mac is BROADCAST_MAC unless the packet is arp or dhcp discover/request
        if(network_ethernet_is_mac_address_eq(packet->ether_destination_mac, BROADCAST_MAC) &&
           !(network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_ARP ||
             (network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_IPV4 &&
              packet->ipv4_protocol == NETWORK_IPV4_PROTOCOL_UDPV4 &&
              packet->udpv4_source_port == 68 &&
              packet->udpv4_destination_port == 67))
           ) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping outbound packet. destination mac is broadcast mac. ethernet ethertype: 0x%04x, ipv4 protocol: 0x%02x, udp source port: %u, udp destination port: %u",
                     network_packet_get_ethernet_ethertype(packet),
                     packet->ipv4_protocol,
                     packet->udpv4_source_port,
                     packet->udpv4_destination_port);
            return NETWORK_FILTER_ACTION_DROP;
        }


        // drop if source ip is not this machine
        if(ni->is_ipv4_address_set &&
           network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_IPV4 &&
           !network_ipv4_is_address_eq(packet->ipv4_source_ip, ni->ipv4_address)) {
            PRINTLOG(NETWORK, LOG_TRACE, "dropping outbound packet. source ip is not this machine. source ip: %u.%u.%u.%u nic ip: %u.%u.%u.%u",
                     packet->ipv4_source_ip.as_bytes[0],
                     packet->ipv4_source_ip.as_bytes[1],
                     packet->ipv4_source_ip.as_bytes[2],
                     packet->ipv4_source_ip.as_bytes[3],
                     ni->ipv4_address.as_bytes[0],
                     ni->ipv4_address.as_bytes[1],
                     ni->ipv4_address.as_bytes[2],
                     ni->ipv4_address.as_bytes[3]);
            return NETWORK_FILTER_ACTION_DROP;
        }

        // allow arp packets if source ip is this machine or not set
        if(network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_ARP &&
           (!ni->is_ipv4_address_set || network_ipv4_is_address_eq(packet->arp_source_ip, ni->ipv4_address))) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }

        // alow icmpv4 packets if source ip is this machine
        if(network_packet_get_ethernet_ethertype(packet) == NETWORK_PROTOCOL_IPV4 &&
           packet->is_icmpv4_packet &&
           ni->is_ipv4_address_set &&
           network_ipv4_is_address_eq(packet->ipv4_source_ip, ni->ipv4_address)) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }

        // if direction is outbound and there is client listener for the packet, accept it
        if(network_filter_allow_client_listeners &&
           network_listener_exists(NETWORK_LISTENER_ROLE_CLIENT,
                                   network_packet_get_ethernet_ethertype(packet),
                                   packet->ipv4_protocol,
                                   packet->ipv4_source_ip,
                                   packet->is_tcpv4_packet ? packet->tcpv4_source_port :
                                   (packet->is_udpv4_packet ? packet->udpv4_source_port : 0))) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }

        // if direction is outbound and general policy allows server listeners
        // and there is a server listener for the packet, accept it
        if(network_filter_allow_server_listeners &&
           network_listener_exists(NETWORK_LISTENER_ROLE_SERVER,
                                   network_packet_get_ethernet_ethertype(packet),
                                   packet->ipv4_protocol,
                                   packet->ipv4_source_ip,
                                   packet->is_tcpv4_packet ? packet->tcpv4_source_port :
                                   (packet->is_udpv4_packet ? packet->udpv4_source_port : 0))) {
            return NETWORK_FILTER_ACTION_ACCEPT;
        }
    }

    list_t* rules_list = NULL;

    switch(direction) {
    case NETWORK_FILTER_DIRECTION_INBOUND:
        rules_list = network_rules->input_rules;
        break;
    case NETWORK_FILTER_DIRECTION_FORWARD:
        rules_list = network_rules->forward_rules;
        break;
    case NETWORK_FILTER_DIRECTION_OUTBOUND:
        rules_list = network_rules->output_rules;
        break;
    default:
        return NETWORK_FILTER_ACTION_DROP;
    }

    for(size_t i = 0; i < list_size(rules_list); i++) {
        const network_filter_rule_t* rule = list_get_data_at_position(rules_list, i);

        if(network_filter_rule_matches_packet(rule, ni, packet)) {
            return rule->action;
        }
    }

    // default policy is to drop the packet
    PRINTLOG(NETWORK, LOG_TRACE, "dropping packet.");
    return NETWORK_FILTER_ACTION_DROP;
}
