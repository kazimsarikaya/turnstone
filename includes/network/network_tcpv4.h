/**
 * @file network_tcpv4.h
 * @brief TCPv4 header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_TCPV4_H
#define ___NETWORK_TCPV4_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>
#include <hashmap.h>


typedef struct network_tcpv4_header_t {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence_number;
    uint32_t acknowledgement_number;
    uint8_t  ns            : 1;
    uint8_t  reserved      : 3;
    uint8_t  header_length : 4;
    uint8_t  fin           : 1;
    uint8_t  syn           : 1;
    uint8_t  rst           : 1;
    uint8_t  psh           : 1;
    uint8_t  ack           : 1;
    uint8_t  urg           : 1;
    uint8_t  ece           : 1;
    uint8_t  cwr           : 1;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
}__attribute__((packed)) network_tcpv4_header_t;

typedef enum network_tcp_connection_state_t {
    NETWORK_TCP_CONNECTION_STATE_CLOSED,
    NETWORK_TCP_CONNECTION_STATE_LISTEN,
    NETWORK_TCP_CONNECTION_STATE_SYN_SENT,
    NETWORK_TCP_CONNECTION_STATE_SYN_RECEIVED,
    NETWORK_TCP_CONNECTION_STATE_ESTABLISHED,
    NETWORK_TCP_CONNECTION_STATE_FIN_WAIT_1,
    NETWORK_TCP_CONNECTION_STATE_FIN_WAIT_2,
    NETWORK_TCP_CONNECTION_STATE_CLOSE_WAIT,
    NETWORK_TCP_CONNECTION_STATE_CLOSING,
    NETWORK_TCP_CONNECTION_STATE_LAST_ACK,
    NETWORK_TCP_CONNECTION_STATE_TIME_WAIT
} network_tcp_connection_state_t;

typedef struct network_tcpv4_connection_t {
    network_ipv4_address_t         local_ip;
    network_ipv4_address_t         remote_ip;
    uint16_t                       local_port;
    uint16_t                       remote_port;
    uint32_t                       local_sequence_number;
    uint32_t                       remote_sequence_number;
    uint32_t                       local_acknowledgement_number;
    uint32_t                       remote_acknowledgement_number;
    uint16_t                       window_size;
    network_tcp_connection_state_t state;
} network_tcpv4_connection_t;

typedef struct network_tcpv4_listener_t {
    network_ipv4_address_t local_ip;
    uint16_t               local_port;
    hashmap_t*             connections;
} network_tcpv4_listener_t;

uint8_t* network_tcpv4_process_packet(network_ipv4_address_t dip, network_ipv4_address_t sip, network_tcpv4_header_t* recv_tcpv4_packet, void* network_info, uint16_t packet_len, uint16_t* return_packet_len);

#endif
