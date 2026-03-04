/**
 * @file network_connection.64.c
 * @brief Network connection implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_connection.h>
#include <network/network_packet.h>
#include <network/network_utils.h>
#include <network/network_filter.h>
#include <network.h>
#include <hashmap.h>
#include <memory.h>
#include <logging.h>
#include <utils.h>
#include <random.h>
#include <pipeline.h>
#include <cpu/task.h>
#include <time.h>
#include <list.h>
#include <cpu/sync.h>

MODULE("turnstone.lib.network");

extern memory_heap_t* network_packet_heap;

typedef struct network_listener_t {
    network_listener_role_t role;
    network_protocol_t      protocol;
    network_ipv4_protocol_t ipv4_protocol;
    const network_info_t*   network_info;
    network_ipv4_address_t  local_ip;
    uint16_t                local_port;
    hashmap_t*              connections;

    // for ping listener, for simplicity, we store last sequence number and timestamp to generate echo reply packets.
    int32_t  icmpv4_last_echo_request_sequence;
    uint64_t icmpv4_last_echo_request_timestamp;
    uint64_t icmpv4_total_round_trip_time;
    uint16_t icmpv4_lost_echo_replies;

    // for server listener, we can store backlog and other information if needed in the future.
    // if a connection come it also added to backlog for accepting. the connection also stored
    // in connections hashmap during this period for fast access.
    // when connection is accepted, it is removed from backlog but still stored in connections hashmap.
    list_t* backlog;
} network_listener_t;

typedef enum network_connection_state_t {
    NETWORK_CONNECTION_STATE_CLOSED,
    NETWORK_CONNECTION_STATE_LISTEN, // pratically we don't need this state.
    NETWORK_CONNECTION_STATE_SYN_SENT,
    NETWORK_CONNECTION_STATE_SYN_ACK_SENT,
    NETWORK_CONNECTION_STATE_SYN_RECEIVED, // pratically we don't need this state.
    NETWORK_CONNECTION_STATE_ESTABLISHED,
    NETWORK_CONNECTION_STATE_FIN_WAIT_1,
    NETWORK_CONNECTION_STATE_FIN_WAIT_2,
    NETWORK_CONNECTION_STATE_CLOSE_WAIT,
    NETWORK_CONNECTION_STATE_CLOSING,
    NETWORK_CONNECTION_STATE_LAST_ACK,
    NETWORK_CONNECTION_STATE_TIME_WAIT,
} network_connection_state_t;

typedef struct network_connection_t {
    network_listener_t*    listener;
    network_ipv4_address_t remote_ip;
    uint16_t               remote_port;

    time_t last_remote_activity_timestamp; // for timeout handling

    network_mac_address_t remote_mac;

    pipeline_t* read_pipeline;

    network_connection_state_t state;

    lock_t* lock; // for synchronizing access to connection state and other fields if needed in the future.

    // sequence and acknowledgement numbers are used for TCP connections
    uint32_t local_sequence_number;
    uint32_t remote_sequence_number;
    uint8_t  local_window_scale;
    uint8_t  remote_window_scale;
    uint32_t local_window_size;
    uint32_t remote_window_size;
    uint16_t mss;
} network_connection_t;

typedef struct network_arp_cache_entry_t {
    const network_info_t*  network_info;
    network_ipv4_address_t ip;
    network_mac_address_t  mac;
    uint64_t               timestamp;
} network_arp_cache_entry_t;


hashmap_t* network_listener_map = NULL;
hashmap_t* network_arp_cache    = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static void network_arp_cache_update(const network_info_t* ni, network_ipv4_address_t ip, network_mac_address_t mac) {
    UNUSED(ni);

    if(network_arp_cache == NULL) {
        network_arp_cache = hashmap_integer_with_heap(network_packet_heap, 128);
        if(network_arp_cache == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to create ARP cache");
            return;
        }
    }

    if(!ni->is_ipv4_address_set) {
        PRINTLOG(NETWORK, LOG_TRACE, "ignoring ARP cache update for ip %i.%i.%i.%i as network interface does not have an ipv4 address",
                 ip.as_bytes[0], ip.as_bytes[1], ip.as_bytes[2], ip.as_bytes[3]);
        return;
    }

    // if ip is not in same subnet, ignore it.
    if(!network_ipv4_is_address_in_same_subnet(ip, ni->ipv4_address, ni->ipv4_subnetmask)) {
        PRINTLOG(NETWORK, LOG_TRACE, "ignoring ARP cache update for ip %i.%i.%i.%i as it is not in same subnet",
                 ip.as_bytes[0], ip.as_bytes[1], ip.as_bytes[2], ip.as_bytes[3]);
        return;
    }

    network_arp_cache_entry_t* existing_entry = (network_arp_cache_entry_t*)hashmap_get(network_arp_cache, (void*)(uintptr_t)ip.as_dword);

    if(existing_entry) {
        memory_memcopy(mac, existing_entry->mac, sizeof(network_mac_address_t));
        existing_entry->timestamp = time_ns(NULL);
        return;
    }

    network_arp_cache_entry_t* entry = memory_malloc_ext(network_packet_heap, sizeof(network_arp_cache_entry_t), 0);
    if(entry == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to allocate memory for ARP cache entry");
        return;
    }

    entry->network_info = ni;
    entry->ip           = ip;
    memory_memcopy(mac, entry->mac, sizeof(network_mac_address_t));
    entry->timestamp = time_ns(NULL);

    hashmap_put(network_arp_cache, (void*)(uintptr_t)ip.as_dword, entry);
}
#pragma GCC diagnostic pop

static void network_arp_cache_cleanup(void) {
    if(network_arp_cache == NULL) {
        return;
    }

    uint64_t now = time_ns(NULL);

    iterator_t* it = hashmap_iterator_create(network_arp_cache);
    if(it) {
        while(it->end_of_iterator(it) != 0) {
            network_arp_cache_entry_t* entry = (network_arp_cache_entry_t*)it->get_item(it);
            if(entry) {
                // remove entries that are older than 5 minutes
                if(now - entry->timestamp > 5 * 60 * 1000000000ULL) {
                    it->delete_item(it);
                    memory_free_ext(network_packet_heap, entry);
                }
            }
            it = it->next(it);
        }
        it->destroy(it);
    }
}

static network_arp_cache_entry_t* network_arp_cache_get(const network_info_t* ni, network_ipv4_address_t ip) {
    UNUSED(ni);

    if(network_arp_cache == NULL) {
        return NULL;
    }

    network_arp_cache_cleanup();

    return (network_arp_cache_entry_t*)hashmap_get(network_arp_cache, (void*)(uintptr_t)ip.as_dword);
}

void network_arp_cache_wait_and_get(const network_info_t* ni, network_ipv4_address_t ip, network_mac_address_t mac) {
    // if ip is not set, return broadcast mac.
    if(!ni->is_ipv4_address_set) {
        memory_memcopy(BROADCAST_MAC, mac, sizeof(network_mac_address_t));
        return;
    }

    // if ip is broadcast.
    if(network_ipv4_is_address_eq(ip, NETWORK_IPV4_GLOBAL_BROADCAST_IP)) {
        memory_memcopy(BROADCAST_MAC, mac, sizeof(network_mac_address_t));
        return;
    }

    network_ipv4_address_t target_ip = ip;

    // if ip is not in same subnet, use gateway ip instead.
    if(!network_ipv4_is_address_in_same_subnet(ip, ni->ipv4_address, ni->ipv4_subnetmask)) {
        target_ip = ni->ipv4_gateway;
    }

    while(true) {
        network_arp_cache_entry_t* entry = network_arp_cache_get(ni, target_ip);
        if(entry) {
            memory_memcopy(entry->mac, mac, sizeof(network_mac_address_t));
            return;
        } else {
            network_arp_query(ni, target_ip);
        }

        task_msleep(10);
    }
}

static int8_t network_listener_ensure_map(void) {
    if(network_listener_map) {
        return 0;
    }

    network_listener_map = hashmap_integer_with_heap(network_packet_heap, 128);

    if(!network_listener_map) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create network listener map");
        return -1;
    }

    return 0;
}

network_protocol_t network_listener_get_protocol(const network_listener_t* listener) {
    if(!listener) {
        return 0;
    }

    return listener->protocol;
}

network_ipv4_protocol_t network_listener_get_ipv4_protocol(const network_listener_t* listener) {
    if(!listener) {
        return 0;
    }

    return listener->ipv4_protocol;
}

network_ipv4_address_t network_listener_get_local_ip(const network_listener_t* listener) {
    if(!listener) {
        return (network_ipv4_address_t){0};
    }

    return listener->local_ip;
}

uint16_t network_listener_get_local_port(const network_listener_t* listener) {
    if(!listener) {
        return 0;
    }

    return listener->local_port;
}

boolean_t network_listener_exists(network_listener_role_t role,
                                  network_protocol_t      protocol,
                                  network_ipv4_protocol_t ipv4_protocol,
                                  network_ipv4_address_t  local_ip,
                                  uint16_t                local_port) {
    UNUSED(protocol);

    if(network_listener_ensure_map() < 0) {
        return false;
    }

    uint64_t key = ((uint64_t)ipv4_protocol << 48) |
                   ((uint64_t)local_ip.as_dword << 16) |
                   local_port;

    network_listener_t* listener = (network_listener_t*)hashmap_get(network_listener_map, (void*)key);

    if(listener) {
        if(listener->role == role) {
            return true;
        } else {
            return false;
        }
    }

    key = ((uint64_t)ipv4_protocol << 48) |
          ((uint64_t)NETWORK_IPV4_ZERO_IP.as_dword << 16) |
          local_port;

    listener = (network_listener_t*)hashmap_get(network_listener_map, (void*)key);

    if(listener && listener->role == role) {
        return true;
    }

    return false;
}

iterator_t* network_listener_get_all(void) {
    if(network_listener_ensure_map() < 0) {
        return NULL;
    }

    return hashmap_iterator_create(network_listener_map);
}

network_listener_t* network_listener_get_or_create(network_listener_role_t role,
                                                   network_protocol_t      protocol,
                                                   network_ipv4_protocol_t ipv4_protocol,
                                                   const network_info_t*   network_info,
                                                   network_ipv4_address_t  local_ip,
                                                   uint16_t                local_port) {
    if(network_listener_ensure_map() < 0) {
        return NULL;
    }

    if(!network_info) {
        network_info = network_get_network_info_of_owned_ipv4_address(local_ip);

        if(!network_info) {
            PRINTLOG(NETWORK, LOG_ERROR, "network info not found for ip %i.%i.%i.%i",
                     local_ip.as_bytes[0], local_ip.as_bytes[1], local_ip.as_bytes[2], local_ip.as_bytes[3]);
            return NULL;
        }
    }

    // if it is client and local_port is 0, then we need to find an available port
    if(role == NETWORK_LISTENER_ROLE_CLIENT && local_port == 0) {
        // find an available port
        for(uint16_t port = 49152; port < 65535; port++) {
            uint64_t key = ((uint64_t)ipv4_protocol << 48) |
                           ((uint64_t)local_ip.as_dword << 16) |
                           port;

            if(hashmap_get(network_listener_map, (void*)key) == NULL) {
                local_port = port;
                break;
            }
        }

        if(local_port == 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to find an available port for client");
            return NULL;
        }
    }

    uint64_t key = ((uint64_t)ipv4_protocol << 48) |
                   ((uint64_t)local_ip.as_dword << 16) |
                   local_port;

    network_listener_t* listener = (network_listener_t*)hashmap_get(network_listener_map, (void*)key);

    if(listener != NULL) {
        return listener;
    }


    listener = (network_listener_t*)memory_malloc_ext(network_packet_heap, sizeof(network_listener_t), 0);

    if(listener == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to allocate memory for network listener");
        return NULL;
    }


    listener->role          = role;
    listener->protocol      = protocol;
    listener->ipv4_protocol = ipv4_protocol;
    listener->network_info  = network_info;
    listener->local_ip      = local_ip;
    listener->local_port    = local_port;
    listener->connections   = hashmap_integer_with_heap(network_packet_heap, 128);

    if(listener->role == NETWORK_LISTENER_ROLE_SERVER) {
        listener->backlog = list_create_queue_with_heap(network_packet_heap);
        if(listener->backlog == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to create backlog for network listener");
            memory_free_ext(network_packet_heap, listener);
            return NULL;
        }
    }

    hashmap_put(network_listener_map, (void*)key, listener);

    PRINTLOG(NETWORK, LOG_DEBUG, "network listener(0x%llx) created: role=%s, protocol=%s, ipv4_protocol=%s, local_ip=%i.%i.%i.%i, local_port=%i",
             key,
             listener->role == NETWORK_LISTENER_ROLE_SERVER ? "server" : "client",
             listener->protocol == NETWORK_PROTOCOL_IPV4 ? "ipv4" : "unknown",
             listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_TCPV4 ? "tcpv4" :
             (listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_UDPV4 ? "udpv4" : "unknown"),
             listener->local_ip.as_bytes[0], listener->local_ip.as_bytes[1], listener->local_ip.as_bytes[2], listener->local_ip.as_bytes[3],
             listener->local_port);

    return listener;
}

void network_listener_destroy(network_listener_t* listener) {
    if(!listener) {
        return;
    }

    if(listener->backlog) {
        while(list_size(listener->backlog) > 0) {
            list_queue_pop(listener->backlog);
            // connections in backlog are also stored in connections hashmap, so we will free them later when we free connections hashmap.
        }
    }

    if(listener->connections) {
        iterator_t* it = hashmap_iterator_create(listener->connections);
        if(it) {
            while(it->end_of_iterator(it) != 0) {
                network_connection_t* connection = (network_connection_t*)it->get_item(it);
                if(connection) {
                    if(connection->read_pipeline) {
                        pipeline_destroy(connection->read_pipeline);
                    }
                    memory_free_ext(network_packet_heap, connection);
                }
                it = it->next(it);
            }
            it->destroy(it);
        }
        hashmap_destroy(listener->connections);
    }

    uint64_t key = ((uint64_t)listener->ipv4_protocol << 48) |
                   ((uint64_t)listener->local_ip.as_dword << 16) |
                   listener->local_port;

    hashmap_delete(network_listener_map, (void*)key);

    memory_free_ext(network_packet_heap, listener);
}

static network_listener_t* network_listener_get(network_ipv4_protocol_t protocol, network_ipv4_address_t local_ip, uint16_t local_port) {
    if(network_listener_ensure_map() < 0) {
        return NULL;
    }

    uint64_t key = ((uint64_t)protocol << 48) |
                   ((uint64_t)local_ip.as_dword << 16) |
                   local_port;

    network_listener_t* listener = (network_listener_t*)hashmap_get(network_listener_map, (void*)key);

    if(listener != NULL) {
        return listener;
    }

    key = ((uint64_t)protocol << 48) |
          ((uint64_t)NETWORK_IPV4_ZERO_IP.as_dword << 16) |
          local_port;

    return (network_listener_t*)hashmap_get(network_listener_map, (void*)key);
}

network_ipv4_address_t network_connection_get_remote_ip(const network_connection_t* connection) {
    if(!connection) {
        return (network_ipv4_address_t){0};
    }

    return connection->remote_ip;
}

uint16_t network_connection_get_remote_port(const network_connection_t* connection) {
    if(!connection) {
        return 0;
    }

    return connection->remote_port;
}

network_listener_t* network_connection_get_listener(const network_connection_t* connection) {
    if(!connection) {
        return NULL;
    }

    return connection->listener;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t network_send_packet_to_return_queue(const network_info_t* network_info, network_packet_t* packet) {
    if(!network_info || !packet) {
        return -1;
    }

    if(network_filter_apply(network_info, packet, NETWORK_FILTER_DIRECTION_OUTBOUND) == NETWORK_FILTER_ACTION_DROP) {
        PRINTLOG(NETWORK, LOG_ERROR, "packet dropped by outbound filter");
        memory_free_ext(list_get_heap(network_info->return_queue), packet->raw_data);
        return -1;
    }

    list_t* return_queue = network_info->return_queue;

    network_transmit_packet_t* tx_packet = memory_malloc_ext(list_get_heap(return_queue), sizeof(network_transmit_packet_t), 0);

    if(tx_packet == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to allocate memory for transmit packet");
        memory_free_ext(list_get_heap(return_queue), packet->raw_data);
        return -1;
    }

    tx_packet->packet_len     = packet->raw_data_len;
    tx_packet->packet_data    = packet->raw_data;
    tx_packet->is_vlan_tagged = network_info->is_vlan_tagged;
    tx_packet->vlan_id        = network_info->vlan_id;

    if(list_queue_push(return_queue, tx_packet) == -1ULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to push packet to return queue");
        memory_free_ext(list_get_heap(return_queue), tx_packet->packet_data);
        memory_free_ext(list_get_heap(return_queue), tx_packet);
        return -1;
    }

    return 0;
}
#pragma GCC diagnostic pop

static network_connection_t* network_connection_connect_internal(network_listener_t*    listener,
                                                                 network_ipv4_address_t remote_ip,
                                                                 uint16_t               remote_port,
                                                                 network_packet_t*      initial_packet) {
    if(!listener) {
        return NULL;
    }

    if(listener->role == NETWORK_LISTENER_ROLE_SERVER) {
        // for server listener, we only create connection when we receive a packet from remote host,
        // and the connection will be in SYN_RECEIVED or SYN_ACK_SENT state until it is accepted.
        if(initial_packet == NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "initial packet is required for server listener");
            return NULL;
        }
    } else {
        // for client listener, we can create connection directly, and the connection will be in SYN_SENT state until we receive SYN-ACK from remote host.
        if(initial_packet != NULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "initial packet should not be provided for client listener");
            return NULL;
        }
    }

    uint64_t key = ((uint64_t)remote_ip.as_dword << 16) | remote_port;

    network_connection_t* connection = (network_connection_t*)hashmap_get(listener->connections, (void*)key);

    if(connection != NULL) {
        return connection;
    }

    connection = (network_connection_t*)memory_malloc_ext(network_packet_heap, sizeof(network_connection_t), 0);

    if(connection == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to allocate memory for network connection");
        return NULL;
    }

    connection->lock = lock_create_with_heap(network_packet_heap);

    if(!connection->lock) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create lock for network connection");
        memory_free_ext(network_packet_heap, connection);
        return NULL;
    }

    connection->read_pipeline = pipeline_create_with_heap(network_packet_heap, 1024 * 1024); // 1MB buffer for incoming data

    if(connection->read_pipeline == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create read pipeline for network connection");
        memory_free_ext(network_packet_heap, connection);
        return NULL;
    }

    connection->listener               = listener;
    connection->remote_ip              = remote_ip;
    connection->remote_port            = remote_port;
    connection->remote_sequence_number = 0;

    // default mss is MTU minus IPv4 and TCP header sizes, but it can be updated later, if we receive MSS option from remote host during handshake.
    uint16_t mss = listener->network_info->mtu - (NETWORK_IPV4_HEADER_LENGTH + NETWORK_TCPV4_HEADER_LENGTH); // MSS is MTU minus IPv4 and TCP header sizes
    connection->mss = mss;

    network_arp_cache_wait_and_get(listener->network_info, remote_ip, connection->remote_mac);

    if(hashmap_put(listener->connections, (void*)key, connection) != NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to put connection to listener's connections hashmap");
        pipeline_destroy(connection->read_pipeline);
        memory_free_ext(network_packet_heap, connection);
        return NULL;
    }

    // if listener is server, we add connection to backlog too.
    // so we can accept backlog connections.
    if(listener->role == NETWORK_LISTENER_ROLE_SERVER) {
        if(list_queue_push(listener->backlog, connection) == -1ULL) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to push connection to backlog");
            pipeline_destroy(connection->read_pipeline);
            memory_free_ext(network_packet_heap, connection);
            hashmap_delete(listener->connections, (void*)key);
            return NULL;
        }
    }

    if(listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_UDPV4) {
        // For UDP, we can consider the connection established immediately after creating it, as there is no handshake process.
        connection->state = NETWORK_CONNECTION_STATE_ESTABLISHED;

        return connection;
    }

    // now we just work for TCP connection.

    connection->local_sequence_number = rand(); // Initial sequence number can be random
    connection->local_window_scale    = 10;
    connection->local_window_size     = pipeline_available_space(connection->read_pipeline);

    if(initial_packet) {
        // If we have an initial packet (for server listener), we can set the remote sequence number based on the SYN packet received from the client.
        connection->remote_sequence_number = initial_packet->tcpv4_sequence_number + 1;
        // Update MSS based on client's MSS option if present
        connection->mss = MIN(connection->mss, initial_packet->tcpv4_mss);
        // Update remote window size based on client's window size and scale
        connection->remote_window_size  = initial_packet->tcpv4_window_size; // it is raw value, not scaled one.
        connection->remote_window_scale = initial_packet->tcpv4_window_scale;
    }

    // in syn or syn+ack we send raw value. not scaled one.
    uint32_t scaled_window_size = MIN(connection->local_window_size, 0xFFFF);

    // create packet and send to ni return queue
    network_packet_t syn_packet;
    memory_memclean(&syn_packet, sizeof(network_packet_t));

    syn_packet.ether_ethertype       = listener->protocol;
    syn_packet.ether_inner_ethertype = listener->protocol;

    memory_memcopy(listener->network_info->mac, syn_packet.ether_source_mac, sizeof(network_mac_address_t));
    // Destination MAC will be resolved by ARP
    // For now, set to broadcast or rely on ARP resolution later
    memory_memcopy(connection->remote_mac, syn_packet.ether_destination_mac, sizeof(network_mac_address_t));

    syn_packet.is_ipv4_packet      = true;
    syn_packet.ipv4_protocol       = NETWORK_IPV4_PROTOCOL_TCPV4;
    syn_packet.ipv4_source_ip      = listener->local_ip;
    syn_packet.ipv4_destination_ip = remote_ip;

    syn_packet.is_tcpv4_packet              = true;
    syn_packet.tcpv4_source_port            = listener->local_port;
    syn_packet.tcpv4_destination_port       = remote_port;
    syn_packet.tcpv4_sequence_number        = connection->local_sequence_number;
    syn_packet.tcpv4_acknowledgement_number = connection->remote_sequence_number;
    syn_packet.tcpv4_syn                    = true;
    syn_packet.tcpv4_window_size            = scaled_window_size;
    syn_packet.tcpv4_window_scale           = connection->local_window_scale;
    syn_packet.tcpv4_mss                    = connection->mss;

    if(listener->role == NETWORK_LISTENER_ROLE_SERVER) {
        syn_packet.tcpv4_ack = true; // we received syn hence we need to send syn+ack
    }

    if(network_packet_craft(listener->network_info, &syn_packet, NULL, 0) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to craft SYN packet");
        pipeline_destroy(connection->read_pipeline);
        memory_free_ext(network_packet_heap, connection);
        return NULL;
    }

    if(network_send_packet_to_return_queue(listener->network_info, &syn_packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send SYN packet to return queue");
        pipeline_destroy(connection->read_pipeline);
        memory_free_ext(network_packet_heap, connection);
        return NULL;
    }

    if(listener->role == NETWORK_LISTENER_ROLE_SERVER) {
        connection->state = NETWORK_CONNECTION_STATE_SYN_ACK_SENT; // We sent SYN-ACK in response to client's SYN
        connection->local_sequence_number++; // SYN consumes one sequence number
    } else {
        connection->state = NETWORK_CONNECTION_STATE_SYN_SENT; // Initial state for client
    }

    connection->last_remote_activity_timestamp = time_ns(NULL);

    return connection;
}

network_connection_t* network_connection_connect(network_listener_t*    listener,
                                                 network_ipv4_address_t remote_ip,
                                                 uint16_t               remote_port) {
    if(!listener) {
        return NULL;
    }

    if(listener->role != NETWORK_LISTENER_ROLE_CLIENT) {
        PRINTLOG(NETWORK, LOG_ERROR, "only client listener can initiate connection");
        return NULL;
    }

    if(hashmap_size(listener->connections) > 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "client listener already has an active connection. "
                                     "multiple connections for client listener is not supported.");
        return NULL;
    }

    return network_connection_connect_internal(listener, remote_ip, remote_port, NULL);
}

network_connection_t* network_connection_accept(network_listener_t * listener) {
    if(!listener || listener->role != NETWORK_LISTENER_ROLE_SERVER) {
        return NULL;
    }

    while(list_size(listener->backlog) == 0) {
        task_msleep(10); // Sleep for a while before checking again
    }

    const network_connection_t* connection = NULL;

    while(true) {
        boolean_t found_established = false;
        boolean_t need_remove       = false;
        size_t found_index          = -1ULL;

        for(size_t i = 0; i < list_size(listener->backlog); i++) {
            connection = list_get_data_at_position(listener->backlog, i);

            if(connection->state == NETWORK_CONNECTION_STATE_ESTABLISHED) {
                found_established = true;
                found_index       = i;
                break;
            }

            if(listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_TCPV4) {
                // If the connection is in a transient state (SYN_SENT, SYN_ACK_SENT, FIN_WAIT_1,
                // FIN_WAIT_2, CLOSING, LAST_ACK, TIME_WAIT)
                // and has been inactive for a long time, consider it timed out and remove it.
                // The timeout duration can be adjusted based on typical network conditions and application requirements.
                // For simplicity, let's use a fixed timeout of 60 seconds for now.
                uint64_t now = time_ns(NULL);
                if (now - connection->last_remote_activity_timestamp > 60 * 1'000'000'000ULL) {
                    PRINTLOG(NETWORK, LOG_WARNING, "TCP connection to %i.%i.%i.%i:%u timed out in state %d. Removing from backlog.",
                             connection->remote_ip.as_bytes[0], connection->remote_ip.as_bytes[1], connection->remote_ip.as_bytes[2], connection->remote_ip.as_bytes[3],
                             connection->remote_port, connection->state);
                    need_remove = true;
                    found_index = i;
                    break;
                }
            }
        }

        if(found_established) {
            list_delete_at_position(listener->backlog, found_index);
            break;
        }

        if(need_remove) {
            list_delete_at_position(listener->backlog, found_index);
            uint64_t key = ((uint64_t)connection->remote_ip.as_dword << 16) | connection->remote_port;
            hashmap_delete(listener->connections, (void*)key);
            continue; // Check the backlog again after removing the timed-out connection
        }

        task_msleep(10); // Sleep for a while before checking again
    }

    return (network_connection_t*)connection;
}

static uint64_t network_connection_get_state_timeout(network_connection_state_t state) {
    switch(state) {
    case NETWORK_CONNECTION_STATE_TIME_WAIT:
        return 30 * 1'000'000'000ULL; // 30s for MSL
    case NETWORK_CONNECTION_STATE_FIN_WAIT_2:
        return 60 * 1'000'000'000ULL; // 60s for remote app close
    case NETWORK_CONNECTION_STATE_SYN_ACK_SENT:
        return 10 * 1'000'000'000ULL; // Fast timeout for failed handshakes
    default:
        return 120 * 1'000'000'000ULL; // Default safety
    }
}

void network_connection_destroy(network_connection_t* connection) {
    if(!connection) {
        return;
    }

    network_connection_close(connection);

    while(true) {
        lock_acquire(connection->lock);
        if(connection->state == NETWORK_CONNECTION_STATE_CLOSED) {
            break;
        }

        uint64_t timeout = network_connection_get_state_timeout(connection->state);
        if(time_ns(NULL) - connection->last_remote_activity_timestamp > timeout) {
            PRINTLOG(NETWORK, LOG_DEBUG, "TCP connection to %i.%i.%i.%i:%u timed out after %llis, "
                                         "while waiting for closure. Forcing close. current state: %d",
                     connection->remote_ip.as_bytes[0],
                     connection->remote_ip.as_bytes[1],
                     connection->remote_ip.as_bytes[2],
                     connection->remote_ip.as_bytes[3],
                     connection->remote_port,
                     timeout / 1'000'000'000ULL,
                     connection->state);
            break;
        }

        lock_release(connection->lock);

        task_msleep(10); // Sleep for a while before checking again
    }

    if(connection->read_pipeline) {
        pipeline_destroy(connection->read_pipeline);
    }

    if(connection->listener && connection->listener->connections) {
        uint64_t key = ((uint64_t)connection->remote_ip.as_dword << 16) | connection->remote_port;
        hashmap_delete(connection->listener->connections, (void*)key);
    }

    PRINTLOG(NETWORK, LOG_DEBUG, "destroying network connection to %i.%i.%i.%i:%u",
             connection->remote_ip.as_bytes[0],
             connection->remote_ip.as_bytes[1],
             connection->remote_ip.as_bytes[2],
             connection->remote_ip.as_bytes[3],
             connection->remote_port);

    lock_t* lock = connection->lock;

    memory_free_ext(network_packet_heap, connection);

    lock_release(lock);
    lock_destroy(lock);
    PRINTLOG(NETWORK, LOG_DEBUG, "network connection destroyed");
}

void network_connection_close(network_connection_t* connection) {
    if(!connection) {
        return;
    }

    if(connection->state == NETWORK_CONNECTION_STATE_CLOSED) {
        return;
    }

    if(connection->listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_UDPV4) {
        // For UDP, we can consider the connection closed immediately as there is no connection state to maintain.
        connection->state = NETWORK_CONNECTION_STATE_CLOSED;
        PRINTLOG(NETWORK, LOG_DEBUG, "UDP connection closed immediately.");
        return;
    }

    network_arp_cache_wait_and_get(connection->listener->network_info, connection->remote_ip, connection->remote_mac);

    lock_acquire(connection->lock);

    // For TCP, we need to perform a graceful shutdown using FIN packets.
    // The state machine for TCP connection closing is complex, but we'll implement a simplified version.
    // We'll send a FIN packet and transition to FIN_WAIT_1 or LAST_ACK depending on the current state.
    // The actual state transitions will be handled by the network_connection_packet_handle function
    // when it receives FIN/ACK packets from the remote host.
    // We will wait for the connection to reach NETWORK_CONNECTION_STATE_CLOSED before actually freeing memory.

    // If we are already in a closing state, just return.
    if(connection->state == NETWORK_CONNECTION_STATE_FIN_WAIT_1 ||
       connection->state == NETWORK_CONNECTION_STATE_FIN_WAIT_2 ||
       connection->state == NETWORK_CONNECTION_STATE_CLOSING ||
       connection->state == NETWORK_CONNECTION_STATE_LAST_ACK ||
       connection->state == NETWORK_CONNECTION_STATE_TIME_WAIT) {
        PRINTLOG(NETWORK, LOG_DEBUG, "connection is already in a closing state. Current state: %d", connection->state);
        lock_release(connection->lock);
        return;
    }

    network_packet_t fin_packet;
    memory_memclean(&fin_packet, sizeof(network_packet_t));

    fin_packet.ether_ethertype       = connection->listener->protocol;
    fin_packet.ether_inner_ethertype = connection->listener->protocol;

    memory_memcopy(connection->listener->network_info->mac, fin_packet.ether_source_mac, sizeof(network_mac_address_t));
    memory_memcopy(connection->remote_mac, fin_packet.ether_destination_mac, sizeof(network_mac_address_t)); // Will be resolved by ARP

    fin_packet.is_ipv4_packet      = true;
    fin_packet.ipv4_protocol       = NETWORK_IPV4_PROTOCOL_TCPV4;
    fin_packet.ipv4_source_ip      = connection->listener->local_ip;
    fin_packet.ipv4_destination_ip = connection->remote_ip;

    fin_packet.is_tcpv4_packet              = true;
    fin_packet.tcpv4_source_port            = connection->listener->local_port;
    fin_packet.tcpv4_destination_port       = connection->remote_port;
    fin_packet.tcpv4_sequence_number        = connection->local_sequence_number;
    fin_packet.tcpv4_acknowledgement_number = connection->remote_sequence_number;
    fin_packet.tcpv4_fin                    = true;
    fin_packet.tcpv4_ack                    = true; // FIN packets also carry ACK

    connection->local_window_size = pipeline_available_space(connection->read_pipeline);
    uint32_t scaled_window_size = connection->local_window_size >> connection->local_window_scale;
    scaled_window_size = MIN(scaled_window_size, 0xFFFF); // TCP window size field is 16 bits

    fin_packet.tcpv4_window_size = scaled_window_size;

    if(network_packet_craft(connection->listener->network_info, &fin_packet, NULL, 0) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to craft FIN packet");
        // Fallback to immediate destroy if we can't send FIN
        connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
        lock_release(connection->lock);
        return;
    }

    if(network_send_packet_to_return_queue(connection->listener->network_info, &fin_packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send FIN packet to return queue");
        // Fallback to immediate destroy if we can't send FIN
        connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
        lock_release(connection->lock);
        return;
    }

    connection->local_sequence_number++; // FIN consumes one sequence number

    switch(connection->state) {
    case NETWORK_CONNECTION_STATE_ESTABLISHED:
        connection->state = NETWORK_CONNECTION_STATE_FIN_WAIT_1;
        break;
    case NETWORK_CONNECTION_STATE_CLOSE_WAIT:
        connection->state = NETWORK_CONNECTION_STATE_LAST_ACK;
        break;
    default:
        break;
    }

    lock_release(connection->lock);
}

int32_t network_connection_send(network_connection_t* connection,
                                uint8_t*              data,
                                uint32_t              data_len) {
    if(!connection || !data || data_len == 0) {
        return -1;
    }

    int64_t remaining = data_len;
    uint32_t offset   = 0;

    while(remaining > 0) {
        uint16_t chunk_size = remaining > connection->mss ? connection->mss : remaining;

        network_packet_t packet;
        memory_memclean(&packet, sizeof(network_packet_t));

        network_arp_cache_wait_and_get(connection->listener->network_info, connection->remote_ip, connection->remote_mac);

        packet.ether_ethertype       = connection->listener->protocol;
        packet.ether_inner_ethertype = connection->listener->protocol;

        memory_memcopy(connection->listener->network_info->mac, packet.ether_source_mac, sizeof(network_mac_address_t));
        memory_memcopy(connection->remote_mac, packet.ether_destination_mac, sizeof(network_mac_address_t));

        if(connection->listener->protocol == NETWORK_PROTOCOL_IPV4) {
            if(connection->listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_TCPV4) {
                while(true) {
                    lock_acquire(connection->lock);
                    if(connection->state >= NETWORK_CONNECTION_STATE_FIN_WAIT_1) {
                        PRINTLOG(NETWORK, LOG_ERROR, "cannot send data on a connection that is closing or closed. Current state: %d", connection->state);
                        lock_release(connection->lock);
                        return -1;
                    }

                    if(connection->state == NETWORK_CONNECTION_STATE_ESTABLISHED &&
                       connection->remote_window_size >= chunk_size) {
                        lock_release(connection->lock);
                        break;
                    }

                    // if there is no state change for a long time, consider the connection is broken and return error.
                    if(time_ns(NULL) - connection->last_remote_activity_timestamp > 60 * 1'000'000'000ULL) {
                        PRINTLOG(NETWORK, LOG_WARNING, "TCP connection to %i.%i.%i.%i:%u seems broken. Last activity was at %llu ns. Current state: %d",
                                 connection->remote_ip.as_bytes[0],
                                 connection->remote_ip.as_bytes[1],
                                 connection->remote_ip.as_bytes[2],
                                 connection->remote_ip.as_bytes[3],
                                 connection->remote_port,
                                 connection->last_remote_activity_timestamp,
                                 connection->state);
                        connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
                        lock_release(connection->lock);
                        return -1;
                    }

                    lock_release(connection->lock);

                    task_msleep(10); // Sleep for a while before checking again
                }

                packet.is_tcpv4_packet              = true;
                packet.ipv4_protocol                = NETWORK_IPV4_PROTOCOL_TCPV4;
                packet.tcpv4_source_port            = connection->listener->local_port;
                packet.tcpv4_destination_port       = connection->remote_port;
                packet.tcpv4_sequence_number        = connection->local_sequence_number;
                packet.tcpv4_acknowledgement_number = connection->remote_sequence_number;
                packet.tcpv4_psh                    = true; // Set PSH flag to indicate data is being sent
                packet.tcpv4_ack                    = true; // Data packets also carry ACK

                connection->local_window_size = pipeline_available_space(connection->read_pipeline);

                connection->local_window_size = pipeline_available_space(connection->read_pipeline);
                uint32_t scaled_window_size = connection->local_window_size >> connection->local_window_scale;
                scaled_window_size = MIN(scaled_window_size, 0xFFFF); // TCP window size field is 16 bits

                packet.tcpv4_window_size = scaled_window_size;
            } else if(connection->listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_UDPV4) {
                if(connection->state != NETWORK_CONNECTION_STATE_ESTABLISHED) {
                    PRINTLOG(NETWORK, LOG_ERROR, "cannot send data on a UDP connection that is not established. Current state: %d", connection->state);
                    return -1;
                }

                packet.is_udpv4_packet        = true;
                packet.ipv4_protocol          = NETWORK_IPV4_PROTOCOL_UDPV4;
                packet.udpv4_source_port      = connection->listener->local_port;
                packet.udpv4_destination_port = connection->remote_port;
            } else {
                PRINTLOG(NETWORK, LOG_ERROR, "unsupported ipv4 protocol %d", connection->listener->ipv4_protocol);
                return -1;
            }

            packet.is_ipv4_packet = true;
            packet.ipv4_source_ip = connection->listener->local_ip;

            if(network_ipv4_is_address_eq(connection->remote_ip, (network_ipv4_address_t){0})) {
                packet.ipv4_destination_ip = NETWORK_IPV4_GLOBAL_BROADCAST_IP;
            } else {
                packet.ipv4_destination_ip = connection->remote_ip;
            }
        } else {
            PRINTLOG(NETWORK, LOG_ERROR, "unsupported network protocol %d", connection->listener->protocol);
            return -1;
        }

        if(network_packet_craft(connection->listener->network_info, &packet, data + offset, chunk_size) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to craft network packet");
            return -1;
        }

        if(network_send_packet_to_return_queue(connection->listener->network_info, &packet) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to send packet to return queue");
            return -1;
        }

        if(connection->listener->ipv4_protocol == NETWORK_IPV4_PROTOCOL_TCPV4) {
            connection->local_sequence_number += chunk_size; // Update sequence number for TCP
            connection->remote_window_size    -= chunk_size; // Decrease remote window size by the amount of data sent
        }

        remaining -= chunk_size;
        offset    += chunk_size;
    }

    return data_len;

}

int32_t network_connection_receive(network_connection_t* connection,
                                   uint8_t*              buffer,
                                   uint32_t              buffer_len) {
    if(!connection || !buffer || buffer_len == 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "connection, buffer or buffer_len is invalid");
        return -1;
    }

    int32_t read_len = 0;


    while(!read_len) {
        while(true) {
            // if lock is held by us it just passes.
            lock_acquire(connection->lock);

            // if the connection is in a state where it cannot receive data, return 0 to indicate no more data will be received.
            if(connection->state >= NETWORK_CONNECTION_STATE_CLOSE_WAIT) {
                PRINTLOG(NETWORK, LOG_WARNING, "connection is closing or closed, no more data to receive. Current state: %d", connection->state);
                lock_release(connection->lock);
                return 0; // Return 0 to indicate connection is closing/closed and no more data will be received
            }

            // here we can receive data not only in ESTABLISHED state, but also in
            // half-closed states (FIN_WAIT_1, FIN_WAIT_2) because
            // the connection is still valid for receiving data until it is fully closed.
            if(connection->state >= NETWORK_CONNECTION_STATE_ESTABLISHED &&
               connection->state < NETWORK_CONNECTION_STATE_CLOSE_WAIT) {
                lock_release(connection->lock);
                break; // Connection is established, we can try to read data
            }

            // If the connection is in a transient state (SYN_SENT, SYN_ACK_SENT, FIN_WAIT_1,
            // FIN_WAIT_2, CLOSING, LAST_ACK, TIME_WAIT)
            // and has been inactive for a long time, consider it timed out and return 0.
            // The timeout duration can be adjusted based on typical network conditions and application requirements.
            // For simplicity, let's use a fixed timeout of 60 seconds for now.
            uint64_t now = time_ns(NULL);
            if (now - connection->last_remote_activity_timestamp > 60 * 1'000'000'000ULL) {
                PRINTLOG(NETWORK, LOG_WARNING, "TCP connection to %i.%i.%i.%i:%u timed out in state %d. Returning 0.",
                         connection->remote_ip.as_bytes[0],
                         connection->remote_ip.as_bytes[1],
                         connection->remote_ip.as_bytes[2],
                         connection->remote_ip.as_bytes[3],
                         connection->remote_port,
                         connection->state);
                connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
                lock_release(connection->lock);
                return 0; // Indicate that the connection is effectively closed due to timeout
            }

            lock_release(connection->lock);

            task_msleep(10); // Sleep for a while before checking again
        }

        read_len = pipeline_read(connection->read_pipeline, buffer_len, buffer);

        if(read_len < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to read from connection pipeline");
            return -1;
        }

        if(read_len == 0) {
            task_msleep(50); // sleep for a while before trying again
        }
    }

    return read_len;
}

static int8_t network_arp_packet_handle(const network_info_t* ni, network_packet_t* packet) {
    if(!ni || !packet) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info or packet is NULL");
        return -1;
    }

    if(packet->arp_operation_code == NETWORK_ARP_OPERATION_CODE_REQUEST) {
        // If the ARP request is for our IP address, send a reply
        if(network_ipv4_is_address_eq(packet->arp_target_ip, ni->ipv4_address)) {
            network_packet_t reply_packet;
            memory_memclean(&reply_packet, sizeof(network_packet_t));

            reply_packet.ether_ethertype       = NETWORK_PROTOCOL_ARP;
            reply_packet.ether_inner_ethertype = NETWORK_PROTOCOL_ARP;
            memory_memcopy(ni->mac, reply_packet.ether_source_mac, sizeof(network_mac_address_t));
            memory_memcopy(packet->ether_source_mac, reply_packet.ether_destination_mac, sizeof(network_mac_address_t));

            reply_packet.is_arp_packet      = true;
            reply_packet.arp_operation_code = NETWORK_ARP_OPERATION_CODE_REPLY;
            memory_memcopy(ni->mac, reply_packet.arp_source_mac, sizeof(network_mac_address_t));
            reply_packet.arp_source_ip = ni->ipv4_address;
            memory_memcopy(packet->arp_source_mac, reply_packet.arp_target_mac, sizeof(network_mac_address_t));
            reply_packet.arp_target_ip = packet->arp_source_ip;

            if(network_packet_craft(ni, &reply_packet, NULL, 0) < 0) {
                PRINTLOG(NETWORK, LOG_ERROR, "failed to craft ARP reply packet");
                return -1;
            }

            if(network_send_packet_to_return_queue(ni, &reply_packet) < 0) {
                PRINTLOG(NETWORK, LOG_ERROR, "failed to send ARP reply packet to return queue");
                return -1;
            }
            return 0;
        } else {
            // If the ARP request is not for our IP address, ignore it
            PRINTLOG(NETWORK, LOG_TRACE, "ignoring ARP request for IP %i.%i.%i.%i as it does not match our IP %i.%i.%i.%i",
                     packet->arp_target_ip.as_bytes[0], packet->arp_target_ip.as_bytes[1], packet->arp_target_ip.as_bytes[2], packet->arp_target_ip.as_bytes[3],
                     ni->ipv4_address.as_bytes[0], ni->ipv4_address.as_bytes[1], ni->ipv4_address.as_bytes[2], ni->ipv4_address.as_bytes[3]);
            return 0;
        }
    } else if(packet->arp_operation_code == NETWORK_ARP_OPERATION_CODE_REPLY) {
        // Update ARP cache with the received information
        network_arp_cache_update(ni, packet->arp_source_ip, packet->arp_source_mac);
        return 0;
    }

    PRINTLOG(NETWORK, LOG_ERROR, "unsupported ARP operation code %d", packet->arp_operation_code);

    return -1;
}

static int8_t network_icmpv4_packet_handle(const network_info_t* ni, network_packet_t* packet) {
    if(!ni || !packet) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info or packet is NULL");
        return -1;
    }

    if(packet->icmpv4_type == NETWORK_ICMPV4_TYPE_ECHO_REQUEST) {
        // If the ICMPv4 request is for our IP address, send a reply
        if(network_ipv4_is_address_eq(packet->ipv4_destination_ip, ni->ipv4_address)) {
            network_packet_t reply_packet;
            memory_memclean(&reply_packet, sizeof(network_packet_t));

            reply_packet.ether_ethertype       = NETWORK_PROTOCOL_IPV4;
            reply_packet.ether_inner_ethertype = NETWORK_PROTOCOL_IPV4;
            memory_memcopy(ni->mac, reply_packet.ether_source_mac, sizeof(network_mac_address_t));
            memory_memcopy(packet->ether_source_mac, reply_packet.ether_destination_mac, sizeof(network_mac_address_t));

            reply_packet.is_ipv4_packet      = true;
            reply_packet.ipv4_protocol       = NETWORK_IPV4_PROTOCOL_ICMPV4;
            reply_packet.ipv4_source_ip      = ni->ipv4_address;
            reply_packet.ipv4_destination_ip = packet->ipv4_source_ip;

            reply_packet.is_icmpv4_packet  = true;
            reply_packet.icmpv4_type       = NETWORK_ICMPV4_TYPE_ECHO_REPLY;
            reply_packet.icmpv4_code       = NETWORK_ICMPV4_ECHO_REQUEST_REPLY_CODE_NO_CODE;
            reply_packet.icmpv4_identifier = packet->icmpv4_identifier;
            reply_packet.icmpv4_sequence   = packet->icmpv4_sequence;

            if(network_packet_craft(ni, &reply_packet, packet->payload_offset, packet->payload_len) < 0) {
                PRINTLOG(NETWORK, LOG_ERROR, "failed to craft ICMPv4 reply packet");
                return -1;
            }

            if(network_send_packet_to_return_queue(ni, &reply_packet) < 0) {
                PRINTLOG(NETWORK, LOG_ERROR, "failed to send ICMPv4 reply packet to return queue");
                return -1;
            }
            return 0;
        }
    } else if(packet->icmpv4_type == NETWORK_ICMPV4_TYPE_ECHO_REPLY) {
        network_listener_t* listener = network_listener_get(NETWORK_IPV4_PROTOCOL_ICMPV4, packet->ipv4_destination_ip, packet->icmpv4_identifier);

        if(!listener) {
            PRINTLOG(NETWORK, LOG_ERROR, "no listener found for received ICMPv4 echo reply packet with destination IP %u.%u.%u.%u and identifier %u",
                     (packet->ipv4_destination_ip.as_dword >> 24) & 0xFF,
                     (packet->ipv4_destination_ip.as_dword >> 16) & 0xFF,
                     (packet->ipv4_destination_ip.as_dword >> 8) & 0xFF,
                     packet->ipv4_destination_ip.as_dword & 0xFF,
                     packet->icmpv4_identifier);
            return -1;
        }

        if(listener->role != NETWORK_LISTENER_ROLE_CLIENT) {
            PRINTLOG(NETWORK, LOG_ERROR, "listener found for received ICMPv4 echo reply packet with destination IP %u.%u.%u.%u and identifier %u, but listener is not a client",
                     (packet->ipv4_destination_ip.as_dword >> 24) & 0xFF,
                     (packet->ipv4_destination_ip.as_dword >> 16) & 0xFF,
                     (packet->ipv4_destination_ip.as_dword >> 8) & 0xFF,
                     packet->ipv4_destination_ip.as_dword & 0xFF,
                     packet->icmpv4_identifier);
            return -1;
        }

        if(!network_ipv4_is_address_eq(packet->ipv4_destination_ip, listener->local_ip)) {
            PRINTLOG(NETWORK, LOG_ERROR, "listener found for received ICMPv4 echo reply packet with destination IP %u.%u.%u.%u and identifier %u, but source IP %u.%u.%u.%u does not match listener local IP",
                     (packet->ipv4_destination_ip.as_dword >> 24) & 0xFF,
                     (packet->ipv4_destination_ip.as_dword >> 16) & 0xFF,
                     (packet->ipv4_destination_ip.as_dword >> 8) & 0xFF,
                     packet->ipv4_destination_ip.as_dword & 0xFF,
                     packet->icmpv4_identifier,
                     (packet->ipv4_source_ip.as_dword >> 24) & 0xFF,
                     (packet->ipv4_source_ip.as_dword >> 16) & 0xFF,
                     (packet->ipv4_source_ip.as_dword >> 8) & 0xFF,
                     packet->ipv4_source_ip.as_dword & 0xFF);
            return -1;
        }

        listener->icmpv4_last_echo_request_timestamp = packet->icmpv4_ping_timestamp_sec * 1000000000ULL + packet->icmpv4_ping_timestamp_usec * 1000ULL;

        time_t now = time_ns(NULL);

        listener->icmpv4_total_round_trip_time = now - listener->icmpv4_last_echo_request_timestamp;

        if(listener->icmpv4_last_echo_request_sequence + 1 != packet->icmpv4_sequence) {
            PRINTLOG(NETWORK, LOG_WARNING, "received ICMPv4 echo reply with sequence %u, but last echo request sequence is %u, consider it as lost echo reply",
                     packet->icmpv4_sequence,
                     listener->icmpv4_last_echo_request_sequence);
            listener->icmpv4_lost_echo_replies++;
        }

        listener->icmpv4_last_echo_request_sequence = packet->icmpv4_sequence;

        return 0;
    }

    PRINTLOG(NETWORK, LOG_ERROR, "unsupported ICMPv4 type %d for connection packet handle", packet->icmpv4_type);
    return -1;
}

static int8_t network_tcpv4_build_and_send_reset_packet(const network_info_t* ni, network_packet_t* packet) {
    network_packet_t rst_packet;
    memory_memclean(&rst_packet, sizeof(network_packet_t));
    rst_packet.ether_ethertype       = NETWORK_PROTOCOL_IPV4;
    rst_packet.ether_inner_ethertype = NETWORK_PROTOCOL_IPV4;
    memory_memcopy(ni->mac, rst_packet.ether_source_mac, sizeof(network_mac_address_t));
    memory_memcopy(packet->ether_source_mac, rst_packet.ether_destination_mac, sizeof(network_mac_address_t));
    rst_packet.is_ipv4_packet               = true;
    rst_packet.ipv4_protocol                = NETWORK_IPV4_PROTOCOL_TCPV4;
    rst_packet.ipv4_source_ip               = ni->ipv4_address;
    rst_packet.ipv4_destination_ip          = packet->ipv4_source_ip;
    rst_packet.is_tcpv4_packet              = true;
    rst_packet.tcpv4_source_port            = packet->tcpv4_destination_port;
    rst_packet.tcpv4_destination_port       = packet->tcpv4_source_port;
    rst_packet.tcpv4_sequence_number        = packet->tcpv4_ack? packet->tcpv4_acknowledgement_number : 0;
    rst_packet.tcpv4_acknowledgement_number = packet->tcpv4_ack? 0 : packet->tcpv4_sequence_number + packet->payload_len;
    rst_packet.tcpv4_rst                    = true;
    rst_packet.tcpv4_ack                    = !packet->tcpv4_ack;

    if(network_packet_craft(ni, &rst_packet, NULL, 0) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to craft TCP RST packet");
    }

    if(network_send_packet_to_return_queue(ni, &rst_packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send TCP RST packet to return queue");
    }

    return 0; // sliently accept our fate.
}

static int8_t network_tcpv4_build_and_send_ack_packet(const network_info_t* ni, network_connection_t* connection, network_packet_t* packet) {
    network_packet_t ack_packet;
    memory_memclean(&ack_packet, sizeof(network_packet_t));
    ack_packet.ether_ethertype       = NETWORK_PROTOCOL_IPV4;
    ack_packet.ether_inner_ethertype = NETWORK_PROTOCOL_IPV4;
    memory_memcopy(ni->mac, ack_packet.ether_source_mac, sizeof(network_mac_address_t));
    memory_memcopy(packet->ether_source_mac, ack_packet.ether_destination_mac, sizeof(network_mac_address_t));
    ack_packet.is_ipv4_packet               = true;
    ack_packet.ipv4_protocol                = NETWORK_IPV4_PROTOCOL_TCPV4;
    ack_packet.ipv4_source_ip               = ni->ipv4_address;
    ack_packet.ipv4_destination_ip          = packet->ipv4_source_ip;
    ack_packet.is_tcpv4_packet              = true;
    ack_packet.tcpv4_source_port            = packet->tcpv4_destination_port;
    ack_packet.tcpv4_destination_port       = packet->tcpv4_source_port;
    ack_packet.tcpv4_sequence_number        = connection->local_sequence_number;
    ack_packet.tcpv4_acknowledgement_number = connection->remote_sequence_number;
    ack_packet.tcpv4_ack                    = true;

    connection->local_window_size = pipeline_available_space(connection->read_pipeline);
    uint32_t scaled_window_size = connection->local_window_size >> connection->local_window_scale;
    scaled_window_size = MIN(scaled_window_size, 0xFFFF); // TCP window size field is 16 bits, so the maximum value after scaling is 65535

    ack_packet.tcpv4_window_size = scaled_window_size;

    if(network_packet_craft(ni, &ack_packet, NULL, 0) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to craft TCP ACK packet");
        return -1;
    }

    if(network_send_packet_to_return_queue(ni, &ack_packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send TCP ACK packet to return queue");
        return -1;
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t network_tcpv4_packet_handle(const network_info_t* ni, network_packet_t* packet) {
    network_ipv4_address_t destination_ip = packet->ipv4_destination_ip;

    network_listener_t* listener = network_listener_get(packet->ipv4_protocol, destination_ip, packet->tcpv4_destination_port);

    if(!listener) {
        PRINTLOG(NETWORK, LOG_DEBUG, "no listener found for received TCPv4 packet with destination IP %u.%u.%u.%u and destination port %u for key 0x%llx",
                 destination_ip.as_bytes[0],
                 destination_ip.as_bytes[1],
                 destination_ip.as_bytes[2],
                 destination_ip.as_bytes[3],
                 packet->udpv4_destination_port,
                 ((uint64_t)destination_ip.as_dword << 16) | packet->udpv4_destination_port);

        // we need to send RST packet back to remote host to inform it
        // that there is no application listening on this IP and port,
        // otherwise remote host will keep retrying and we will keep receiving
        // the same packet and keep logging this error without sending any response back,
        // which will cause a lot of noise in the log.
        return network_tcpv4_build_and_send_reset_packet(ni, packet);
    }

    network_ipv4_address_t source_ip = packet->ipv4_source_ip;

    uint64_t connection_key = ((uint64_t)source_ip.as_dword << 16) | packet->tcpv4_source_port;

    network_connection_t* connection = (network_connection_t*)hashmap_get(listener->connections, (void*)connection_key);

    if(!connection) {
        if(listener->role != NETWORK_LISTENER_ROLE_SERVER) {
            PRINTLOG(NETWORK, LOG_ERROR, "no connection found for received UDPv4 packet with source IP %u.%u.%u.%u and source port %u, and listener is not a server",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port);
            // we need to send RST packet back to remote host to inform it
            // that there is no connection associated with this source IP and port for this listener,
            // and since it's not a server listener, it means we are not expecting any new connections on this IP and port,
            // so we should send RST back to remote host to inform it, otherwise
            // remote host will keep retrying and we will keep receiving the same packet
            // and keep logging this error without sending any response back, which will cause a lot of noise in the log.
            return network_tcpv4_build_and_send_reset_packet(ni, packet);
        }

        // in here we should check if the SYN flag is set in the received packet,
        // if not, it means this is not a valid new connection request,
        // we should send RST back to remote host to inform it,
        // otherwise remote host will keep retrying and we will keep receiving
        // the same packet and keep logging this error without sending any response back,
        // which will cause a lot of noise in the log.
        if(!packet->tcpv4_syn) {
            PRINTLOG(NETWORK, LOG_DEBUG, "received TCPv4 packet with source IP %u.%u.%u.%u and source port %u does not have SYN flag set, and no existing connection found for it",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port);

            // sometime in here we can get rst packet from remote host,
            // then just silently accept it without sending rst back,
            // because if we keep sending rst back for the rst packet we received,
            // it will cause a lot of noise in the log and also cause unnecessary network traffic.
            if(packet->tcpv4_rst) {
                PRINTLOG(NETWORK, LOG_DEBUG, "received TCP RST packet with source IP %u.%u.%u.%u and source port %u, silently accepting it without sending RST back",
                         source_ip.as_bytes[0],
                         source_ip.as_bytes[1],
                         source_ip.as_bytes[2],
                         source_ip.as_bytes[3],
                         packet->tcpv4_source_port);
                return 0; // silently accept the RST packet without sending RST back
            }

            return network_tcpv4_build_and_send_reset_packet(ni, packet);
        }

        // create a new connection for this source IP and port
        // we should set the initial sequence number to the sequence number in the received SYN packet,
        // because in TCP handshake, the server should respond with a SYN-ACK packet
        // that has the same sequence number as the client's SYN packet,
        // and then the client will ACK with the next sequence number,
        // so if we set the initial sequence number to a different value, the client's ACK
        // will not match our expected sequence number and the connection will fail to establish.
        // for server type listener's this method sends syn+ack packet back to client and create connection in SYN_ACK_SENT state.
        // also this method updates remote window size and other related info based on the received SYN packet.
        connection = network_connection_connect_internal(listener, packet->ipv4_source_ip, packet->tcpv4_source_port, packet);

        if(!connection) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to create connection for received UDPv4 packet with source IP %u.%u.%u.%u and source port %u",
                     (packet->ipv4_source_ip.as_dword >> 24) & 0xFF,
                     (packet->ipv4_source_ip.as_dword >> 16) & 0xFF,
                     (packet->ipv4_source_ip.as_dword >> 8) & 0xFF,
                     packet->ipv4_source_ip.as_dword & 0xFF,
                     packet->udpv4_source_port);

            // in here we also need to send RST packet back to remote host to inform it
            // that we failed to create a connection for this incoming packet,
            // otherwise remoteni host will keep retrying and we will keep receiving
            // the same packet and keep logging this error without sending any response back,
            // which will cause a lot of noise in the log.
            network_tcpv4_build_and_send_reset_packet(ni, packet);

            return -1; // this is an error because we expect to be able to create a connection for any incoming UDP packet to a server listener, so we return -1 to indicate an error
        }

        return 0; // we have successfully created a new connection for this incoming SYN packet, so we can return 0 to indicate success
    }

    connection->last_remote_activity_timestamp = time_ns(NULL); // Update last remote activity timestamp

    // now we have the connection for this incoming packet,
    // first check if the packet rst packet, if so, set connection state to closed and return,
    // because RST packet indicates that the remote host wants to immediately close the connection,
    // so we should not send any response back and just close the connection locally.
    if(packet->tcpv4_rst) {
        PRINTLOG(NETWORK, LOG_DEBUG, "received TCP RST packet for connection with remote IP %u.%u.%u.%u and remote port %u, closing the connection",
                 source_ip.as_bytes[0],
                 source_ip.as_bytes[1],
                 source_ip.as_bytes[2],
                 source_ip.as_bytes[3],
                 packet->tcpv4_source_port);
        connection->state = NETWORK_CONNECTION_STATE_CLOSED;
        return 0; // silently accept the RST packet and close the connection without sending RST back
    }

    // Now handle FIN packet. The FIN flag indicates that the sender has finished sending data and wants to close the connection gracefully.
    if(packet->tcpv4_fin) {
        lock_acquire(connection->lock);

        // 1. Sequence Validation: Only accept FIN if it matches the expected sequence
        // A FIN that arrives early (out of order) should be buffered, not processed.
        if(packet->tcpv4_sequence_number != connection->remote_sequence_number) {
            lock_release(connection->lock);
            return 0;
        }

        // 2. Increment remote sequence to account for the FIN bit
        connection->remote_sequence_number = packet->tcpv4_sequence_number + 1;
        connection->remote_window_size     = packet->tcpv4_window_size << connection->local_window_scale;

        // 3. Prepare the state transition
        boolean_t ack_covers_our_fin = (packet->tcpv4_ack &&
                                        packet->tcpv4_acknowledgement_number == connection->local_sequence_number + 1);

        switch(connection->state) {
        case NETWORK_CONNECTION_STATE_ESTABLISHED:
            connection->state = NETWORK_CONNECTION_STATE_CLOSE_WAIT;
            break;
        case NETWORK_CONNECTION_STATE_FIN_WAIT_1:
            if(ack_covers_our_fin) {
                // Combined FIN+ACK: Transition directly to TIME_WAIT
                connection->state = NETWORK_CONNECTION_STATE_TIME_WAIT;
            } else {
                // Pure FIN: Simultaneous close
                connection->state = NETWORK_CONNECTION_STATE_CLOSING;
            }
            break;
        case NETWORK_CONNECTION_STATE_FIN_WAIT_2:
            // Pure FIN: Transition to TIME_WAIT and wait for potential retransmissions of FIN from remote
            connection->state = NETWORK_CONNECTION_STATE_TIME_WAIT;
            break;
        case NETWORK_CONNECTION_STATE_CLOSING:
            if(ack_covers_our_fin) {
                // ACK for our FIN received: Transition to TIME_WAIT
                connection->state = NETWORK_CONNECTION_STATE_TIME_WAIT;
            }
            break;
        case NETWORK_CONNECTION_STATE_LAST_ACK:
            if(ack_covers_our_fin) {
                // ACK for our FIN received: Transition to CLOSED
                connection->state = NETWORK_CONNECTION_STATE_CLOSED;
            }
            break;
        case NETWORK_CONNECTION_STATE_TIME_WAIT:
            // Retransmission of FIN: Stay in TIME_WAIT and re-send ACK
            break;
        default:
            PRINTLOG(NETWORK, LOG_WARNING, "Ignored FIN in state %d", connection->state);
            break;
        }

        // 4. Always ACK the FIN
        if(network_tcpv4_build_and_send_ack_packet(ni, connection, packet) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to send ACK packet in response to FIN");
            // Even if ACK fails, we still consider the connection closed from our side, so we proceed with state transition.
        }

        lock_release(connection->lock);
        return 0;
    }


    // if we received a SYN packet, we should check several conditions to determine if this is expected or unexpected,
    // and whether we should send RST back to remote host or not, because receiving a SYN packet for an existing
    // connection can be normal in some cases (for example, if the client is trying to re-establish a connection after a timeout),
    // but it can also be unexpected in some cases (for example, if the client is trying to establish a new connection
    // with the same source IP and port while we already have an existing connection with that source IP and port),
    // so we need to carefully check the conditions to determine how to handle it.
    if(packet->tcpv4_syn) {
        // if we receive syn packet without ack packet, it is unexpected because
        // it means the remote host is trying to establish a new connection with
        // the same source IP and port while we already have an existing connection
        // with that source IP and port, which is not expected in normal TCP connection
        // lifecycle, so we consider it as an error and send RST back to remote host to inform it,
        // and also mark the existing connection as closed to prevent further issues.
        if(!packet->tcpv4_ack) {
            PRINTLOG(NETWORK, LOG_DEBUG, "received unexpected TCP SYN packet for an existing connection with remote IP %u.%u.%u.%u and remote port %u, which is not expected",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port);
            connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
            return network_tcpv4_build_and_send_reset_packet(ni, packet);
        }

        // okay we received a SYN packet with ACK flag set. now we should check if the listener is for client role.
        // if not it is unexpected because only client role listener should receive SYN packet with ACK flag set
        // in normal TCP connection lifecycle, which indicates the second step of TCP handshake,
        // so we consider it as an error and send RST back to remote host to inform it, and also
        // mark the existing connection as closed to prevent further issues.
        if(listener->role != NETWORK_LISTENER_ROLE_CLIENT) {
            PRINTLOG(NETWORK, LOG_DEBUG, "received unexpected TCP SYN packet with ACK flag set for an existing connection with remote IP %u.%u.%u.%u and remote port %u, but listener role is not client",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port);
            connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
            return network_tcpv4_build_and_send_reset_packet(ni, packet);
        }

        lock_acquire(connection->lock);

        // okay, we are client and we received a SYN+ACK packet,
        // but we should check if the connection is in SYN+ACK sent state, if not, it
        // is unexpected because it means we are receiving a SYN+ACK packet for a connection
        // that is not in the process of being established, which is not expected in normal TCP
        // connection lifecycle, so we consider it as an error and send RST back to remote
        // host to inform it, and also mark the existing connection as closed to prevent further issues.
        if(connection->state != NETWORK_CONNECTION_STATE_SYN_ACK_SENT) {
            PRINTLOG(NETWORK, LOG_DEBUG, "received unexpected TCP SYN packet with ACK flag set for an existing connection with remote IP %u.%u.%u.%u and remote port %u, but connection state is not SYN_ACK_SENT. Current state: %d",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port,
                     connection->state);
            connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
            lock_release(connection->lock);
            return network_tcpv4_build_and_send_reset_packet(ni, packet);
        }

        // Now we should check if the acknowledgement number in the received SYN+ACK packet
        // matches our expected sequence number for SYN+ACK response,
        // if not, it is unexpected because it means the remote host is trying to establish a new connection
        // with the same source IP and port while we already have an existing connection with
        // that source IP and port, which is not expected in normal TCP connection lifecycle,
        // so we consider it as an error and send RST back to remote host to inform it, and also
        // mark the existing connection as closed to prevent further issues.
        if(packet->tcpv4_acknowledgement_number != connection->local_sequence_number + 1) {
            PRINTLOG(NETWORK, LOG_DEBUG, "received unexpected TCP SYN packet with ACK flag set "
                                         "for an existing connection with remote IP %u.%u.%u.%u and remote port %u, "
                                         "but ACK number %u does not match expected ACK number %u",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port,
                     packet->tcpv4_acknowledgement_number,
                     connection->local_sequence_number + 1);
            connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
            lock_release(connection->lock);
            return network_tcpv4_build_and_send_reset_packet(ni, packet);
        }

        // now we can send ack packet back to remote host to complete the TCP handshake and establish the connection,
        // and also update the connection state to established, and update the last remote activity timestamp,
        // and also update the remote window size and other related info based on the received SYN+ACK packet.
        if(network_tcpv4_build_and_send_ack_packet(ni, connection, packet) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to send TCP ACK packet in response to "
                                         "received SYN+ACK packet for connection with "
                                         "remote IP %u.%u.%u.%u and remote port %u",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port);
            connection->state = NETWORK_CONNECTION_STATE_CLOSED; // Mark the connection as closed
            lock_release(connection->lock);
            return -1; // This is an error because we expect to be able to send ACK packet in response to SYN+ACK packet to complete the TCP handshake and establish the connection, so we return -1 to indicate an error
        }

        // Update remote sequence number to the sequence number in the received SYN+ACK packet + 1, because SYN flag consumes one sequence number
        connection->remote_sequence_number = packet->tcpv4_sequence_number + 1;
        // Update remote window size based on the received SYN+ACK packet
        connection->remote_window_size  = packet->tcpv4_window_size << packet->tcpv4_window_scale;
        connection->remote_window_scale = packet->tcpv4_window_scale;
        connection->state               = NETWORK_CONNECTION_STATE_ESTABLISHED; // Update connection state to established
        lock_release(connection->lock);
        return 0; // We have successfully handled the received SYN+ACK packet and established the connection, so we can return 0 to indicate success
    }

    // in here process single ack packets. (till now we have processed syn, syn+ack, fin and fin+ack packets,
    // now we are processing pure ack packets without syn or fin flag set)
    if(packet->tcpv4_ack && !packet->tcpv4_psh) {
        lock_acquire(connection->lock);

        // FIXME: error for ack packet for our fin packet.
        uint32_t recv_ack_number         = packet->tcpv4_acknowledgement_number;
        uint32_t expected_ack_number     = connection->local_sequence_number;
        boolean_t when_not_closing_state = recv_ack_number == expected_ack_number;
        boolean_t when_closing_state     = (connection->state == NETWORK_CONNECTION_STATE_FIN_WAIT_1 ||
                                            connection->state == NETWORK_CONNECTION_STATE_CLOSING) &&
                                           recv_ack_number + 1 == expected_ack_number;
        if (!(when_not_closing_state || when_closing_state)) {
            // This is a "blind" ACK or a duplicate from a previous step.
            // We ignore it to keep the state machine stable.
            PRINTLOG(NETWORK, LOG_WARNING, "received unexpected TCP ACK packet for connection with "
                                           "remote IP %u.%u.%u.%u and remote port %u, but ACK number %u "
                                           "does not match expected sequence number %u. Ignoring the ACK. state: %d",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->tcpv4_source_port,
                     packet->tcpv4_acknowledgement_number,
                     connection->local_sequence_number,
                     connection->state);
            lock_release(connection->lock);
            return 0;
        }

        if(connection->state == NETWORK_CONNECTION_STATE_LAST_ACK) {
            connection->state = NETWORK_CONNECTION_STATE_CLOSED;
            lock_release(connection->lock);
            return 0; // Connection is closed, no need to process further
        } else if(connection->state == NETWORK_CONNECTION_STATE_CLOSING) {
            connection->state = NETWORK_CONNECTION_STATE_TIME_WAIT;
            lock_release(connection->lock);
            return 0; // Connection is now in TIME_WAIT, no need to process further
        } else if(connection->state == NETWORK_CONNECTION_STATE_FIN_WAIT_1) {
            connection->state = NETWORK_CONNECTION_STATE_FIN_WAIT_2;
            lock_release(connection->lock);
            return 0; // Stay in FIN_WAIT_2 until we receive a FIN from remote
        } else if(connection->state == NETWORK_CONNECTION_STATE_FIN_WAIT_2) {
            // Stay in FIN_WAIT_2 until we receive a FIN from remote, then we will transition to TIME_WAIT
            lock_release(connection->lock);
            return 0;
        } else if(connection->state == NETWORK_CONNECTION_STATE_CLOSE_WAIT) {
            // Stay in CLOSE_WAIT until the application calls close, then we will send a FIN and transition to LAST_ACK
            lock_release(connection->lock);
            return 0;
        }

        if(connection->listener->role == NETWORK_LISTENER_ROLE_SERVER &&
           connection->state == NETWORK_CONNECTION_STATE_SYN_ACK_SENT) {
            connection->state                  = NETWORK_CONNECTION_STATE_ESTABLISHED;
            connection->remote_sequence_number = packet->tcpv4_sequence_number;
        }

        connection->remote_window_size = packet->tcpv4_window_size << connection->remote_window_scale;
        lock_release(connection->lock);
        return 0;
    }

    // After here only we need process PSH and PSH+ACK packets.
    if(!packet->tcpv4_psh) {
        PRINTLOG(NETWORK, LOG_WARNING, "received TCP packet without PSH flag set for connection with "
                                       "remote IP %u.%u.%u.%u and remote port %u. Ignoring the packet.",
                 source_ip.as_bytes[0],
                 source_ip.as_bytes[1],
                 source_ip.as_bytes[2],
                 source_ip.as_bytes[3],
                 packet->tcpv4_source_port);
        return 0;
    }

    // If the connection is not in an established state, we should not process data packets.
    // However, we might receive PSH+ACK during the closing handshake (e.g., in FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT).
    // In these states, we should still process incoming data if any, but not if the connection is already fully closed.
    lock_acquire(connection->lock);
    if(connection->state >= NETWORK_CONNECTION_STATE_CLOSE_WAIT) {
        PRINTLOG(NETWORK, LOG_WARNING, "received TCP PSH packet for connection that is closing or closed. "
                                       "Current state: %d. Dropping packet.", connection->state);
        lock_release(connection->lock);
        return 0;
    }
    lock_release(connection->lock);

    if(packet->tcpv4_sequence_number != connection->remote_sequence_number) {
        PRINTLOG(NETWORK, LOG_WARNING, "received out-of-order TCP packet for connection with "
                                       "remote IP %u.%u.%u.%u and remote port %u. "
                                       "Expected sequence: %u, received sequence: %u. Dropping packet.",
                 source_ip.as_bytes[0],
                 source_ip.as_bytes[1],
                 source_ip.as_bytes[2],
                 source_ip.as_bytes[3],
                 packet->tcpv4_source_port,
                 connection->remote_sequence_number,
                 packet->tcpv4_sequence_number);
        return 0; // Drop out-of-order packets for simplicity
    }

    PRINTLOG(NETWORK, LOG_DEBUG, "received TCP PSH packet for connection with remote IP %u.%u.%u.%u and remote port %u. Sequence: %u, Payload length: %u",
             source_ip.as_bytes[0],
             source_ip.as_bytes[1],
             source_ip.as_bytes[2],
             source_ip.as_bytes[3],
             packet->tcpv4_source_port,
             packet->tcpv4_sequence_number,
             packet->payload_len);

    // Update remote sequence number and window size
    connection->remote_sequence_number += packet->payload_len;
    connection->remote_window_size      = packet->tcpv4_window_size << connection->remote_window_scale;

    // Write payload to the read pipeline
    if(packet->payload_len > 0) {
        if(pipeline_write(connection->read_pipeline, packet->payload_len, packet->payload_offset) != packet->payload_len) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to write TCP payload to connection pipeline");
            return -1;
        }

        connection->local_window_size -= packet->payload_len; // Decrease local window size by the amount of data received
    }

    // Send an ACK for the received data
    if(network_tcpv4_build_and_send_ack_packet(ni, connection, packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send TCP ACK packet in response to received PSH packet");
        return -1;
    }

    return 0;
}

static int8_t network_udpv4_packet_handle(const network_info_t* ni, network_packet_t* packet) {
    network_ipv4_address_t destination_ip = packet->ipv4_destination_ip;

    network_listener_t* listener = network_listener_get(packet->ipv4_protocol, destination_ip, packet->udpv4_destination_port);

    if(!listener) {
        PRINTLOG(NETWORK, LOG_DEBUG, "no listener found for received UDPv4 packet with destination IP %u.%u.%u.%u and destination port %u for key 0x%llx",
                 destination_ip.as_bytes[0],
                 destination_ip.as_bytes[1],
                 destination_ip.as_bytes[2],
                 destination_ip.as_bytes[3],
                 packet->udpv4_destination_port,
                 ((uint64_t)destination_ip.as_dword << 16) | packet->udpv4_destination_port);
        return 0; // this is not an error, just no application is listening on this IP and port, so we can drop the packet silently
    }

    network_ipv4_address_t source_ip = packet->ipv4_source_ip;

    if(ni->is_ipv4_address_requested && !ni->is_ipv4_address_set &&
       packet->udpv4_source_port == 67 &&
       packet->udpv4_destination_port == 68) {
        source_ip = NETWORK_IPV4_GLOBAL_BROADCAST_IP;
    }

    uint64_t connection_key = ((uint64_t)source_ip.as_dword << 16) | packet->udpv4_source_port;

    network_connection_t* connection = (network_connection_t*)hashmap_get(listener->connections, (void*)connection_key);

    if(!connection) {
        if(listener->role != NETWORK_LISTENER_ROLE_SERVER) {
            PRINTLOG(NETWORK, LOG_ERROR, "no connection found for received UDPv4 packet with source IP %u.%u.%u.%u and source port %u, and listener is not a server",
                     source_ip.as_bytes[0],
                     source_ip.as_bytes[1],
                     source_ip.as_bytes[2],
                     source_ip.as_bytes[3],
                     packet->udpv4_source_port);
            return 0; // this is not an error, just no connection is associated with this source IP and port for this listener, so we can drop the packet silently
        }

        // create a new connection for this source IP and port
        // there is no sequence number for UDP, so we can set it to 0
        connection = network_connection_connect_internal(listener, packet->ipv4_source_ip, packet->udpv4_source_port, packet);

        if(!connection) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to create connection for received UDPv4 packet with source IP %u.%u.%u.%u and source port %u",
                     (packet->ipv4_source_ip.as_dword >> 24) & 0xFF,
                     (packet->ipv4_source_ip.as_dword >> 16) & 0xFF,
                     (packet->ipv4_source_ip.as_dword >> 8) & 0xFF,
                     packet->ipv4_source_ip.as_dword & 0xFF,
                     packet->udpv4_source_port);
            return -1; // this is an error because we expect to be able to create a connection for any incoming UDP packet to a server listener, so we return -1 to indicate an error
        }
    }

    if(connection->state != NETWORK_CONNECTION_STATE_ESTABLISHED) {
        PRINTLOG(NETWORK, LOG_ERROR, "received UDPv4 packet for connection that is not established. Current state: %d", connection->state);
        return -1; // this is an error because we expect to only receive UDP packets for established connections, so we return -1 to indicate an error
    }

    connection->last_remote_activity_timestamp = time_ns(NULL);

    pipeline_write(connection->read_pipeline, packet->payload_len, packet->payload_offset);

    return 0;
}
#pragma GCC diagnostic pop

int8_t network_connection_packet_handle(const network_info_t* ni, network_packet_t* packet) {
    if(!ni || !packet) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info or packet is NULL");
        return -1;
    }

    network_arp_cache_update(ni, packet->ipv4_source_ip, packet->ether_source_mac);

    if(packet->is_arp_packet) {
        return network_arp_packet_handle(ni, packet);
    } else if(packet->is_icmpv4_packet) {
        return network_icmpv4_packet_handle(ni, packet);
    } else if(packet->is_tcpv4_packet) {
        return network_tcpv4_packet_handle(ni, packet);
    } else if(packet->is_udpv4_packet) {
        return network_udpv4_packet_handle(ni, packet);
    }

    PRINTLOG(NETWORK, LOG_ERROR, "unsupported packet type for connection packet handle");
    return -1;
}

int8_t network_arp_query(const network_info_t* ni, network_ipv4_address_t target_ip) {
    if(!ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info is NULL");
        return -1;
    }

    network_packet_t arp_request_packet;
    memory_memclean(&arp_request_packet, sizeof(network_packet_t));

    arp_request_packet.ether_ethertype       = NETWORK_PROTOCOL_ARP;
    arp_request_packet.ether_inner_ethertype = NETWORK_PROTOCOL_ARP;
    memory_memcopy(ni->mac, arp_request_packet.ether_source_mac, sizeof(network_mac_address_t));
    memory_memcopy(BROADCAST_MAC, arp_request_packet.ether_destination_mac, sizeof(network_mac_address_t));

    arp_request_packet.is_arp_packet      = true;
    arp_request_packet.arp_operation_code = NETWORK_ARP_OPERATION_CODE_REQUEST;
    memory_memcopy(ni->mac, arp_request_packet.arp_source_mac, sizeof(network_mac_address_t));
    arp_request_packet.arp_source_ip = ni->ipv4_address;
    arp_request_packet.arp_target_ip = target_ip;

    if(network_packet_craft(ni, &arp_request_packet, NULL, 0) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to craft ARP request packet");
        return -1;
    }

    if(network_send_packet_to_return_queue(ni, &arp_request_packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to send ARP request packet to return queue");
        return -1;
    }

    return 0;
}

#define network_listener_create_ping_client(local_ip) \
        network_listener_get_or_create(NETWORK_LISTENER_ROLE_CLIENT, \
                                       NETWORK_PROTOCOL_IPV4, \
                                       NETWORK_IPV4_PROTOCOL_ICMPV4, \
                                       NULL, \
                                       local_ip, \
                                       0)

int8_t network_ping_send(network_ipv4_address_t destination_ip, uint16_t count, uint16_t interval_ms){
    network_info_t* ni = network_get_network_info_by_ipv4_address(destination_ip);

    if(!ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info not found for ip %i.%i.%i.%i",
                 destination_ip.as_bytes[0], destination_ip.as_bytes[1], destination_ip.as_bytes[2], destination_ip.as_bytes[3]);
        return -1;
    }

    network_mac_address_t destination_mac;
    network_arp_cache_wait_and_get(ni, destination_ip, destination_mac);

    network_listener_t* ping_listener = network_listener_create_ping_client(ni->ipv4_address);

    if(!ping_listener) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create ping listener");
        return -1;
    }

    // initialize lost echo replies count and last echo request sequence to detect lost echo replies
    ping_listener->icmpv4_last_echo_request_sequence = -1;

    uint8_t payload[56]; // 56 bytes of payload for ping
    for(uint8_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i); // fill payload with some data
    }

    for(uint32_t i = 0; i < count; i++) {
        time_t timestamp = time_ns(NULL);

        network_packet_t icmp_request_packet;
        memory_memclean(&icmp_request_packet, sizeof(network_packet_t));

        icmp_request_packet.ether_ethertype       = NETWORK_PROTOCOL_IPV4;
        icmp_request_packet.ether_inner_ethertype = NETWORK_PROTOCOL_IPV4;
        memory_memcopy(ping_listener->network_info->mac, icmp_request_packet.ether_source_mac, sizeof(network_mac_address_t));
        memory_memcopy(destination_mac, icmp_request_packet.ether_destination_mac, sizeof(network_mac_address_t));

        icmp_request_packet.is_ipv4_packet      = true;
        icmp_request_packet.ipv4_protocol       = NETWORK_IPV4_PROTOCOL_ICMPV4;
        icmp_request_packet.ipv4_source_ip      = ping_listener->local_ip;
        icmp_request_packet.ipv4_destination_ip = destination_ip;

        icmp_request_packet.is_icmpv4_packet           = true;
        icmp_request_packet.is_icmpv4_ping_packet      = true;
        icmp_request_packet.icmpv4_type                = NETWORK_ICMPV4_TYPE_ECHO_REQUEST;
        icmp_request_packet.icmpv4_code                = NETWORK_ICMPV4_ECHO_REQUEST_REPLY_CODE_NO_CODE;
        icmp_request_packet.icmpv4_identifier          = ping_listener->local_port; // use local port as identifier to match reply
        icmp_request_packet.icmpv4_sequence            = i;
        icmp_request_packet.icmpv4_ping_timestamp_sec  = timestamp / 1000000000ULL;
        icmp_request_packet.icmpv4_ping_timestamp_usec = (timestamp % 1000000000ULL) / 1000ULL;

        if(network_packet_craft(ping_listener->network_info, &icmp_request_packet, payload, sizeof(payload)) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to craft ICMPv4 echo request packet");
            network_listener_destroy(ping_listener);
            return -1;
        }

        if(network_send_packet_to_return_queue(ping_listener->network_info, &icmp_request_packet) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to send ICMPv4 echo request packet to return queue");
            network_listener_destroy(ping_listener);
            return -1;
        }

        task_msleep(interval_ms);
    }

    ping_listener->icmpv4_total_round_trip_time /= count; // calculate average round trip time

    PRINTLOG(NETWORK, LOG_INFO, "ping sent to %i.%i.%i.%i: count=%u, interval=%ums",
             destination_ip.as_bytes[0], destination_ip.as_bytes[1], destination_ip.as_bytes[2], destination_ip.as_bytes[3],
             count, interval_ms);
    PRINTLOG(NETWORK, LOG_INFO, "ping statistics for %i.%i.%i.%i:",
             destination_ip.as_bytes[0], destination_ip.as_bytes[1], destination_ip.as_bytes[2], destination_ip.as_bytes[3]);
    PRINTLOG(NETWORK, LOG_INFO, "    packets sent: %u", count);
    PRINTLOG(NETWORK, LOG_INFO, "    packets received: %u", count - ping_listener->icmpv4_lost_echo_replies);
    PRINTLOG(NETWORK, LOG_INFO, "    packets lost: %u", ping_listener->icmpv4_lost_echo_replies);
    PRINTLOG(NETWORK, LOG_INFO, "    average round trip time: %lluns %lluus",
             ping_listener->icmpv4_total_round_trip_time, ping_listener->icmpv4_total_round_trip_time / 1000ULL);

    network_listener_destroy(ping_listener);

    return 0;
}
