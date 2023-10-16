/**
 * @file network_udpv4.h
 * @brief UDPv4 header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_UDPV4_H
#define ___NETWORK_UDPV4_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>

#define NETWORK_APPLICATION_PORT_ECHO_SERVER 7
#define NETWORK_APPLICATION_PORT_DHCP_CLIENT 68

typedef struct network_udpv4_header_t {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
}__attribute__((packed)) network_udpv4_header_t;


uint8_t*                network_udpv4_process_packet(network_udpv4_header_t* recv_udpv4_packet, void* network_info, uint16_t* return_packet_len);
network_udpv4_header_t* network_udpv4_create_packet_from_data(uint16_t sp, uint16_t dp, uint16_t len, uint8_t* data);

#endif
