/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/network_protocols.h>
#include <utils.h>
#include <memory.h>
#include <video.h>
#include <time.h>
#include <video.h>

mac_address_t BROADCAST_MAC = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
ipv4_address_t NETWORK_TEST_OUR_STATIC_IP = {192, 168, 122, 5};
ipv4_address_t NETWORK_TEST_OUR_BROADCAST_IP = {192, 168, 122, 255};

int8_t network_is_mac_address_eq(mac_address_t mac1, mac_address_t mac2){
    uint8_t* mac1_b = (uint8_t*) mac1;
    uint8_t* mac2_b = (uint8_t*) mac2;

    for(uint64_t i = 0; i < sizeof(mac_address_t); i++) {
        if(mac1_b[i] != mac2_b[i]) {
            return -1;
        }
    }

    return 0;
}

int8_t network_is_ipv4_address_eq(ipv4_address_t ipv4_addr1, ipv4_address_t ipv4_addr2){
    uint8_t* ipv4_addr1_b = (uint8_t*) ipv4_addr1;
    uint8_t* ipv4_addr2_b = (uint8_t*) ipv4_addr2;

    for(uint64_t i = 0; i < sizeof(ipv4_address_t); i++) {
        if(ipv4_addr1_b[i] != ipv4_addr2_b[i]) {
            return -1;
        }
    }

    return 0;
}

network_ethernet_t*  network_process_packet(network_received_packet_t* packet, uint16_t* return_packet_len) {
    network_ethernet_t* recv_eth_packet = (network_ethernet_t*)packet->packet_data;

    uint16_t packet_type = BYTE_SWAP16(recv_eth_packet->type);

    if(network_is_mac_address_eq(recv_eth_packet->destination, packet->device_mac_address) != 0 &&
       network_is_mac_address_eq(recv_eth_packet->destination, BROADCAST_MAC) != 0) {
        PRINTLOG(NETWORK, LOG_TRACE, "destination is not this machine, discarding packet");

        return NULL;
    }

    network_ethernet_t* res = NULL;
    uint16_t res_packet_len = 0;

    if(packet_type == NETWORK_PROTOCOL_ARP) {
        PRINTLOG(NETWORK, LOG_TRACE, "arp packet received");

        res = network_create_arp_reply_from_packet(packet);
        res_packet_len = NETWORK_ARP_ETHERNET_PACKET_LEN;

    } else if(packet_type == NETWORK_PROTOCOL_IP) {
        res = network_process_ipv4_packet_from_packet(packet, &res_packet_len);
    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "unimplemented packet type 0x%04x", packet_type);
    }

    if(return_packet_len == NULL) {
        if(res) {
            memory_free(res);
        }
    } else {
        *return_packet_len = res_packet_len;
    }

    return res;
}

network_ethernet_t* network_process_ipv4_packet_from_packet(network_received_packet_t* packet, uint16_t* return_packet_len){
    uint8_t* offset = (uint8_t*)packet->packet_data;

    network_ethernet_t* eth_packet = (network_ethernet_t*)offset;
    offset += sizeof(network_ethernet_t);
    network_ipv4_header_t* ipv4_hdr = (network_ipv4_header_t*)offset;

    if(network_ipv4_header_checksum_verify(ipv4_hdr) != 0) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet checksum failed");

        return NULL;
    }

    if(network_is_ipv4_address_eq(NETWORK_TEST_OUR_STATIC_IP, ipv4_hdr->destination_ip) != 0 &&
       network_is_ipv4_address_eq(NETWORK_TEST_OUR_BROADCAST_IP, ipv4_hdr->destination_ip) != 0) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet destination isnot us");
        return NULL;
    }

    if(ipv4_hdr->protocol == NETWORK_IPV4_PROTOCOL_ICMP) {
        offset += ipv4_hdr->header_length * 4;
        network_icmp_header_t* icmp_hdr = (network_icmp_header_t*)offset;

        if(icmp_hdr->type == NETWORK_ICMP_ECHO_REQUEST && icmp_hdr->code == NETWORK_ICMP_ECHO_CODE) {
            uint16_t pp_len = 0;
            offset += sizeof(network_icmp_header_t);
            uint16_t ping_data_len = BYTE_SWAP16(ipv4_hdr->total_length) - ((ipv4_hdr->header_length * 4) + sizeof(network_icmp_header_t));

            network_icmp_ping_header_t* ping = network_create_ping_packet(1, icmp_hdr->identifier, icmp_hdr->sequence, ping_data_len, offset, &pp_len);
            network_ipv4_header_t* ip = network_create_ipv4_packet_from_icmp_packet(NETWORK_TEST_OUR_STATIC_IP, ipv4_hdr->source_ip, (network_icmp_header_t*)ping, pp_len);
            network_ethernet_t* reply_eth_packet = network_create_ethernet_packet_from_ipv4_packet(eth_packet->source, packet->device_mac_address, ip, return_packet_len);

            return reply_eth_packet;
        } else {
            PRINTLOG(NETWORK, LOG_TRACE, "unimplemented icmp type 0x%02x code 0x%02x", icmp_hdr->type, icmp_hdr->code);
        }


    } else {
        PRINTLOG(NETWORK, LOG_TRACE, "unimplemented ipv4 protocol 0x%02x", ipv4_hdr->protocol);
    }

    return NULL;
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

network_icmp_ping_header_t* network_create_ping_packet(boolean_t is_reply, uint16_t identifier, uint16_t sequence, uint16_t data_len, uint8_t* data, uint16_t* packet_len){
    uint16_t plen = 0;
    uint16_t data_offset = 0;

    if(is_reply) {
        plen = sizeof(network_icmp_header_t) + data_len;
        data_offset = sizeof(network_icmp_header_t);
    } else {
        plen = sizeof(network_icmp_ping_header_t) + data_len;
        data_offset = sizeof(network_icmp_ping_header_t);
    }

    network_icmp_ping_header_t* pp = memory_malloc(plen);

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

    return (network_icmp_ping_header_t*)pp;
}

network_ethernet_t* network_create_ethernet_packet_from_ipv4_packet(mac_address_t dst, mac_address_t src, network_ipv4_header_t* ipv4_hdr, uint16_t* len){
    if(len == NULL) {
        return NULL;
    }

    *len = sizeof(network_ethernet_t) + BYTE_SWAP16(ipv4_hdr->total_length);

    network_ethernet_t* eth_packet = memory_malloc(*len);

    memory_memcopy(dst, eth_packet->destination, sizeof(mac_address_t));
    memory_memcopy(src, eth_packet->source, sizeof(mac_address_t));
    eth_packet->type = BYTE_SWAP16(NETWORK_ETHERNET_TYPE_IP);

    uint8_t* buf = (uint8_t*)eth_packet;

    memory_memcopy(ipv4_hdr, buf + sizeof(network_ethernet_t), BYTE_SWAP16(ipv4_hdr->total_length));

    memory_free(ipv4_hdr);

    return eth_packet;

}

network_udp_header_t* network_create_udp_packet_from_data(uint16_t sp, uint16_t dp, uint16_t len, uint8_t* data){
    uint16_t packet_len = sizeof(network_udp_header_t) + len;

    network_udp_header_t* res = memory_malloc(packet_len);

    res->source_port = BYTE_SWAP16(sp);
    res->destination_port = BYTE_SWAP16(dp);
    res->length = BYTE_SWAP16(packet_len);

    uint32_t hcsum = res->source_port + res->destination_port + res->length * 2 + BYTE_SWAP16(NETWORK_IPV4_PROTOCOL_UDP);

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
    udp_buffer += sizeof(network_udp_header_t);

    memory_memcopy(data, udp_buffer, len);

    return res;
}


network_ipv4_header_t* network_create_ipv4_packet_from_udp_packet(ipv4_address_t sip, ipv4_address_t dip, network_udp_header_t* udp_hdr){
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

network_ipv4_header_t* network_create_ipv4_packet_from_icmp_packet(ipv4_address_t sip, ipv4_address_t dip, network_icmp_header_t* icmp_hdr, uint16_t icmp_packet_len){
    uint16_t packet_len = sizeof(network_ipv4_header_t) + icmp_packet_len;

    network_ipv4_header_t* ipv4_packet = memory_malloc(packet_len);

    ipv4_packet->version = NETWORK_IPV4_VERSION;
    ipv4_packet->header_length = 5;
    ipv4_packet->total_length = BYTE_SWAP16(packet_len);
    ipv4_packet->ttl = NETWORK_IPV4_TTL;
    ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_ICMP;

    memory_memcopy(sip, ipv4_packet->source_ip, sizeof(ipv4_address_t));
    memory_memcopy(dip, ipv4_packet->destination_ip, sizeof(ipv4_address_t));

    network_ipv4_header_checksum(ipv4_packet);

    uint8_t* buf = (uint8_t*)ipv4_packet;
    buf += ipv4_packet->header_length * 4;

    memory_memcopy((void*)icmp_hdr, buf, icmp_packet_len);

    memory_free(icmp_hdr);

    return ipv4_packet;
}

network_ethernet_t* network_create_arp_reply_from_packet(network_received_packet_t* packet) {
    network_arp_t* arp_packet = (network_arp_t*)(packet->packet_data + sizeof(network_ethernet_t));

    if(network_is_ipv4_address_eq(NETWORK_TEST_OUR_STATIC_IP, arp_packet->target_ip) != 0) {
        return NULL;
    }

    uint8_t* buffer = memory_malloc(packet->packet_len);
    memory_memcopy(packet->packet_data, buffer, packet->packet_len);

    arp_packet = (network_arp_t*)(buffer + sizeof(network_ethernet_t));

    memory_memcopy(arp_packet->source_ip, arp_packet->target_ip, sizeof(ipv4_address_t));
    memory_memcopy(arp_packet->source_mac, arp_packet->target_mac, sizeof(mac_address_t));

    memory_memcopy(packet->device_mac_address, arp_packet->source_mac, sizeof(mac_address_t));
    memory_memcopy(NETWORK_TEST_OUR_STATIC_IP, arp_packet->source_ip, sizeof(ipv4_address_t));

    arp_packet->operation_code = BYTE_SWAP16(NETWORK_ARP_OPERATION_CODE_ANSWER);

    network_ethernet_t* eth_packet = (network_ethernet_t*)buffer;

    memory_memcopy(eth_packet->source, eth_packet->destination, sizeof(mac_address_t));
    memory_memcopy(packet->device_mac_address, eth_packet->source, sizeof(mac_address_t));

    return eth_packet;
}

network_ethernet_t* network_create_arp_request(mac_address_t src_mac, ipv4_address_t src_ip, ipv4_address_t tgt_ip){
    network_arp_t* arp_packet = memory_malloc(sizeof(network_arp_t));

    arp_packet->hardware_type = BYTE_SWAP16(NETWORK_ARP_HARDWARE_TYPE_ETHERNET);
    arp_packet->protocol_type = BYTE_SWAP16(NETWORK_ARP_PROTOCOL_TYPE_IP);
    arp_packet->hardware_address_length = NETWORK_ARP_HARDWARE_ADDRESS_LENGTH;
    arp_packet->protocol_address_length = NETWORK_ARP_PROTOCOL_ADDRESS_LENGTH;
    arp_packet->operation_code = BYTE_SWAP16(NETWORK_ARP_OPERATION_CODE_REQUEST);

    memory_memcopy(src_mac, arp_packet->source_mac, sizeof(mac_address_t));
    memory_memcopy(src_ip, arp_packet->source_ip, sizeof(ipv4_address_t));
    memory_memcopy(tgt_ip, arp_packet->target_ip, sizeof(ipv4_address_t));

    network_ethernet_t* eth_packet = memory_malloc(sizeof(network_ethernet_t) + sizeof(network_arp_t));

    memory_memcopy(BROADCAST_MAC, eth_packet->destination, sizeof(mac_address_t));
    memory_memcopy(src_mac, eth_packet->source, sizeof(mac_address_t));
    eth_packet->type = BYTE_SWAP16(NETWORK_ETHERNET_TYPE_ARP);

    uint8_t* buf = (uint8_t*)eth_packet;

    memory_memcopy(arp_packet, buf + sizeof(network_ethernet_t), sizeof(network_arp_t));

    memory_free(arp_packet);

    return eth_packet;
}
