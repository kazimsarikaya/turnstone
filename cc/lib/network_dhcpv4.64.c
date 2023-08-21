/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_dhcpv4.h>
#include <network/network_ipv4.h>
#include <network/network_udpv4.h>
#include <network/network_ethernet.h>
#include <video.h>
#include <random.h>
#include <memory.h>
#include <utils.h>
#include <network/network_info.h>
#include <time/timer.h>

MODULE("turnstone.lib");

network_dhcpv4_t* network_dhcpv4_create_discover_packet(network_mac_address_t mac, uint32_t xid, uint16_t * return_packet_len);
network_dhcpv4_t* network_dhcpv4_create_request_packet(network_info_t* ni, uint32_t xid, uint16_t * return_packet_len);

network_dhcpv4_t* network_dhcpv4_create_discover_packet(network_mac_address_t mac, uint32_t xid, uint16_t * return_packet_len) {
    uint16_t dhcp_discover_size = sizeof(network_dhcpv4_t) + 9;

    if(return_packet_len) {
        *return_packet_len = dhcp_discover_size;
    }

    network_dhcpv4_t * dhcp_discover = memory_malloc(dhcp_discover_size);

    if(dhcp_discover == NULL) {
        return NULL;
    }

    dhcp_discover->opcode = NETWORK_DHCPV4_OPCODE_DISCOVER;
    dhcp_discover->hardware_type = NETWORK_DHCPV4_HTYPE;
    dhcp_discover->hardware_address_length = NETWORK_DHCPV4_HLEN;
    dhcp_discover->hops = NETWORK_DHCPV4_HOPS;
    dhcp_discover->xid = xid;

    memory_memcopy(mac, dhcp_discover->client_mac_address, sizeof(network_mac_address_t));

    dhcp_discover->magic_cookie = BYTE_SWAP32(NETWORK_DHCPV4_MAGICCOOKIE);

    dhcp_discover->options[0] = NETWORK_DHCPV4_OPTION_MESSAGETYPE;
    dhcp_discover->options[1] = 1;
    dhcp_discover->options[2] = NETWORK_DHCPV4_OPCODE_DISCOVER;

    dhcp_discover->options[3] = NETWORK_DHCPV4_OPTION_PARAMETER_REQUEST_LIST;
    dhcp_discover->options[4] = 3;
    dhcp_discover->options[5] = NETWORK_DHCPV4_OPTION_SUBNETMASK;
    dhcp_discover->options[6] = NETWORK_DHCPV4_OPTION_ROUTER;
    dhcp_discover->options[7] = NETWORK_DHCPV4_OPTION_DOMAINNAMESERVER;

    dhcp_discover->options[8] = NETWORK_DHCPV4_OPTION_END;

    return dhcp_discover;
}

network_dhcpv4_t* network_dhcpv4_create_request_packet(network_info_t* ni, uint32_t xid, uint16_t * return_packet_len) {
    uint16_t dhcp_discover_size = sizeof(network_dhcpv4_t) + 16;

    if(return_packet_len) {
        *return_packet_len = dhcp_discover_size;
    }

    network_dhcpv4_t * dhcp_discover = memory_malloc(dhcp_discover_size);

    if(dhcp_discover == NULL) {
        return NULL;
    }

    dhcp_discover->opcode = NETWORK_DHCPV4_OPCODE_DISCOVER;
    dhcp_discover->hardware_type = NETWORK_DHCPV4_HTYPE;
    dhcp_discover->hardware_address_length = NETWORK_DHCPV4_HLEN;
    dhcp_discover->hops = NETWORK_DHCPV4_HOPS;
    dhcp_discover->xid = xid;

    memory_memcopy(ni->ipv4_dhcpserver, dhcp_discover->server_ip, sizeof(network_ipv4_address_t));
    memory_memcopy(ni->mac, dhcp_discover->client_mac_address, sizeof(network_mac_address_t));

    dhcp_discover->magic_cookie = BYTE_SWAP32(NETWORK_DHCPV4_MAGICCOOKIE);

    dhcp_discover->options[0] = NETWORK_DHCPV4_OPTION_MESSAGETYPE;
    dhcp_discover->options[1] = 1;
    dhcp_discover->options[2] = NETWORK_DHCPV4_OPCODE_REQUEST;

    dhcp_discover->options[3] = NETWORK_DHCPV4_OPTION_REQUESTIP;
    dhcp_discover->options[4] = 4;
    memory_memcopy(ni->ipv4_address, &dhcp_discover->options[5], sizeof(network_ipv4_address_t));


    dhcp_discover->options[9] = NETWORK_DHCPV4_OPTION_SERVERIP;
    dhcp_discover->options[10] = 4;
    memory_memcopy(ni->ipv4_dhcpserver, &dhcp_discover->options[11], sizeof(network_ipv4_address_t));

    dhcp_discover->options[15] = NETWORK_DHCPV4_OPTION_END;

    return dhcp_discover;
}

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

        network_transmit_packet_t* res = memory_malloc_ext(linkedlist_get_heap(ni->return_queue), sizeof(network_transmit_packet_t), 0);

        if(res == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "network transmit packet is null, re trying...");

            memory_free(eth);

            continue;
        }

        res->packet_len = sizeof(network_ethernet_t) + tl;

        uint8_t* packet_data = memory_malloc_ext(linkedlist_get_heap(ni->return_queue), res->packet_len, 0);

        if(packet_data == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "packet data allocation failed, re trying...");

            memory_free(eth);
            memory_free_ext(linkedlist_get_heap(ni->return_queue), res);

            continue;
        }

        memory_memcopy(eth, packet_data, res->packet_len);

        memory_free(eth);

        res->packet_data = packet_data;

        ni->is_ipv4_address_requested = true;

        PRINTLOG(NETWORK, LOG_TRACE, "dhcp packet sending...");

        linkedlist_queue_push(ni->return_queue, res);
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
uint8_t* network_dhcpv4_process_packet(network_dhcpv4_t* recv_dhcpv4_packet, void* network_info, uint16_t* return_packet_len) {
    UNUSED(return_packet_len);

    if(recv_dhcpv4_packet == NULL) {
        return NULL;
    }

    if(!network_ethernet_is_mac_address_eq(network_info, recv_dhcpv4_packet->client_mac_address)) {
        return NULL;
    }

    if(BYTE_SWAP32(recv_dhcpv4_packet->magic_cookie) != NETWORK_DHCPV4_MAGICCOOKIE) {
        return NULL;
    }

    if(recv_dhcpv4_packet->opcode != NETWORK_DHCPV4_OPCODE_OFFER) {
        return NULL;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "dhcp packet received");

    network_dhcpv4_opcode_t type = 0;

    uint8_t* options = recv_dhcpv4_packet->options;
    uint16_t idx = 0;

    while(options[idx] != NETWORK_DHCPV4_OPTION_END) {
        if(options[idx] == NETWORK_DHCPV4_OPTION_MESSAGETYPE) {
            type = options[idx + 2];
            break;
        }

        idx += options[idx + 1] + 2;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "dhcp packet type %i", type);

    network_info_t* ni = (network_info_t*)map_get(network_info_map, network_info);

    if(ni == NULL || ni->return_queue == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "no network info or return queue");

        return NULL;
    }

    if(type == NETWORK_DHCPV4_OPCODE_OFFER) {
        memory_memcopy(recv_dhcpv4_packet->your_ip, ni->ipv4_address, sizeof(network_ipv4_address_t));
        memory_memcopy(recv_dhcpv4_packet->server_ip, ni->ipv4_dhcpserver, sizeof(network_ipv4_address_t));

        uint16_t dhcp_packet_len = 0;
        network_dhcpv4_t* dhcp_packet = network_dhcpv4_create_request_packet(ni, recv_dhcpv4_packet->xid, &dhcp_packet_len);

        if(dhcp_packet == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "dhcp packet is null");

            return NULL;
        }

        network_udpv4_header_t* udp = network_udpv4_create_packet_from_data(NETWORK_DHCPV4_SOURCE_PORT, NETWORK_DHCPV4_DESTINATION_PORT, dhcp_packet_len, (uint8_t*)dhcp_packet);

        memory_free(dhcp_packet);

        if(udp == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "udp packet is null");

            return NULL;
        }

        network_ipv4_header_t* ip = network_ipv4_create_packet_from_udp_packet(NETWORK_IPV4_ZERO_IP, NETWORK_IPV4_GLOBAL_BROADCAST_IP, udp);

        if(ip == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "ip packet is null");

            return NULL;
        }

        uint16_t tl = BYTE_SWAP16(ip->total_length);

        uint8_t* eth = (uint8_t*)network_ethernet_create_packet(BROADCAST_MAC, network_info, NETWORK_PROTOCOL_IPV4, tl, (uint8_t*)ip);

        network_transmit_packet_t* res = memory_malloc_ext(linkedlist_get_heap(ni->return_queue), sizeof(network_transmit_packet_t), 0);

        if(res == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "network transmit packet is null, re trying...");

            memory_free(eth);

            return NULL;
        }

        res->packet_len = sizeof(network_ethernet_t) + tl;

        uint8_t* packet_data = memory_malloc_ext(linkedlist_get_heap(ni->return_queue), res->packet_len, 0);

        if(packet_data == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "packet data allocation failed, re trying...");

            memory_free(eth);
            memory_free_ext(linkedlist_get_heap(ni->return_queue), res);

            return NULL;
        }

        memory_memcopy(eth, packet_data, res->packet_len);

        memory_free(eth);

        res->packet_data = packet_data;

        linkedlist_queue_push(ni->return_queue, res);

        PRINTLOG(NETWORK, LOG_TRACE, "dhcp request is send");
    } else if(type == NETWORK_DHCPV4_OPCODE_ACK) {
        PRINTLOG(NETWORK, LOG_TRACE, "dhcp type is ack");

        memory_memcopy(recv_dhcpv4_packet->your_ip, ni->ipv4_address, sizeof(network_ipv4_address_t));
        PRINTLOG(NETWORK, LOG_TRACE, "ipaddr %i.%i.%i.%i", ni->ipv4_address[0], ni->ipv4_address[1], ni->ipv4_address[2], ni->ipv4_address[3]);
        memory_memcopy(recv_dhcpv4_packet->server_ip, ni->ipv4_dhcpserver, sizeof(network_ipv4_address_t));

        idx = 0;

        while(options[idx] != NETWORK_DHCPV4_OPTION_END) {
            if(options[idx] == NETWORK_DHCPV4_OPTION_SUBNETMASK) {
                memory_memcopy(options + idx + 2, ni->ipv4_subnetmask, sizeof(network_ipv4_address_t));
                PRINTLOG(NETWORK, LOG_TRACE, "subnet %i.%i.%i.%i", ni->ipv4_subnetmask[0], ni->ipv4_subnetmask[1], ni->ipv4_subnetmask[2], ni->ipv4_subnetmask[3]);
            } else if(options[idx] == NETWORK_DHCPV4_OPTION_ROUTER) {
                memory_memcopy(options + idx + 2, ni->ipv4_gateway, sizeof(network_ipv4_address_t));
                PRINTLOG(NETWORK, LOG_TRACE, "gw %i.%i.%i.%i", ni->ipv4_gateway[0], ni->ipv4_gateway[1], ni->ipv4_gateway[2], ni->ipv4_gateway[3]);
            } else if(options[idx] == NETWORK_DHCPV4_OPTION_DOMAINNAMESERVER) {
                memory_memcopy(options + idx + 2, ni->ipv4_nameserver, sizeof(network_ipv4_address_t));
                PRINTLOG(NETWORK, LOG_TRACE, "ns %i.%i.%i.%i", ni->ipv4_nameserver[0], ni->ipv4_nameserver[1], ni->ipv4_nameserver[2], ni->ipv4_nameserver[3]);
            } else if(options[idx] == NETWORK_DHCPV4_OPTION_BROADCAST) {
                memory_memcopy(options + idx + 2, ni->ipv4_broadcast, sizeof(network_ipv4_address_t));
                PRINTLOG(NETWORK, LOG_TRACE, "bcast %i.%i.%i.%i", ni->ipv4_broadcast[0], ni->ipv4_broadcast[1], ni->ipv4_broadcast[2], ni->ipv4_broadcast[3]);
            } else if(options[idx] == NETWORK_DHCPV4_OPTION_IPLEASETIME) {
                ni->lease_time = (uint32_t)*((uint32_t*)(options + idx + 2));
                ni->lease_time = BYTE_SWAP32(ni->lease_time);
                PRINTLOG(NETWORK, LOG_TRACE, "lease time %lli", ni->lease_time);
            } else if(options[idx] == NETWORK_DHCPV4_OPTION_RENEWALTIME) {
                ni->renewal_time = (uint32_t)*((uint32_t*)(options + idx + 2));
                ni->renewal_time = BYTE_SWAP32(ni->renewal_time);
                PRINTLOG(NETWORK, LOG_TRACE, "lease time %lli", ni->renewal_time);
            } else if(options[idx] == NETWORK_DHCPV4_OPTION_REBINDTIME) {
                ni->rebind_time = (uint32_t)*((uint32_t*)(options + idx + 2));
                ni->rebind_time = BYTE_SWAP32(ni->rebind_time);
                PRINTLOG(NETWORK, LOG_TRACE, "lease time %lli", ni->rebind_time);
            }

            idx += options[idx + 1] + 2;
        }

        if(ni->renewal_time == 0) {
            ni->renewal_time = ni->lease_time >> 1;
        }

        ni->is_ipv4_address_set = true;
        ni->is_ipv4_address_requested = false;

        PRINTLOG(NETWORK, LOG_TRACE, "dhcp conf completed");
    }

    return NULL;
}
#pragma GCC diagnostic pop
