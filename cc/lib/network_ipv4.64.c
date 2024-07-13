/**
 * @file network_ipv4.64.c
 * @brief Network IPv4 implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_ipv4.h>
#include <network/network_icmpv4.h>
#include <network/network_udpv4.h>
#include <network/network_tcpv4.h>
#include <network/network_info.h>
#include <network/network_ethernet.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>
#include <time.h>
#include <logging.h>
#include <list.h>
#include <map.h>
#include <random.h>

MODULE("turnstone.lib.network");


map_t network_ipv4_packet_fragments = NULL;

typedef struct network_ipv4_fragment_t {
    uint32_t offset;
    uint16_t data_len;
    uint8_t* data;
} network_ipv4_fragment_t;

typedef struct network_ipv4_fragment_item_t {
    uint32_t total_length;
    list_t*  fragments;
} network_ipv4_fragment_item_t;

typedef union network_ipv4_fragment_key_t {
    struct fields_t {
        network_ipv4_address_t ip;
        uint16_t               identification;
        uint8_t                protocol;
        uint8_t                reserved;
    } __attribute__((packed)) fields;
    uint64_t bits;
} network_ipv4_fragment_key_t;

network_ipv4_address_t NETWORK_IPV4_GLOBAL_BROADCAST_IP = { .as_bytes = {255, 255, 255, 255} };
network_ipv4_address_t NETWORK_IPV4_ZERO_IP = { .as_bytes = {0, 0, 0, 0} };

int8_t   network_ipv4_fragment_comparator(const void* f1, const void* f2);
uint16_t network_ipv4_header_checksum(network_ipv4_header_t* ipv4_hdr);
int8_t   network_ipv4_header_checksum_verify(network_ipv4_header_t* ipv4_hdr);
list_t*  network_ipv4_collect_fragments(network_ipv4_header_t* recv_ipv4_packet);
uint8_t* network_ipv4_get_packet_data(network_ipv4_header_t* recv_ipv4_packet);

int8_t network_ipv4_fragment_comparator(const void* f1, const void* f2){
    const network_ipv4_fragment_t* tf1 = f1;
    const network_ipv4_fragment_t* tf2 = f2;

    if(tf1->offset < tf2->offset) {
        return -1;
    }

    if(tf1->offset > tf2->offset) {
        return 1;
    }

    return 0;
}

static inline uint64_t network_ipv4_fragment_key_generator(network_ipv4_address_t ip, uint16_t identification, uint8_t protocol) {
    network_ipv4_fragment_key_t res;

    res.fields.ip = ip;
    res.fields.identification = identification;
    res.fields.protocol = protocol;

    return res.bits;
}

boolean_t network_ipv4_is_address_eq(const network_ipv4_address_t ipv4_addr1, const network_ipv4_address_t ipv4_addr2) {
    return (ipv4_addr1.as_dword == ipv4_addr2.as_dword);
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
list_t* network_ipv4_collect_fragments(network_ipv4_header_t* recv_ipv4_packet) {
    uint64_t key = network_ipv4_fragment_key_generator(recv_ipv4_packet->destination_ip, recv_ipv4_packet->identification, recv_ipv4_packet->protocol);

    network_ipv4_fragment_item_t* frag_item = (network_ipv4_fragment_item_t*)map_get(network_ipv4_packet_fragments, (void*)key);

    if(frag_item == NULL) {
        frag_item = memory_malloc(sizeof(network_ipv4_fragment_item_t));

        if(frag_item == NULL) {
            return NULL;
        }

        frag_item->fragments = list_create_sortedlist(&network_ipv4_fragment_comparator);

        map_insert(network_ipv4_packet_fragments, (void*)key, frag_item);
    }

    uint16_t data_len = BYTE_SWAP16(recv_ipv4_packet->total_length) - (recv_ipv4_packet->header_length * 4);
    uint8_t* data = (uint8_t*)recv_ipv4_packet;
    data += recv_ipv4_packet->header_length * 4;

    frag_item->total_length += data_len;

    network_ipv4_fragment_t* frag = memory_malloc(sizeof(network_ipv4_fragment_t));

    if(frag == NULL) {
        return NULL;
    }

    frag->offset = (uint32_t)recv_ipv4_packet->flags_fragment_offset.fields.fragment_offset << 3;
    frag->data_len = data_len;
    frag->data = memory_malloc(data_len);
    memory_memcopy(data, frag->data, data_len);

    list_sortedlist_insert(frag_item->fragments, frag);

    return NULL;
}

uint8_t* network_ipv4_get_packet_data(network_ipv4_header_t* recv_ipv4_packet) {
    uint8_t* packet_data = NULL;

    if (recv_ipv4_packet->flags_fragment_offset.fields.fragment_offset) {
        uint64_t key = network_ipv4_fragment_key_generator(recv_ipv4_packet->destination_ip, recv_ipv4_packet->identification, recv_ipv4_packet->protocol);

        network_ipv4_fragment_item_t* frag_item = (network_ipv4_fragment_item_t*)map_get(network_ipv4_packet_fragments, (void*)key);

        if(frag_item == NULL) {
            return NULL;
        }

        uint16_t data_len = BYTE_SWAP16(recv_ipv4_packet->total_length) - (recv_ipv4_packet->header_length * 4);
        uint8_t* data = (uint8_t*)recv_ipv4_packet;
        data += recv_ipv4_packet->header_length * 4;

        frag_item->total_length += data_len;

        network_ipv4_fragment_t* frag = memory_malloc(sizeof(network_ipv4_fragment_t));

        if(frag == NULL) {
            return NULL;
        }

        frag->offset = (uint32_t)recv_ipv4_packet->flags_fragment_offset.fields.fragment_offset << 3;
        frag->data_len = data_len;
        frag->data = memory_malloc(data_len);
        memory_memcopy(data, frag->data, data_len);

        list_sortedlist_insert(frag_item->fragments, frag);

        packet_data = memory_malloc(frag_item->total_length);

        iterator_t* iter = list_iterator_create(frag_item->fragments);

        while(iter->end_of_iterator(iter) != 0) {
            frag =  (network_ipv4_fragment_t*)iter->delete_item(iter);

            PRINTLOG(NETWORK, LOG_TRACE, "reassembly offset %i len %i", frag->offset, frag->data_len);

            memory_memcopy(frag->data, packet_data + frag->offset, frag->data_len);

            memory_free(frag->data);
            memory_free(frag);

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        map_delete(network_ipv4_packet_fragments, (void*)key);
        list_destroy_with_data(frag_item->fragments);
        memory_free(frag_item);

    } else {
        uint16_t data_len = BYTE_SWAP16(recv_ipv4_packet->total_length) - (recv_ipv4_packet->header_length * 4);
        uint8_t* data = (uint8_t*)recv_ipv4_packet;
        data += recv_ipv4_packet->header_length * 4;

        packet_data = memory_malloc(data_len);

        if(packet_data == NULL) {
            return NULL;
        }

        memory_memcopy(data, packet_data, data_len);
    }

    return packet_data;
}

list_t* network_ipv4_process_packet(network_ipv4_header_t* recv_ipv4_packet, void* network_info) {
    if(network_ipv4_packet_fragments == NULL) {
        network_ipv4_packet_fragments = map_integer();
    }

    if(network_ipv4_header_checksum_verify(recv_ipv4_packet) != 0) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet checksum failed");

        return NULL;
    }

    const network_info_t* ni = map_get(network_info_map, network_info);

    if(ni && ni->is_ipv4_address_set && (!network_ipv4_is_address_eq(ni->ipv4_address, recv_ipv4_packet->destination_ip) && !network_ipv4_is_address_eq(ni->ipv4_broadcast, recv_ipv4_packet->destination_ip))) {
        PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet destination isnot us");
        return NULL;
    }

    recv_ipv4_packet->flags_fragment_offset.bits = BYTE_SWAP16(recv_ipv4_packet->flags_fragment_offset.bits);

    uint8_t* packet_data = NULL;

    if(recv_ipv4_packet->flags_fragment_offset.fields.flags & NETWORK_IPV4_FLAG_MORE_FRAGMENTS) {
        return network_ipv4_collect_fragments(recv_ipv4_packet);
    } else {
        packet_data = network_ipv4_get_packet_data(recv_ipv4_packet);
    }

    if(recv_ipv4_packet->protocol == NETWORK_IPV4_PROTOCOL_ICMPV4) {
        PRINTLOG(NETWORK, LOG_TRACE, "icmp packet received");

        if(!ni) {
            PRINTLOG(NETWORK, LOG_TRACE, "network info not found for mac address");
            memory_free(packet_data);
            return NULL;
        }

        if(ni && !ni->is_ipv4_address_set) {
            PRINTLOG(NETWORK, LOG_TRACE, "ip address is not set, discarding packet");
            memory_free(packet_data);
            return NULL;
        }

        uint8_t* icmp_data = (uint8_t*)recv_ipv4_packet;
        icmp_data += recv_ipv4_packet->header_length * 4;
        network_icmpv4_header_t* recv_icmp_hdr = (network_icmpv4_header_t*)icmp_data;

        uint16_t pp_len = 0;
        uint16_t ping_data_len = BYTE_SWAP16(recv_ipv4_packet->total_length) - ((recv_ipv4_packet->header_length * 4) + sizeof(network_icmpv4_header_t));

        network_icmpv4_header_t* resp_icmp_hdr = (network_icmpv4_header_t*)network_icmpv4_process_packet(recv_icmp_hdr, &ping_data_len, &pp_len);

        memory_free(packet_data);

        return network_ipv4_create_packet_from_icmp_packet(ni->ipv4_address, recv_ipv4_packet->source_ip, resp_icmp_hdr, pp_len);

    } else if(recv_ipv4_packet->protocol == NETWORK_IPV4_PROTOCOL_UDPV4) {
        network_udpv4_header_t* recv_udpv4_hdr = (network_udpv4_header_t*)packet_data;

        network_udpv4_header_t* resp_udpv4_hdr = (network_udpv4_header_t*)network_udpv4_process_packet(recv_ipv4_packet->destination_ip, recv_ipv4_packet->source_ip, recv_udpv4_hdr, network_info, NULL);

        memory_free(packet_data);

        list_t* ip_pckts = network_ipv4_create_packet_from_udp_packet(recv_ipv4_packet->destination_ip, recv_ipv4_packet->source_ip, resp_udpv4_hdr);

        if(ip_pckts == NULL) {
            PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet response discarded");
            return NULL;
        }

        return ip_pckts;
    } else if(recv_ipv4_packet->protocol == NETWORK_IPV4_PROTOCOL_TCPV4) {
        network_tcpv4_header_t* recv_tcpv4_hdr = (network_tcpv4_header_t*)packet_data;

        uint16_t data_len = BYTE_SWAP16(recv_ipv4_packet->total_length) - (recv_ipv4_packet->header_length * 4);

        uint16_t pp_len = 0;

        network_tcpv4_header_t* resp_tcpv4_hdr = (network_tcpv4_header_t*)network_tcpv4_process_packet(recv_ipv4_packet->destination_ip, recv_ipv4_packet->source_ip, recv_tcpv4_hdr, network_info, data_len, &pp_len);

        memory_free(packet_data);

        list_t* ip_pckts = network_ipv4_create_packet_from_tcp_packet(recv_ipv4_packet->destination_ip, recv_ipv4_packet->source_ip, resp_tcpv4_hdr, pp_len);

        if(ip_pckts == NULL) {
            PRINTLOG(NETWORK, LOG_TRACE, "ipv4 packet response discarded");
            return NULL;
        }

        return ip_pckts;
    } else {
        memory_free(packet_data);
        PRINTLOG(NETWORK, LOG_TRACE, "unimplemented ipv4 protocol 0x%02x", recv_ipv4_packet->protocol);
    }

    return NULL;
}

list_t* network_ipv4_create_packet_from_icmp_packet(const network_ipv4_address_t sip, network_ipv4_address_t dip, network_icmpv4_header_t* icmp_hdr, uint16_t icmp_packet_len) {
    if(icmp_hdr == NULL) { // empty packet
        return NULL;
    }

    uint16_t packet_len = sizeof(network_ipv4_header_t) + icmp_packet_len;

    network_ipv4_header_t* ipv4_packet = memory_malloc(packet_len);

    if(ipv4_packet == NULL) {
        return NULL;
    }

    ipv4_packet->version = NETWORK_IPV4_VERSION;
    ipv4_packet->header_length = 5;
    ipv4_packet->total_length = BYTE_SWAP16(packet_len);
    ipv4_packet->ttl = NETWORK_IPV4_TTL;
    ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_ICMPV4;

    ipv4_packet->source_ip = sip;
    ipv4_packet->destination_ip = dip;

    network_ipv4_header_checksum(ipv4_packet);

    uint8_t* buf = (uint8_t*)ipv4_packet;
    buf += ipv4_packet->header_length * 4;

    memory_memcopy((void*)icmp_hdr, buf, icmp_packet_len);

    memory_free(icmp_hdr);

    network_transmit_packet_t* ipv4_pckt = memory_malloc(sizeof(network_transmit_packet_t));

    if(ipv4_pckt == NULL) {
        memory_free(ipv4_packet);
        return NULL;
    }

    ipv4_pckt->packet_data = (uint8_t*)ipv4_packet;
    ipv4_pckt->packet_len = packet_len;

    list_t* ipv4_packets = list_create_list();

    if(ipv4_packets == NULL) {
        memory_free(ipv4_packet);
        memory_free(ipv4_pckt);
        return NULL;
    }

    list_queue_push(ipv4_packets, ipv4_pckt);

    return ipv4_packets;
}

list_t* network_ipv4_create_packet_from_udp_packet(const network_ipv4_address_t sip, network_ipv4_address_t dip, network_udpv4_header_t* udp_hdr) {
    if(udp_hdr == NULL) {
        return NULL;
    }

    list_t* fragments = list_create_list();

    if(fragments == NULL) {
        memory_free(udp_hdr);

        return NULL;
    }

    uint8_t* raw_udp_hdr = (uint8_t*)udp_hdr;

    uint16_t packet_len = BYTE_SWAP16(udp_hdr->length);

    uint16_t max_packet_len = 1500 - sizeof(network_ipv4_header_t);

    if(max_packet_len % 8) {
        max_packet_len -= max_packet_len % 8;
    }

    uint16_t identification = 0;

    if(packet_len > max_packet_len) {
        identification = rand();
    }

    uint16_t offset = 0;

    while(packet_len > max_packet_len) {
        packet_len -= max_packet_len;

        network_ipv4_header_t* ipv4_packet = memory_malloc(max_packet_len + sizeof(network_ipv4_header_t));

        if(ipv4_packet == NULL) {
            return NULL;
        }

        ipv4_packet->version = NETWORK_IPV4_VERSION;
        ipv4_packet->header_length = 5;
        ipv4_packet->total_length = BYTE_SWAP16(max_packet_len + sizeof(network_ipv4_header_t));
        ipv4_packet->ttl = NETWORK_IPV4_TTL;
        ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_UDPV4;
        ipv4_packet->identification = BYTE_SWAP16(identification);
        ipv4_packet->flags_fragment_offset.fields.fragment_offset = offset >> 3;
        ipv4_packet->flags_fragment_offset.fields.flags = NETWORK_IPV4_FLAG_MORE_FRAGMENTS;
        ipv4_packet->flags_fragment_offset.bits = BYTE_SWAP16(ipv4_packet->flags_fragment_offset.bits);

        ipv4_packet->source_ip = sip;
        ipv4_packet->destination_ip = dip;

        network_ipv4_header_checksum(ipv4_packet);

        uint32_t hcsum = udp_hdr->checksum + sip.as_words[0] + sip.as_words[1] + dip.as_words[0] + dip.as_words[1];

        while(hcsum >> 16) {
            uint32_t carry = hcsum >> 16;
            hcsum &= 0xFFFF;
            hcsum += carry;
        }

        int16_t csum = hcsum;

        udp_hdr->checksum = ~csum;

        uint8_t* buf = (uint8_t*)ipv4_packet;
        buf += ipv4_packet->header_length * 4;

        memory_memcopy(raw_udp_hdr, buf, max_packet_len);

        raw_udp_hdr += max_packet_len;
        offset += max_packet_len;

        network_transmit_packet_t* ipv4_pckt = memory_malloc(sizeof(network_transmit_packet_t));

        if(ipv4_pckt == NULL) {
            memory_free(ipv4_packet);
            list_destroy_with_type(fragments, LIST_DESTROY_WITH_DATA, network_transmit_packet_destroyer);
            return NULL;
        }

        ipv4_pckt->packet_data = (uint8_t*)ipv4_packet;
        ipv4_pckt->packet_len = max_packet_len + sizeof(network_ipv4_header_t);

        list_queue_push(fragments, ipv4_pckt);
    }


    network_ipv4_header_t* ipv4_packet = memory_malloc(packet_len + sizeof(network_ipv4_header_t));

    if(ipv4_packet == NULL) {
        return NULL;
    }

    ipv4_packet->version = NETWORK_IPV4_VERSION;
    ipv4_packet->header_length = 5;
    ipv4_packet->total_length = BYTE_SWAP16(packet_len + sizeof(network_ipv4_header_t));
    ipv4_packet->ttl = NETWORK_IPV4_TTL;
    ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_UDPV4;
    ipv4_packet->identification = BYTE_SWAP16(identification);
    ipv4_packet->flags_fragment_offset.fields.fragment_offset = offset >> 3;
    ipv4_packet->flags_fragment_offset.bits = BYTE_SWAP16(ipv4_packet->flags_fragment_offset.bits);

    ipv4_packet->source_ip = sip;
    ipv4_packet->destination_ip = dip;

    network_ipv4_header_checksum(ipv4_packet);


    uint32_t hcsum = udp_hdr->checksum + sip.as_words[0] + sip.as_words[1] + dip.as_words[0] + dip.as_words[1];

    while(hcsum >> 16) {
        uint32_t carry = hcsum >> 16;
        hcsum &= 0xFFFF;
        hcsum += carry;
    }

    int16_t csum = hcsum;

    udp_hdr->checksum = ~csum;

    uint8_t* buf = (uint8_t*)ipv4_packet;
    buf += ipv4_packet->header_length * 4;

    memory_memcopy(raw_udp_hdr, buf, packet_len);

    memory_free(udp_hdr);

    network_transmit_packet_t* ipv4_pckt = memory_malloc(sizeof(network_transmit_packet_t));

    if(ipv4_pckt == NULL) {
        memory_free(ipv4_packet);
        list_destroy_with_type(fragments, LIST_DESTROY_WITH_DATA, network_transmit_packet_destroyer);
        return NULL;
    }

    ipv4_pckt->packet_data = (uint8_t*)ipv4_packet;
    ipv4_pckt->packet_len = packet_len + sizeof(network_ipv4_header_t);

    list_queue_push(fragments, ipv4_pckt);

    return fragments;
}

list_t* network_ipv4_create_packet_from_tcp_packet(const network_ipv4_address_t sip, network_ipv4_address_t dip, network_tcpv4_header_t* tcp_hdr, uint16_t packet_len) {
    if(tcp_hdr == NULL) {
        return NULL;
    }

    list_t* fragments = list_create_list();

    if(fragments == NULL) {
        memory_free(tcp_hdr);

        return NULL;
    }

    uint8_t* raw_tcp_hdr = (uint8_t*)tcp_hdr;

    uint16_t max_packet_len = 1500 - sizeof(network_ipv4_header_t);

    if(max_packet_len % 8) {
        max_packet_len -= max_packet_len % 8;
    }

    uint16_t identification = 0;

    if(packet_len > max_packet_len) {
        identification = rand();
    }

    uint16_t offset = 0;

    while(packet_len > max_packet_len) {
        packet_len -= max_packet_len;

        network_ipv4_header_t* ipv4_packet = memory_malloc(max_packet_len + sizeof(network_ipv4_header_t));

        if(ipv4_packet == NULL) {
            return NULL;
        }

        ipv4_packet->version = NETWORK_IPV4_VERSION;
        ipv4_packet->header_length = 5;
        ipv4_packet->total_length = BYTE_SWAP16(max_packet_len + sizeof(network_ipv4_header_t));
        ipv4_packet->ttl = NETWORK_IPV4_TTL;
        ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_TCPV4;
        ipv4_packet->identification = BYTE_SWAP16(identification);
        ipv4_packet->flags_fragment_offset.fields.fragment_offset = offset >> 3;
        ipv4_packet->flags_fragment_offset.fields.flags = NETWORK_IPV4_FLAG_MORE_FRAGMENTS;
        ipv4_packet->flags_fragment_offset.bits = BYTE_SWAP16(ipv4_packet->flags_fragment_offset.bits);

        ipv4_packet->source_ip = sip;
        ipv4_packet->destination_ip = dip;

        network_ipv4_header_checksum(ipv4_packet);

        uint32_t hcsum = tcp_hdr->checksum + sip.as_words[0] + sip.as_words[1] + dip.as_words[0] + dip.as_words[1];

        while(hcsum >> 16) {
            uint32_t carry = hcsum >> 16;
            hcsum &= 0xFFFF;
            hcsum += carry;
        }

        int16_t csum = hcsum;

        tcp_hdr->checksum = ~csum;

        uint8_t* buf = (uint8_t*)ipv4_packet;
        buf += ipv4_packet->header_length * 4;

        memory_memcopy(raw_tcp_hdr, buf, max_packet_len);

        raw_tcp_hdr += max_packet_len;
        offset += max_packet_len;

        network_transmit_packet_t* ipv4_pckt = memory_malloc(sizeof(network_transmit_packet_t));

        if(ipv4_pckt == NULL) {
            memory_free(ipv4_packet);
            list_destroy_with_type(fragments, LIST_DESTROY_WITH_DATA, network_transmit_packet_destroyer);
            return NULL;
        }

        ipv4_pckt->packet_data = (uint8_t*)ipv4_packet;
        ipv4_pckt->packet_len = max_packet_len + sizeof(network_ipv4_header_t);

        list_queue_push(fragments, ipv4_pckt);
    }


    network_ipv4_header_t* ipv4_packet = memory_malloc(packet_len + sizeof(network_ipv4_header_t));

    if(ipv4_packet == NULL) {
        return NULL;
    }

    ipv4_packet->version = NETWORK_IPV4_VERSION;
    ipv4_packet->header_length = 5;
    ipv4_packet->total_length = BYTE_SWAP16(packet_len + sizeof(network_ipv4_header_t));
    ipv4_packet->ttl = NETWORK_IPV4_TTL;
    ipv4_packet->protocol = NETWORK_IPV4_PROTOCOL_TCPV4;
    ipv4_packet->identification = BYTE_SWAP16(identification);
    ipv4_packet->flags_fragment_offset.fields.fragment_offset = offset >> 3;
    ipv4_packet->flags_fragment_offset.bits = BYTE_SWAP16(ipv4_packet->flags_fragment_offset.bits);

    ipv4_packet->source_ip = sip;
    ipv4_packet->destination_ip = dip;

    network_ipv4_header_checksum(ipv4_packet);

    uint32_t hcsum = tcp_hdr->checksum + sip.as_words[0] + sip.as_words[1] + dip.as_words[0] + dip.as_words[1];

    while(hcsum >> 16) {
        uint32_t carry = hcsum >> 16;
        hcsum &= 0xFFFF;
        hcsum += carry;
    }

    int16_t csum = hcsum;

    tcp_hdr->checksum = ~csum;

    uint8_t* buf = (uint8_t*)ipv4_packet;
    buf += ipv4_packet->header_length * 4;

    memory_memcopy(raw_tcp_hdr, buf, packet_len);

    memory_free(tcp_hdr);

    network_transmit_packet_t* ipv4_pckt = memory_malloc(sizeof(network_transmit_packet_t));

    if(ipv4_pckt == NULL) {
        memory_free(ipv4_packet);
        list_destroy_with_type(fragments, LIST_DESTROY_WITH_DATA, network_transmit_packet_destroyer);
        return NULL;
    }

    ipv4_pckt->packet_data = (uint8_t*)ipv4_packet;
    ipv4_pckt->packet_len = packet_len + sizeof(network_ipv4_header_t);

    list_queue_push(fragments, ipv4_pckt);

    return fragments;
}
#pragma GCC diagnostic pop
