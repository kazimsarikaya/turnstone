/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_udpv4.h>
#include <network/network_ipv4.h>
#include <utils.h>
#include <memory.h>
#include <video.h>
#include <time.h>
#include <video.h>

network_udpv4_header_t* network_create_udp_packet_from_data(uint16_t sp, uint16_t dp, uint16_t len, uint8_t* data){
    uint16_t packet_len = sizeof(network_udpv4_header_t) + len;

    network_udpv4_header_t* res = memory_malloc(packet_len);

    res->source_port = BYTE_SWAP16(sp);
    res->destination_port = BYTE_SWAP16(dp);
    res->length = BYTE_SWAP16(packet_len);

    uint32_t hcsum = res->source_port + res->destination_port + res->length * 2 + BYTE_SWAP16(NETWORK_IPV4_PROTOCOL_UDPV4);

    boolean_t single_byte = len & 1;
    uint16_t tmp_len = len - single_byte;

    for(uint32_t i = 0; i < tmp_len; i += 2) {
        hcsum += (data[i + 1] << 8) | data[i];

        uint32_t carry = hcsum >> 16;
        hcsum &= 0xFFFF;
        hcsum += carry;
    }

    if(single_byte) {
        hcsum += data[len - 1];

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
