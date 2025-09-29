/**
 * @file dhcpd.64.c
 * @brief DHCPD service main entry point.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_dhcpv4.h>
#include <network/network_ipv4.h>
#include <network/network_udpv4.h>
#include <network/network_ethernet.h>
#include <logging.h>
#include <random.h>
#include <memory.h>
#include <utils.h>
#include <time/timer.h>
#include <strings.h>

MODULE("turnstone.programs.user.dhcpd");

static uint8_t* network_dhcpv4_create_dhcpv4_discover_or_request_packet(network_info_t* ni, uint16_t* out_packet_size) {
    uint32_t xid = rand();

    network_dhcpv4_t* dhcp_packet = NULL;
    uint16_t return_packet_len = 0;

    char_t* msg = strprintf("creating dhcp %s for mac %02x:%02x:%02x:%02x:%02x:%02x",
                            ni->is_ipv4_address_set ? "request" : "discover",
                            ni->mac[0], ni->mac[1], ni->mac[2],
                            ni->mac[3], ni->mac[4], ni->mac[5]);

    PRINTLOG(NETWORK, LOG_INFO, "%s", msg);

    memory_free(msg);

    if(ni->is_ipv4_address_set) {
        dhcp_packet = network_dhcpv4_create_request_packet(ni, xid, &return_packet_len);
    } else {
        dhcp_packet = network_dhcpv4_create_discover_packet(ni->mac, xid, &return_packet_len);
    }

    if(!dhcp_packet) {
        PRINTLOG(NETWORK, LOG_ERROR, "dhcp packet is null");

        return NULL;
    }


    network_udpv4_header_t* udp = network_udpv4_create_packet_from_data(NETWORK_DHCPV4_SOURCE_PORT, NETWORK_DHCPV4_DESTINATION_PORT, return_packet_len, (uint8_t*)dhcp_packet);

    if(!udp) {
        PRINTLOG(NETWORK, LOG_ERROR, "udp packet is null");
        memory_free(dhcp_packet);

        return NULL;
    }

    list_t* l_ip = network_ipv4_create_packet_from_udp_packet(ni->ipv4_address, NETWORK_IPV4_GLOBAL_BROADCAST_IP, udp);

    if(l_ip == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "ip packet is null");
        memory_free(udp);

        return NULL;
    }

    if(list_size(l_ip) == 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "ip packet list is empty");

        list_destroy(l_ip);

        return NULL;
    }

    network_transmit_packet_t* t_ip = (network_transmit_packet_t*)list_queue_pop(l_ip);

    list_destroy(l_ip);

    if(t_ip == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "ip packet is null");

        return NULL;
    }

    network_ipv4_header_t* ip = (network_ipv4_header_t*)t_ip->packet_data;

    memory_free(t_ip);

    uint16_t tl = BYTE_SWAP16(ip->total_length);

    boolean_t need_vlan_frame = false;

    if(!ni->has_hw_vlan_support && ni->is_vlan_tagged) {
        need_vlan_frame = true;
    }

    uint8_t* eth = network_ethernet_create_packet_with_vlan_tag(BROADCAST_MAC, ni->mac,
                                                                NETWORK_PROTOCOL_IPV4,
                                                                need_vlan_frame, ni->vlan_id,
                                                                tl, (uint8_t*)ip);
    if(eth == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "eth packet is null");
        memory_free(ip);

        return NULL;
    }

    uint32_t eth_packet_size = need_vlan_frame ? sizeof(network_ethernet_with_vlan_t) : sizeof(network_ethernet_t);

    *out_packet_size = eth_packet_size + tl;

    return eth;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t network_dhcpv4_send_dhcpv4_discover_or_request_packet(network_info_t* ni) {
    uint16_t packet_len = 0;

    uint8_t* eth = network_dhcpv4_create_dhcpv4_discover_or_request_packet(ni, &packet_len);

    if(eth == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "eth packet is null");

        return -1;
    }

    network_transmit_packet_t* res = memory_malloc_ext(list_get_heap(ni->return_queue), sizeof(network_transmit_packet_t), 0);

    if(res == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "network transmit packet is null, re trying...");

        memory_free(eth);

        return -1;
    }

    res->packet_len = packet_len;

    uint8_t* packet_data = memory_malloc_ext(list_get_heap(ni->return_queue), res->packet_len, 0);

    if(packet_data == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "packet data allocation failed");

        memory_free(eth);
        memory_free_ext(list_get_heap(ni->return_queue), res);

        return -1;
    }

    memory_memcopy(eth, packet_data, res->packet_len);

    memory_free(eth);

    res->packet_data = packet_data;
    res->is_vlan_tagged = ni->is_vlan_tagged;
    res->vlan_id = ni->vlan_id;

    if(list_queue_push(ni->return_queue, res) == -1ULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to push packet to return queue");
        memory_free_ext(list_get_heap(ni->return_queue), packet_data);
        memory_free_ext(list_get_heap(ni->return_queue), res);

        return -1;
    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "packet pushed to return queue");
    }

    if(!ni->is_ipv4_address_set) {
        PRINTLOG(NETWORK, LOG_INFO, "dhcp discover is sending...");
    } else {
        PRINTLOG(NETWORK, LOG_INFO, "dhcp request is sending...");
    }

    ni->is_ipv4_address_requested = true;

    int32_t wait_time = 1;

    while(wait_time < 16 && ni->is_ipv4_address_requested) {
        time_timer_sleep(wait_time);
        wait_time <<= 1;
    }

    return 0;
}
#pragma GCC diagnostic pop

static void network_dhcpv4_wait_for_renewal(network_info_t* ni) {
    while(ni->lease_time > ni->renewal_time) {
        PRINTLOG(NETWORK, LOG_INFO, "dhcp lease time left %lli seconds, sleeping %lli seconds",
                 ni->lease_time,
                 ni->renewal_time >> 1);
        ni->lease_time -= ni->renewal_time >> 1;
        time_timer_sleep(ni->renewal_time >> 1);
    }

    PRINTLOG(NETWORK, LOG_INFO, "dhcp lease time expired, requesting again");
}

int32_t network_dhcpv4_send_discover(uint64_t args_cnt, void** args) {
    UNUSED(args_cnt);

    network_mac_address_t mac = {};
    uint8_t* mac_data = args[0];
    memory_memcopy(mac_data, mac, sizeof(network_mac_address_t));

    PRINTLOG(NETWORK, LOG_INFO, "dhcp task started for mac %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    network_info_t* ni = (network_info_t*)map_get(network_info_map, mac);

    if(!ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info not found for mac %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return -1;
    }

    int32_t retry_sleep = 5;
    int32_t backoff = 1;

    boolean_t failed = false;

    while(true) {

        if(ni->is_ipv4_address_set && !ni->is_ipv4_address_requested) {
            network_dhcpv4_wait_for_renewal(ni);
        }

        if (failed || ni->is_ipv4_address_requested) {
            int32_t sleep_time = retry_sleep * backoff;

            if(sleep_time >= 300) {
                sleep_time = 300;
                backoff = 1;
            } else {
                backoff <<= 1;
            }

            time_timer_sleep(sleep_time);
        }

        if(network_dhcpv4_send_dhcpv4_discover_or_request_packet(ni) == -1) {
            failed = true;
            PRINTLOG(NETWORK, LOG_ERROR, "failed to send dhcp discover/request packet");
        } else {
            failed = false;
        }
    }

    return 0;
}
