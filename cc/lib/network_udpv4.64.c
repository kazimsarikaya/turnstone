/**
 * @file network_udpv4.64.c
 * @brief UDPv4 protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_udpv4.h>
#include <network/network_ipv4.h>
#include <network/network_dhcpv4.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>
#include <time.h>
#include <logging.h>

MODULE("turnstone.lib");

uint8_t* network_udpv4_process_packet(network_udpv4_header_t* recv_udpv4_packet, void* network_info, uint16_t* return_packet_len) {
    UNUSED(network_info);
    UNUSED(return_packet_len);

    if(recv_udpv4_packet == NULL) {
        return NULL;
    }

    uint8_t* data = (uint8_t*)recv_udpv4_packet;
    data += sizeof(network_udpv4_header_t);
    uint16_t data_len = BYTE_SWAP16(recv_udpv4_packet->length) - sizeof(network_udpv4_header_t);

    uint16_t dport = BYTE_SWAP16(recv_udpv4_packet->destination_port);
    uint16_t sport = BYTE_SWAP16(recv_udpv4_packet->source_port);


    PRINTLOG(NETWORK, LOG_TRACE, "udpv4 packet dest port %i data len %i", dport, data_len);


    if(dport == NETWORK_APPLICATION_PORT_ECHO_SERVER) {
        return (uint8_t*)network_udpv4_create_packet_from_data(dport, sport, data_len, data);
    } else if(dport == NETWORK_APPLICATION_PORT_DHCP_CLIENT) {
        if(data_len < sizeof(network_dhcpv4_t)) {
            return NULL;
        }

        network_dhcpv4_process_packet((network_dhcpv4_t*)data, network_info, NULL);
    }

    return NULL;
}

network_udpv4_header_t* network_udpv4_create_packet_from_data(uint16_t sp, uint16_t dp, uint16_t len, uint8_t* data) {
    uint16_t packet_len = sizeof(network_udpv4_header_t) + len;

    network_udpv4_header_t* res = memory_malloc(packet_len);

    if(res == NULL) {
        return NULL;
    }

    res->source_port = BYTE_SWAP16(sp);
    res->destination_port = BYTE_SWAP16(dp);
    res->length = BYTE_SWAP16(packet_len);

    uint32_t hcsum = res->source_port + res->destination_port + res->length * 2 + BYTE_SWAP16(NETWORK_IPV4_PROTOCOL_UDPV4);

    boolean_t single_byte = len & 1;
    uint16_t tmp_len = len - single_byte;

    for(uint32_t i = 0; i < tmp_len; i += 2) {
        hcsum += (data[i + 1] << 8) | data[i];
    }

    if(single_byte) {
        hcsum += data[len - 1];
    }

    while(hcsum >> 16) {
        uint32_t carry = hcsum >> 16;
        hcsum &= 0xFFFF;
        hcsum += carry;
    }

    res->checksum = hcsum;

    uint8_t* udp_buffer = (uint8_t*)res;
    udp_buffer += sizeof(network_udpv4_header_t);

    memory_memcopy(data, udp_buffer, len);

    return res;
}
