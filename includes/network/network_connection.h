/**
 * @file network_connection.h
 * @brief Network connection related definitions.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_CONNECTION_H
#define ___NETWORK_CONNECTION_H 0

#include <types.h>
#include <network/network_packet.h>
#include <network/network_info.h>
#include <iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum network_listener_role_t :uint8_t {
    NETWORK_LISTENER_ROLE_SERVER,
    NETWORK_LISTENER_ROLE_CLIENT,
} network_listener_role_t;

typedef struct network_listener_t network_listener_t;

network_protocol_t      network_listener_get_protocol(const network_listener_t* listener);
network_ipv4_protocol_t network_listener_get_ipv4_protocol(const network_listener_t* listener);
network_ipv4_address_t  network_listener_get_local_ip(const network_listener_t* listener);
uint16_t                network_listener_get_local_port(const network_listener_t* listener);

iterator_t* network_listener_get_all(void);

network_listener_t* network_listener_get_or_create(network_listener_role_t role,
                                                   network_protocol_t      protocol,
                                                   network_ipv4_protocol_t ipv4_protocol,
                                                   const network_info_t*   network_info,
                                                   network_ipv4_address_t  local_ip,
                                                   uint16_t                local_port);

#define network_listener_create_tcpv4_server(network_info, local_ip, local_port) \
        network_listener_get_or_create(NETWORK_LISTENER_ROLE_SERVER, \
                                       NETWORK_PROTOCOL_IPV4, \
                                       NETWORK_IPV4_PROTOCOL_TCPV4, \
                                       network_info, \
                                       local_ip, \
                                       local_port)

#define network_listener_create_tcpv4_client(network_info, local_ip, local_port) \
        network_listener_get_or_create(NETWORK_LISTENER_ROLE_CLIENT, \
                                       NETWORK_PROTOCOL_IPV4, \
                                       NETWORK_IPV4_PROTOCOL_TCPV4, \
                                       network_info, \
                                       local_ip, \
                                       local_port)

#define network_listener_create_udpv4_server(network_info, local_ip, local_port) \
        network_listener_get_or_create(NETWORK_LISTENER_ROLE_SERVER, \
                                       NETWORK_PROTOCOL_IPV4, \
                                       NETWORK_IPV4_PROTOCOL_UDPV4, \
                                       network_info, \
                                       local_ip, \
                                       local_port)

#define network_listener_create_udpv4_client(network_info, local_ip, local_port) \
        network_listener_get_or_create(NETWORK_LISTENER_ROLE_CLIENT, \
                                       NETWORK_PROTOCOL_IPV4, \
                                       NETWORK_IPV4_PROTOCOL_UDPV4, \
                                       network_info, \
                                       local_ip, \
                                       local_port)

void network_listener_destroy(network_listener_t* listener);

boolean_t network_listener_exists(network_listener_role_t role,
                                  network_protocol_t      protocol,
                                  network_ipv4_protocol_t ipv4_protocol,
                                  network_ipv4_address_t  local_ip,
                                  uint16_t                local_port);

typedef struct network_connection_t network_connection_t;

network_connection_t* network_connection_connect(network_listener_t*    listener,
                                                 network_ipv4_address_t remote_ip,
                                                 uint16_t               remote_port);

network_connection_t* network_connection_accept(network_listener_t* listener);

int32_t network_connection_send(network_connection_t* connection,
                                uint8_t*              data,
                                uint32_t              data_len);

int32_t network_connection_receive(network_connection_t* connection,
                                   uint8_t*              buffer,
                                   uint32_t              buffer_len);

void network_connection_close(network_connection_t* connection);

void network_connection_destroy(network_connection_t* connection);

network_ipv4_address_t network_connection_get_remote_ip(const network_connection_t* connection);

uint16_t network_connection_get_remote_port(const network_connection_t* connection);

network_listener_t* network_connection_get_listener(const network_connection_t* connection);

int8_t network_connection_packet_handle(const network_info_t* ni, network_packet_t* packet);

int8_t network_arp_query(const network_info_t* ni, network_ipv4_address_t target_ip);

void network_arp_cache_wait_and_get(const network_info_t* ni, network_ipv4_address_t ip, network_mac_address_t mac);

int8_t network_ping_send(network_ipv4_address_t destination_ip, uint16_t count, uint16_t interval_ms);

#ifdef __cplusplus
}
#endif

#endif
