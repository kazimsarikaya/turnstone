/**
 * @file network_icmpv4.64.c
 * @brief ICMPv4 protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_icmpv4.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>
#include <time.h>
#include <logging.h>

MODULE("turnstone.lib.network");

network_icmpv4_ping_header_t* network_create_ping_packet(boolean_t is_reply, uint16_t identifier, uint16_t sequence, uint16_t data_len, uint8_t* data, uint16_t* packet_len);


uint8_t* network_icmpv4_process_packet(network_icmpv4_header_t* recv_icmpv4_packet, void* network_info, uint16_t* return_packet_len) {
    if(return_packet_len == NULL) {
        return NULL;
    }


    if(recv_icmpv4_packet->type == NETWORK_ICMP_ECHO_REQUEST && recv_icmpv4_packet->code == NETWORK_ICMP_ECHO_CODE) {
        uint8_t* data = (uint8_t*)recv_icmpv4_packet;
        data += sizeof(network_icmpv4_header_t);
        uint16_t ping_data_len = *((uint16_t*)network_info);

        network_icmpv4_ping_header_t* ping = network_create_ping_packet(1, recv_icmpv4_packet->identifier, recv_icmpv4_packet->sequence, ping_data_len, data, return_packet_len);


        return (uint8_t*)ping;
    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "unimplemented icmp type 0x%02x code 0x%02x", recv_icmpv4_packet->type, recv_icmpv4_packet->code);
    }


    return NULL;
}


network_icmpv4_ping_header_t* network_create_ping_packet(boolean_t is_reply, uint16_t identifier, uint16_t sequence, uint16_t data_len, uint8_t* data, uint16_t* packet_len) {
    uint16_t plen = 0;
    uint16_t data_offset = 0;

    if(is_reply) {
        plen = sizeof(network_icmpv4_header_t) + data_len;
        data_offset = sizeof(network_icmpv4_header_t);
    } else {
        plen = sizeof(network_icmpv4_ping_header_t) + data_len;
        data_offset = sizeof(network_icmpv4_ping_header_t);
    }

    network_icmpv4_ping_header_t* pp = memory_malloc(plen);

    if(pp == NULL) {
        return NULL;
    }

    pp->header.type = is_reply?NETWORK_ICMP_ECHO_REPLY:NETWORK_ICMP_ECHO_REQUEST;
    pp->header.code = NETWORK_ICMP_ECHO_CODE;
    pp->header.identifier = identifier;
    pp->header.sequence = sequence;

    if(is_reply == 0) {
        uint64_t ts = time_ns(NULL);
        uint32_t sec = ts / (1000 * 1000 * 1000);
        pp->timestamp_sec = BYTE_SWAP32(sec);
        uint32_t usec = (ts % (1000 * 1000 * 1000)) / (1000);
        pp->timestamp_usec = BYTE_SWAP32(usec);
    }

    uint8_t* pp_data = (uint8_t*)pp;

    memory_memcopy(data, pp_data + data_offset, data_len);

    uint16_t* pp_data_16 = (uint16_t*)pp_data;

    uint32_t csum = 0;

    for(uint16_t i = 0; i < plen / 2; i++) {
        csum += pp_data_16[i];

        uint32_t carry = csum >> 16;
        csum &= 0xFFFF;
        csum += carry;
    }

    int16_t res = csum;

    pp->header.checksum = ~res;

    if(packet_len) {
        *packet_len = plen;
    }

    return (network_icmpv4_ping_header_t*)pp;
}
