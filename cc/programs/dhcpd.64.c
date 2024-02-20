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

MODULE("turnstone.programs.user.dhcpd");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int32_t network_dhcpv4_send_discover(uint64_t args_cnt, void** args) {
    UNUSED(args_cnt);

    network_mac_address_t mac = {};
    uint8_t* mac_data = args[0];
    memory_memcopy(mac_data, mac, sizeof(network_mac_address_t));

    network_info_t* ni = NULL;

    int32_t req_try = 3;
    uint32_t xid = rand();

    while(1) {
        if(network_info_map) {
            ni = (network_info_t*)map_get(network_info_map, mac);

            if(ni) {
                if(ni->is_ipv4_address_set && !ni->is_ipv4_address_requested) {

                    if(ni->lease_time > ni->renewal_time) {
                        ni->lease_time -= ni->renewal_time >> 1;
                        time_timer_sleep(ni->renewal_time >> 1);
                        continue;
                    } else {
                        xid = rand();
                    }
                } else if (ni->is_ipv4_address_requested) {
                    if(req_try) {
                        req_try--;
                        time_timer_sleep(5);
                        continue;
                    } else {
                        req_try = 3;
                        ni->is_ipv4_address_requested = 0;
                    }
                }
            } else {
                ni = memory_malloc(sizeof(network_info_t));

                if(ni == NULL) {
                    continue;
                }

                memory_memcopy(mac, ni->mac, sizeof(network_mac_address_t));
                ni->return_queue = args[1];
                map_insert(network_info_map, ni->mac, ni);
            }
        }

        if(ni == NULL || ni->return_queue == NULL) {
            PRINTLOG(NETWORK, LOG_WARNING, "network info or return queue is null");
            time_timer_sleep(5);

            continue;
        }

        network_dhcpv4_t* dhcp_packet = NULL;
        uint16_t return_packet_len = 0;

        if(ni->is_ipv4_address_set) {
            dhcp_packet = network_dhcpv4_create_request_packet(ni, xid, &return_packet_len);
        } else {
            dhcp_packet = network_dhcpv4_create_discover_packet(mac, xid, &return_packet_len);
        }

        if(dhcp_packet == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "dhcp packet is null, re trying...");

            continue;
        }


        network_udpv4_header_t* udp = network_udpv4_create_packet_from_data(NETWORK_DHCPV4_SOURCE_PORT, NETWORK_DHCPV4_DESTINATION_PORT, return_packet_len, (uint8_t*)dhcp_packet);

        memory_free(dhcp_packet);

        if(udp == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "udp packet is null, re trying...");

            continue;
        }

        network_ipv4_header_t* ip = network_ipv4_create_packet_from_udp_packet(ni->ipv4_address, NETWORK_IPV4_GLOBAL_BROADCAST_IP, udp);

        if(ip == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "ip packet is null, re trying...");

            continue;
        }

        uint16_t tl = BYTE_SWAP16(ip->total_length);

        uint8_t* eth = (uint8_t*)network_ethernet_create_packet(BROADCAST_MAC, mac, NETWORK_PROTOCOL_IPV4, tl, (uint8_t*)ip);

        if(eth == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "eth packet is null, re trying...");

            continue;
        }

        network_transmit_packet_t* res = memory_malloc_ext(list_get_heap(ni->return_queue), sizeof(network_transmit_packet_t), 0);

        if(res == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "network transmit packet is null, re trying...");

            memory_free(eth);

            continue;
        }

        res->packet_len = sizeof(network_ethernet_t) + tl;

        uint8_t* packet_data = memory_malloc_ext(list_get_heap(ni->return_queue), res->packet_len, 0);

        if(packet_data == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "packet data allocation failed, re trying...");

            memory_free(eth);
            memory_free_ext(list_get_heap(ni->return_queue), res);

            continue;
        }

        memory_memcopy(eth, packet_data, res->packet_len);

        memory_free(eth);

        res->packet_data = packet_data;

        ni->is_ipv4_address_requested = true;

        PRINTLOG(NETWORK, LOG_TRACE, "dhcp packet sending...");

        list_queue_push(ni->return_queue, res);
    }

    return 0;
}
#pragma GCC diagnostic pop
