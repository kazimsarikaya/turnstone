/**
 * @file network_dhcpv4.64.c
 * @brief DHCPv4 protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_connection.h>
#include <network/network_utils.h>
#include <network.h>
#include <logging.h>
#include <random.h>
#include <memory.h>
#include <utils.h>
#include <time/timer.h>
#include <strings.h>
#include <cpu/task.h>

MODULE("turnstone.lib.network");

typedef enum network_dhcpv4_options_t {
    NETWORK_DHCPV4_OPTION_PAD                    = 0,
    NETWORK_DHCPV4_OPTION_SUBNETMASK             = 1,
    NETWORK_DHCPV4_OPTION_TIMEOFFSET             = 2,
    NETWORK_DHCPV4_OPTION_ROUTER                 = 3,
    NETWORK_DHCPV4_OPTION_TIMESERVER             = 4,
    NETWORK_DHCPV4_OPTION_NAMESERVER             = 5,
    NETWORK_DHCPV4_OPTION_DOMAINNAMESERVER       = 6,
    NETWORK_DHCPV4_OPTION_LOGSERVER              = 7,
    NETWORK_DHCPV4_OPTION_COOKIESERVER           = 8,
    NETWORK_DHCPV4_OPTION_LPRSERVER              = 9,
    NETWORK_DHCPV4_OPTION_IMPRESSSERVER          = 10,
    NETWORK_DHCPV4_OPTION_RESOURCELOCATIONSERVER = 11,
    NETWORK_DHCPV4_OPTION_HOSTNAME               = 12,
    NETWORK_DHCPV4_OPTION_BOOTFILESIZE           = 13,
    NETWORK_DHCPV4_OPTION_MERITDUMPFILE          = 14,
    NETWORK_DHCPV4_OPTION_DOMAINNAME             = 15,
    NETWORK_DHCPV4_OPTION_SWAPSERVER             = 16,
    NETWORK_DHCPV4_OPTION_ROOTPATH               = 17,
    NETWORK_DHCPV4_OPTION_EXTENSIONPATH          = 18,
    NETWORK_DHCPV4_OPTION_BROADCAST              = 28,
    NETWORK_DHCPV4_OPTION_REQUESTIP              = 50,
    NETWORK_DHCPV4_OPTION_IPLEASETIME            = 51,
    NETWORK_DHCPV4_OPTION_MESSAGETYPE            = 53,
    NETWORK_DHCPV4_OPTION_SERVERIP               = 54,
    NETWORK_DHCPV4_OPTION_PARAMETER_REQUEST_LIST = 55,
    NETWORK_DHCPV4_OPTION_RENEWALTIME            = 58,
    NETWORK_DHCPV4_OPTION_REBINDTIME             = 59,
    NETWORK_DHCPV4_OPTION_END                    = 255,
} network_dhcpv4_options_t;

typedef enum network_dhcpv4_opcode_t : uint8_t {
    NETWORK_DHCPV4_OPCODE_DISCOVER   = 1,
    NETWORK_DHCPV4_OPCODE_OFFER      = 2,
    NETWORK_DHCPV4_OPCODE_REQUEST    = 3,
    NETWORK_DHCPV4_OPCODE_DECLINE    = 4,
    NETWORK_DHCPV4_OPCODE_ACK        = 5,
    NETWORK_DHCPV4_OPCODE_NACK       = 6,
    NETWORK_DHCPV4_OPCODE_RELEASE    = 7,
    NETWORK_DHCPV4_OPCODE_INFORM     = 8,
    NETWORK_DHCPV4_OPCODE_FORCERENEW = 9,
} network_dhcpv4_opcode_t;

typedef enum network_dhcpv4_flags_t : uint16_t {
    NETWORK_DHCPV4_FLAG_BROADCAST = 0x8000,
} network_dhcpv4_flags_t;

#define NETWORK_DHCPV4_HTYPE 0x01
#define NETWORK_DHCPV4_HLEN 0x06
#define NETWORK_DHCPV4_HOPS 0x00

#define NETWORK_DHCPV4_MAGICCOOKIE 0x63825363

#define NETWORK_DHCPV4_SOURCE_PORT 68
#define NETWORK_DHCPV4_DESTINATION_PORT 67

typedef struct network_dhcpv4_header_t {
    network_dhcpv4_opcode_t opcode;
    uint8_t hardware_type;
    uint8_t hardware_address_length;
    uint8_t hops;
    uint32_t xid;
    uint16_t seconds;
    uint16_t flags;
    network_ipv4_address_t client_ip;
    network_ipv4_address_t your_ip;
    network_ipv4_address_t server_ip;
    network_ipv4_address_t relay_ip;
    network_mac_address_t client_mac_address;
    uint8_t padding[10]; ///< wtf?
    uint8_t sname[64];
    uint8_t bootp_file[128];
    uint32_t magic_cookie;
} __attribute((packed)) network_dhcpv4_header_t;

_Static_assert(sizeof(network_dhcpv4_header_t) == 240, "invalid network_dhcpv4_t size");

typedef struct network_dhcpv4_t {
    network_dhcpv4_header_t header;
    uint8_t options[1024];
} __attribute((packed)) network_dhcpv4_t;

static int32_t network_dhcpv4_create_discover_packet(network_mac_address_t mac, uint32_t xid, network_dhcpv4_t* dhcp_discover) {
    dhcp_discover->header.opcode                  = NETWORK_DHCPV4_OPCODE_DISCOVER;
    dhcp_discover->header.hardware_type           = NETWORK_DHCPV4_HTYPE;
    dhcp_discover->header.hardware_address_length = NETWORK_DHCPV4_HLEN;
    dhcp_discover->header.hops                    = NETWORK_DHCPV4_HOPS;
    dhcp_discover->header.xid                     = xid;
    // dhcp_discover->header.flags                   = BYTE_SWAP16(NETWORK_DHCPV4_FLAG_BROADCAST);

    memory_memcopy(mac, dhcp_discover->header.client_mac_address, sizeof(network_mac_address_t));

    dhcp_discover->header.magic_cookie = BYTE_SWAP32(NETWORK_DHCPV4_MAGICCOOKIE);

    dhcp_discover->options[0] = NETWORK_DHCPV4_OPTION_MESSAGETYPE;
    dhcp_discover->options[1] = 1;
    dhcp_discover->options[2] = NETWORK_DHCPV4_OPCODE_DISCOVER;

    dhcp_discover->options[3] = NETWORK_DHCPV4_OPTION_PARAMETER_REQUEST_LIST;
    dhcp_discover->options[4] = 3;
    dhcp_discover->options[5] = NETWORK_DHCPV4_OPTION_SUBNETMASK;
    dhcp_discover->options[6] = NETWORK_DHCPV4_OPTION_ROUTER;
    dhcp_discover->options[7] = NETWORK_DHCPV4_OPTION_DOMAINNAMESERVER;

    dhcp_discover->options[8] = NETWORK_DHCPV4_OPTION_END;

    return 9;
}

static int32_t network_dhcpv4_create_request_packet(network_info_t* ni, uint32_t xid, network_dhcpv4_t* dhcp_discover) {
    dhcp_discover->header.opcode                  = NETWORK_DHCPV4_OPCODE_DISCOVER;
    dhcp_discover->header.hardware_type           = NETWORK_DHCPV4_HTYPE;
    dhcp_discover->header.hardware_address_length = NETWORK_DHCPV4_HLEN;
    dhcp_discover->header.hops                    = NETWORK_DHCPV4_HOPS;
    dhcp_discover->header.xid                     = xid;

    dhcp_discover->header.server_ip = ni->ipv4_dhcpserver;
    memory_memcopy(ni->mac, dhcp_discover->header.client_mac_address, sizeof(network_mac_address_t));

    dhcp_discover->header.magic_cookie = BYTE_SWAP32(NETWORK_DHCPV4_MAGICCOOKIE);

    dhcp_discover->options[0] = NETWORK_DHCPV4_OPTION_MESSAGETYPE;
    dhcp_discover->options[1] = 1;
    dhcp_discover->options[2] = NETWORK_DHCPV4_OPCODE_REQUEST;

    dhcp_discover->options[3] = NETWORK_DHCPV4_OPTION_REQUESTIP;
    dhcp_discover->options[4] = 4;
    memory_memcopy(ni->ipv4_address.as_bytes, &dhcp_discover->options[5], sizeof(network_ipv4_address_t));


    dhcp_discover->options[9]  = NETWORK_DHCPV4_OPTION_SERVERIP;
    dhcp_discover->options[10] = 4;
    memory_memcopy(ni->ipv4_dhcpserver.as_bytes, &dhcp_discover->options[11], sizeof(network_ipv4_address_t));

    dhcp_discover->options[15] = NETWORK_DHCPV4_OPTION_END;

    return 16;
}

static int32_t network_dhcpv4_process_packet(network_info_t* ni, network_dhcpv4_t* recv_dhcpv4_packet, network_dhcpv4_t* send_dhcpv4_packet) {
    if(!network_ethernet_is_mac_address_eq(ni->mac, recv_dhcpv4_packet->header.client_mac_address)) {
        PRINTLOG(NETWORK, LOG_TRACE, "dhcp packet is not for us");
        return -1;
    }

    if(BYTE_SWAP32(recv_dhcpv4_packet->header.magic_cookie) != NETWORK_DHCPV4_MAGICCOOKIE) {
        PRINTLOG(NETWORK, LOG_ERROR, "dhcp magic cookie is invalid");
        return -1;
    }

    if(recv_dhcpv4_packet->header.opcode != NETWORK_DHCPV4_OPCODE_OFFER &&
       recv_dhcpv4_packet->header.opcode != NETWORK_DHCPV4_OPCODE_ACK) {
        PRINTLOG(NETWORK, LOG_ERROR, "dhcp packet is not offer or ack");
        return -1;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "dhcp packet received");

    network_dhcpv4_opcode_t type = 0;

    network_ipv4_address_t server_ip   = {0};
    boolean_t server_is_is_from_option = false;

    uint8_t* options = recv_dhcpv4_packet->options;
    uint16_t idx     = 0;

    while(options[idx] != NETWORK_DHCPV4_OPTION_END) {
        if(options[idx] == NETWORK_DHCPV4_OPTION_MESSAGETYPE) {
            type = options[idx + 2];
        } else if(options[idx] == NETWORK_DHCPV4_OPTION_SERVERIP) {
            memory_memcopy(&options[idx + 2], server_ip.as_bytes, sizeof(network_ipv4_address_t));
            PRINTLOG(NETWORK, LOG_TRACE, "dhcp server ip at option %i.%i.%i.%i",
                     server_ip.as_bytes[0],
                     server_ip.as_bytes[1],
                     server_ip.as_bytes[2],
                     server_ip.as_bytes[3]);
            server_is_is_from_option = true;
        }

        idx += options[idx + 1] + 2;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "dhcp packet type %i", type);

    if(type == NETWORK_DHCPV4_OPCODE_OFFER) {
        PRINTLOG(NETWORK, LOG_INFO, "dhcp offer recevied");
        ni->ipv4_address = recv_dhcpv4_packet->header.your_ip;
        PRINTLOG(NETWORK, LOG_INFO, "offered ipaddr %i.%i.%i.%i",
                 ni->ipv4_address.as_bytes[0],
                 ni->ipv4_address.as_bytes[1],
                 ni->ipv4_address.as_bytes[2],
                 ni->ipv4_address.as_bytes[3]);
        if(server_is_is_from_option) {
            ni->ipv4_dhcpserver = server_ip;
        } else {
            ni->ipv4_dhcpserver = recv_dhcpv4_packet->header.server_ip;
        }
        PRINTLOG(NETWORK, LOG_INFO, "by dhcp server ipaddr %i.%i.%i.%i",
                 ni->ipv4_dhcpserver.as_bytes[0],
                 ni->ipv4_dhcpserver.as_bytes[1],
                 ni->ipv4_dhcpserver.as_bytes[2],
                 ni->ipv4_dhcpserver.as_bytes[3]);

        return network_dhcpv4_create_request_packet(ni, recv_dhcpv4_packet->header.xid, send_dhcpv4_packet);
    }

    PRINTLOG(NETWORK, LOG_INFO, "dhcp ack recevied");

    ni->ipv4_address = recv_dhcpv4_packet->header.your_ip;
    PRINTLOG(NETWORK, LOG_INFO, "ipaddr %i.%i.%i.%i",
             ni->ipv4_address.as_bytes[0],
             ni->ipv4_address.as_bytes[1],
             ni->ipv4_address.as_bytes[2],
             ni->ipv4_address.as_bytes[3]);
    if(server_is_is_from_option) {
        ni->ipv4_dhcpserver = server_ip;
    } else {
        ni->ipv4_dhcpserver = recv_dhcpv4_packet->header.server_ip;
    }
    PRINTLOG(NETWORK, LOG_INFO, "by dhcp server ipaddr %i.%i.%i.%i",
             ni->ipv4_dhcpserver.as_bytes[0],
             ni->ipv4_dhcpserver.as_bytes[1],
             ni->ipv4_dhcpserver.as_bytes[2],
             ni->ipv4_dhcpserver.as_bytes[3]);

    idx = 0;

    while(options[idx] != NETWORK_DHCPV4_OPTION_END) {
        if(options[idx] == NETWORK_DHCPV4_OPTION_SUBNETMASK) {
            memory_memcopy(options + idx + 2, ni->ipv4_subnetmask.as_bytes, sizeof(network_ipv4_address_t));
            PRINTLOG(NETWORK, LOG_INFO, "subnet %i.%i.%i.%i",
                     ni->ipv4_subnetmask.as_bytes[0],
                     ni->ipv4_subnetmask.as_bytes[1],
                     ni->ipv4_subnetmask.as_bytes[2],
                     ni->ipv4_subnetmask.as_bytes[3]);
        } else if(options[idx] == NETWORK_DHCPV4_OPTION_ROUTER) {
            memory_memcopy(options + idx + 2, ni->ipv4_gateway.as_bytes, sizeof(network_ipv4_address_t));
            PRINTLOG(NETWORK, LOG_INFO, "gw %i.%i.%i.%i",
                     ni->ipv4_gateway.as_bytes[0],
                     ni->ipv4_gateway.as_bytes[1],
                     ni->ipv4_gateway.as_bytes[2],
                     ni->ipv4_gateway.as_bytes[3]);
        } else if(options[idx] == NETWORK_DHCPV4_OPTION_DOMAINNAMESERVER) {
            memory_memcopy(options + idx + 2, ni->ipv4_nameserver.as_bytes, sizeof(network_ipv4_address_t));
            PRINTLOG(NETWORK, LOG_INFO, "ns %i.%i.%i.%i",
                     ni->ipv4_nameserver.as_bytes[0],
                     ni->ipv4_nameserver.as_bytes[1],
                     ni->ipv4_nameserver.as_bytes[2],
                     ni->ipv4_nameserver.as_bytes[3]);
        } else if(options[idx] == NETWORK_DHCPV4_OPTION_BROADCAST) {
            memory_memcopy(options + idx + 2, ni->ipv4_broadcast.as_bytes, sizeof(network_ipv4_address_t));
            PRINTLOG(NETWORK, LOG_INFO, "bcast %i.%i.%i.%i",
                     ni->ipv4_broadcast.as_bytes[0],
                     ni->ipv4_broadcast.as_bytes[1],
                     ni->ipv4_broadcast.as_bytes[2],
                     ni->ipv4_broadcast.as_bytes[3]);
        } else if(options[idx] == NETWORK_DHCPV4_OPTION_IPLEASETIME) {
            ni->lease_time = (uint32_t)*((uint32_t*)(void*)(options + idx + 2));
            ni->lease_time = BYTE_SWAP32(ni->lease_time);
            PRINTLOG(NETWORK, LOG_TRACE, "lease time %lli", ni->lease_time);
        } else if(options[idx] == NETWORK_DHCPV4_OPTION_RENEWALTIME) {
            ni->renewal_time = (uint32_t)*((uint32_t*)(void*)(options + idx + 2));
            ni->renewal_time = BYTE_SWAP32(ni->renewal_time);
            PRINTLOG(NETWORK, LOG_TRACE, "lease time %lli", ni->renewal_time);
        } else if(options[idx] == NETWORK_DHCPV4_OPTION_REBINDTIME) {
            ni->rebind_time = (uint32_t)*((uint32_t*)(void*)(options + idx + 2));
            ni->rebind_time = BYTE_SWAP32(ni->rebind_time);
            PRINTLOG(NETWORK, LOG_TRACE, "lease time %lli", ni->rebind_time);
        }

        idx += options[idx + 1] + 2;
    }

    if(ni->renewal_time == 0) {
        ni->renewal_time = ni->lease_time >> 1;
    }

    ni->is_ipv4_address_set       = true;
    ni->is_ipv4_address_requested = false;

    if(network_arp_query(ni, ni->ipv4_gateway) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to query arp for gateway ip %i.%i.%i.%i",
                 ni->ipv4_gateway.as_bytes[0],
                 ni->ipv4_gateway.as_bytes[1],
                 ni->ipv4_gateway.as_bytes[2],
                 ni->ipv4_gateway.as_bytes[3]);
    }

    if(!network_ipv4_is_address_eq(ni->ipv4_dhcpserver, ni->ipv4_gateway)) {
        if(network_arp_query(ni, ni->ipv4_dhcpserver) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to query arp for dhcp server ip %i.%i.%i.%i",
                     ni->ipv4_dhcpserver.as_bytes[0],
                     ni->ipv4_dhcpserver.as_bytes[1],
                     ni->ipv4_dhcpserver.as_bytes[2],
                     ni->ipv4_dhcpserver.as_bytes[3]);
        }
    }

    if(!network_ipv4_is_address_eq(ni->ipv4_nameserver, ni->ipv4_gateway) &&
       !network_ipv4_is_address_eq(ni->ipv4_nameserver, ni->ipv4_dhcpserver)) {
        if(network_arp_query(ni, ni->ipv4_nameserver) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to query arp for nameserver ip %i.%i.%i.%i",
                     ni->ipv4_nameserver.as_bytes[0],
                     ni->ipv4_nameserver.as_bytes[1],
                     ni->ipv4_nameserver.as_bytes[2],
                     ni->ipv4_nameserver.as_bytes[3]);
        }
    }

    PRINTLOG(NETWORK, LOG_INFO, "dhcp conf completed");

    return 0;
}

static int32_t network_dhcpv4_create_dhcpv4_discover_or_request_packet(network_info_t* ni, network_dhcpv4_t* dhcp_packet) {
    uint32_t xid = rand();

    PRINTLOG(NETWORK, LOG_INFO, "creating dhcp %s for mac %02x:%02x:%02x:%02x:%02x:%02x",
             ni->is_ipv4_address_set ? "request" : "discover",
             ni->mac[0], ni->mac[1], ni->mac[2],
             ni->mac[3], ni->mac[4], ni->mac[5]);

    if(ni->is_ipv4_address_set) {
        return network_dhcpv4_create_request_packet(ni, xid, dhcp_packet);
    }

    return network_dhcpv4_create_discover_packet(ni->mac, xid, dhcp_packet);
}

static int8_t network_dhcpv4_send_dhcpv4_discover_or_request_packet(network_info_t* ni) {
    network_ipv4_address_t bind_ip      = ni->is_ipv4_address_set ? ni->ipv4_address : NETWORK_IPV4_ZERO_IP;
    network_ipv4_address_t broadcast_ip = ni->is_ipv4_address_set ? ni->ipv4_dhcpserver : NETWORK_IPV4_GLOBAL_BROADCAST_IP;

    int8_t err = -1;

    network_listener_t* listener = network_listener_create_udpv4_client(ni, bind_ip, NETWORK_DHCPV4_SOURCE_PORT);

    if(!listener) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create dhcp udp listener for mac %02x:%02x:%02x:%02x:%02x:%02x",
                 ni->mac[0], ni->mac[1], ni->mac[2], ni->mac[3], ni->mac[4], ni->mac[5]);
        return -1;
    }


    network_connection_t* connection = network_connection_connect(listener, broadcast_ip, NETWORK_DHCPV4_DESTINATION_PORT);

    if(!connection) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create dhcp udp connection for mac %02x:%02x:%02x:%02x:%02x:%02x",
                 ni->mac[0], ni->mac[1], ni->mac[2], ni->mac[3], ni->mac[4], ni->mac[5]);
        network_listener_destroy(listener);
        return -1;
    }

    network_dhcpv4_t dhcp_packet;
    memory_memclean(&dhcp_packet, sizeof(network_dhcpv4_t));
    int32_t options_length = network_dhcpv4_create_dhcpv4_discover_or_request_packet(ni, &dhcp_packet);
    int32_t packet_length  = sizeof(network_dhcpv4_header_t) + options_length;

    if(network_connection_send(connection, (uint8_t*)&dhcp_packet, packet_length) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send dhcp discover/request packet");
        goto cleanup;
    }

    ni->is_ipv4_address_requested = true;

    memory_memclean(&dhcp_packet, sizeof(network_dhcpv4_t));

    int32_t recv_len = network_connection_receive(connection, (uint8_t*)&dhcp_packet, sizeof(network_dhcpv4_t));

    if(recv_len < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to receive dhcp offer/ack packet: %i", recv_len);
        goto cleanup;
    }

    network_dhcpv4_t dhcp_offer_or_ack_packet;
    memory_memclean(&dhcp_offer_or_ack_packet, sizeof(network_dhcpv4_t));
    int32_t res = network_dhcpv4_process_packet(ni, &dhcp_packet, &dhcp_offer_or_ack_packet);

    if(res < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to process dhcp offer/ack packet");
        goto cleanup;
    }

    if(res == 0) {
        PRINTLOG(NETWORK, LOG_INFO, "dhcp configuration completed.");
        err = 0;
        goto cleanup;
    }

    if(network_connection_send(connection, (uint8_t*)&dhcp_offer_or_ack_packet, sizeof(network_dhcpv4_header_t) + res) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send dhcp request packet");
        goto cleanup;
    }

    recv_len = network_connection_receive(connection, (uint8_t*)&dhcp_packet, sizeof(network_dhcpv4_t));

    if(recv_len < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to receive dhcp ack packet");
        goto cleanup;
    }

    res = network_dhcpv4_process_packet(ni, &dhcp_packet, &dhcp_offer_or_ack_packet);

    if(res < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to process dhcp ack packet");
        goto cleanup;
    }

    err = 0;

cleanup:
    network_connection_close(connection);
    network_connection_destroy(connection);
    network_listener_destroy(listener);

    return err;
}

static void network_dhcpv4_wait_for_renewal(network_info_t* ni) {
    while(ni->lease_time > ni->renewal_time) {
        PRINTLOG(NETWORK, LOG_INFO, "dhcp lease time left %lli seconds, sleeping %lli seconds",
                 ni->lease_time,
                 ni->renewal_time >> 1);
        ni->lease_time -= ni->renewal_time >> 1;
        task_sleep(ni->renewal_time >> 1);
    }

    PRINTLOG(NETWORK, LOG_INFO, "dhcp lease time expired, requesting again");
}

int32_t network_dhcpv4_send_discover(uint64_t args_cnt, void** args) {
    UNUSED(args_cnt);

    network_mac_address_t mac = {};
    uint8_t* mac_data         = args[0];
    memory_memcopy(mac_data, mac, sizeof(network_mac_address_t));

    PRINTLOG(NETWORK, LOG_INFO, "dhcp task started for mac %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    network_info_t* ni = network_get_network_info(mac);

    if(!ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info not found for mac %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return -1;
    }

    int32_t retry_sleep = 5;
    int32_t backoff     = 1;

    boolean_t failed = false;

    while(true) {

        if(ni->is_ipv4_address_set && !ni->is_ipv4_address_requested) {
            network_dhcpv4_wait_for_renewal(ni);
        }

        if (failed || ni->is_ipv4_address_requested) {
            int32_t sleep_time = retry_sleep * backoff;

            if(sleep_time >= 300) {
                sleep_time = 300;
                backoff    = 1;
            } else {
                backoff <<= 1;
            }

            task_sleep(sleep_time);
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
