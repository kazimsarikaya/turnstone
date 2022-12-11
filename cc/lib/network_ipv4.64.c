/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_ipv4.h>
#include <network/network_icmpv4.h>
#include <utils.h>
#include <memory.h>
#include <video.h>
#include <time.h>
#include <video.h>

ipv4_address_t NETWORK_TEST_OUR_STATIC_IP = {192, 168, 122, 5};
ipv4_address_t NETWORK_TEST_OUR_BROADCAST_IP = {192, 168, 122, 255};

network_ipv4_header_t* network_ipv4_create_packet_from_icmp_packet(ipv4_address_t sip, ipv4_address_t dip, network_icmpv4_header_t* icmp_hdr, uint16_t icmp_packet_len);

boolean_t network_ipv4_is_address_eq(ipv4_address_t ipv4_addr1, ipv4_address_t ipv4_addr2) {
    for(uint64_t i = 0; i < sizeof(ipv4_address_t); i++) {
        if(ipv4_addr1[i] != ipv4_addr2[i]) {
            return false;
        }
    }

    return true;
}

uint16_t network_ipv4_header_checksum(network_ipv4_header_t* ipv4_hdr){
    uint8_t* data_tmp = (uint8_t*)ipv4_hdr;
    uint16_t* data = (uint16_t*)data_tmp;
    uint32_t csum = 0;

    for(uint16_t i = 0; i < ipv4_hdr->header_length * 2; i++) {
        if(i != 10 && i != 11) {
            csum += data[i];

            uint32_t carry = csum >> 16;
            csum &= 0xFFFF;
            csum += carry;
        }
    }

    int16_t res = csum;

    ipv4_hdr->header_checksum = ~res;

    return ~res;
}

int8_t network_ipv4_header_checksum_verify(network_ipv4_header_t* ipv4_hdr){
    uint8_t* data_tmp = (uint8_t*)ipv4_hdr;
    uint16_t* data = (uint16_t*)data_tmp;
    uint32_t csum = 0;

    for(uint16_t i = 0; i < ipv4_hdr->header_length * 2; i++) {
        csum += data[i];

        uint32_t carry = csum >> 16;
        csum &= 0xFFFF;
        csum += carry;
    }

    int16_t res = csum;

    return (~res == 0)?0:-1;
}

uint8_t* network_ipv4_process_packet(network_ipv4_header_t* recv_ipv4_packet, void* network_info, uint16_t* return_packet_len) {
    UNUSED(network_info);

    if(return_packet_len == NULL) {
        return NULL;
    }

    if(network_ipv4_header_checksum_verify(recv_ipv4_packet) != 0) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet checksum failed");

        return NULL;
    }

    if(network_ipv4_is_address_eq(NETWORK_TEST_OUR_STATIC_IP, recv_ipv4_packet->destination_ip) != 0 &&
       network_ipv4_is_address_eq(NETWORK_TEST_OUR_BROADCAST_IP, recv_ipv4_packet->destination_ip) != 0) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet destination isnot us");
        return NULL;
    }

    if(recv_ipv4_packet->protocol == NETWORK_IPV4_PROTOCOL_ICMPV4) {
        uint8_t* data = (uint8_t*)recv_ipv4_packet;
        data += recv_ipv4_packet->header_length * 4;
        network_icmpv4_header_t* recv_icmp_hdr = (network_icmpv4_header_t*)data;

        uint16_t pp_len = 0;
        uint16_t ping_data_len = BYTE_SWAP16(recv_ipv4_packet->total_length) - ((recv_ipv4_packet->header_length * 4) + sizeof(network_icmpv4_header_t));

        network_icmpv4_header_t* resp_icmp_hdr = (network_icmpv4_header_t*)network_icmpv4_process_packet(recv_icmp_hdr, &ping_data_len, &pp_len);

        network_ipv4_header_t* ip = network_ipv4_create_packet_from_icmp_packet(NETWORK_TEST_OUR_STATIC_IP, recv_ipv4_packet->source_ip, resp_icmp_hdr, pp_len);

        *return_packet_len = BYTE_SWAP16(ip->total_length);

        return (uint8_t*)ip;
    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "unimplemented ipv4 protocol 0x%02x", recv_ipv4_packet->protocol);
    }

    return NULL;
}

network_ipv4_header_t* network_ipv4_create_packet_from_icmp_packet(ipv4_address_t sip, ipv4_address_t dip, network_icmpv4_header_t* icmp_hdr, uint16_t icmp_packet_len) {
    uint16_t packet_len = sizeof(network_ipv4_header_t) + icmp_packet_len;

    network_ipv4_header_t* ipv4_packet = memory_malloc(packet_len);

    ipv4_packet->version = NETWORK_IPV4_VERSION;
    ipv4_packet->header_length = 5;
    ipv4_packet->total_length = BYTE_SWAP16(packet_len);
    ipv4_packet->ttl = NETWORK_IPV4_TTL;
    ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_ICMPV4;

    memory_memcopy(sip, ipv4_packet->source_ip, sizeof(ipv4_address_t));
    memory_memcopy(dip, ipv4_packet->destination_ip, sizeof(ipv4_address_t));

    network_ipv4_header_checksum(ipv4_packet);

    uint8_t* buf = (uint8_t*)ipv4_packet;
    buf += ipv4_packet->header_length * 4;

    memory_memcopy((void*)icmp_hdr, buf, icmp_packet_len);

    memory_free(icmp_hdr);

    return ipv4_packet;
}

/*

   network_ipv4_header_t* network_ipv4_create_packet_from_udp_packet(ipv4_address_t sip, ipv4_address_t dip, network_udp_header_t* udp_hdr){
    uint16_t packet_len = sizeof(network_ipv4_header_t) + BYTE_SWAP16(udp_hdr->length);

    network_ipv4_header_t* ipv4_packet = memory_malloc(packet_len);

    ipv4_packet->version = NETWORK_IPV4_VERSION;
    ipv4_packet->header_length = 5;
    ipv4_packet->total_length = BYTE_SWAP16(packet_len);
    ipv4_packet->ttl = NETWORK_IPV4_TTL;
    ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_UDP;

    memory_memcopy(sip, ipv4_packet->source_ip, sizeof(ipv4_address_t));
    memory_memcopy(dip, ipv4_packet->destination_ip, sizeof(ipv4_address_t));

    network_ipv4_header_checksum(ipv4_packet);

    uint16_t* sip_u16 = (uint16_t*)sip;
    uint16_t* dip_u16 = (uint16_t*)dip;

    uint32_t hcsum = udp_hdr->checksum + sip_u16[0] + sip_u16[1] + dip_u16[0] + dip_u16[1];
    uint32_t carry = hcsum >> 16;
    hcsum &= 0xFFFF;
    hcsum += carry;

    int16_t csum = hcsum;

    udp_hdr->checksum = ~csum;

    uint8_t* buf = (uint8_t*)ipv4_packet;
    buf += ipv4_packet->header_length * 4;

    memory_memcopy((void*)udp_hdr, buf, BYTE_SWAP16(udp_hdr->length));

    memory_free(udp_hdr);


    return ipv4_packet;
   }
 */
