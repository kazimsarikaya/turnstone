/**
 * @file network_packet.64.c
 * @brief Network packet implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_packet.h>
#include <network/network_filter.h>
#include <network/network_utils.h>
#include <network/network_connection.h>
#include <utils.h>
#include <logging.h>
#include <time.h>


MODULE("turnstone.lib.network");

typedef struct network_ethernet_t {
    network_mac_address_t destination;
    network_mac_address_t source;
    network_protocol_t    type;
}__attribute__((packed)) network_ethernet_t;

_Static_assert(sizeof(network_ethernet_t) == 14, "invalid network_ethernet_t size");

typedef struct network_ethernet_with_vlan_t {
    network_mac_address_t destination;
    network_mac_address_t source;
    network_protocol_t    type;
    uint16_t              vlan_tag;
    network_protocol_t    inner_type;
}__attribute__((packed)) network_ethernet_with_vlan_t;

_Static_assert(sizeof(network_ethernet_with_vlan_t) == 18, "invalid network_ethernet_with_vlan_t size");


typedef struct network_arp_t {
    uint16_t               hardware_type;
    uint16_t               protocol_type;
    uint8_t                hardware_address_length;
    uint8_t                protocol_address_length;
    uint16_t               operation_code;
    network_mac_address_t  source_mac;
    network_ipv4_address_t source_ip;
    network_mac_address_t  target_mac;
    network_ipv4_address_t target_ip;
}__attribute__((packed)) network_arp_t;

_Static_assert(sizeof(network_arp_t) == 28, "invalid network_arp_t size");

#define NETWORK_IPV4_VERSION 4
#define NETWORK_IPV4_TTL 128

typedef enum network_ipv4_flag_t : uint16_t {
    NETWORK_IPV4_FLAG_RESERVED       = 0x8000,
    NETWORK_IPV4_FLAG_DONT_FRAGMENT  = 0x4000,
    NETWORK_IPV4_FLAG_MORE_FRAGMENTS = 0x2000,
} network_ipv4_flag_t;
#define NETWORK_IPV4_FLAG_DONT_FRAGMENT  2
#define NETWORK_IPV4_FLAG_MORE_FRAGMENTS 1

typedef struct network_ipv4_header_t {
    uint32_t                header_length         : 4;
    uint32_t                version               : 4;
    uint32_t                ecn                   : 2;
    uint32_t                dscp                  : 6;
    uint32_t                total_length          : 16;
    uint32_t                identification        : 16;
    uint16_t                flags_fragment_offset : 16;
    uint32_t                ttl                   : 8;
    network_ipv4_protocol_t protocol              : 8;
    uint32_t                header_checksum       : 16;
    network_ipv4_address_t  source_ip;
    network_ipv4_address_t  destination_ip;
}__attribute__((packed)) network_ipv4_header_t;

_Static_assert(sizeof(network_ipv4_header_t) == NETWORK_IPV4_HEADER_LENGTH, "invalid network_ipv4_header_t size");

typedef struct network_icmpv4_header_t {
    network_icmpv4_type_t type;
    uint8_t               code;
    uint16_t              checksum;
    uint16_t              identifier;
    uint16_t              sequence;
}__attribute__((packed)) network_icmpv4_header_t;

_Static_assert(sizeof(network_icmpv4_header_t) == 8, "invalid network_icmpv4_header_t size");

typedef struct network_icmpv4_ping_header_t {
    uint32_t timestamp_sec;
    uint32_t timestamp_usec;
}__attribute__((packed)) network_icmpv4_ping_header_t;

_Static_assert(sizeof(network_icmpv4_ping_header_t) == 8, "invalid network_icmpv4_ping_header_t size");

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
    uint8_t  options[];
}__attribute__((packed)) network_tcpv4_header_t;

_Static_assert(sizeof(network_tcpv4_header_t) == NETWORK_TCPV4_HEADER_LENGTH, "invalid network_tcpv4_header_t size");

typedef enum network_tcpv4_option_kind_t : uint8_t {
    NETWORK_TCPV4_OPTION_KIND_END_OF_OPTION_LIST = 0,
    NETWORK_TCPV4_OPTION_KIND_NO_OPERATION       = 1,
    NETWORK_TCPV4_OPTION_KIND_MAX_SEGMENT_SIZE   = 2,
    NETWORK_TCPV4_OPTION_KIND_WINDOW_SCALE       = 3,
    NETWORK_TCPV4_OPTION_KIND_SACK_PERMITTED     = 4,
    NETWORK_TCPV4_OPTION_KIND_SACK               = 5,
    NETWORK_TCPV4_OPTION_KIND_TIMESTAMP          = 8,
} network_tcpv4_option_kind_t;

typedef struct network_tcpv4_option_t {
    network_tcpv4_option_kind_t kind;
    uint8_t                     length;
    uint8_t                     data[];
}__attribute__((packed)) network_tcpv4_option_t;

typedef struct network_udpv4_header_t {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
}__attribute__((packed)) network_udpv4_header_t;

static int8_t network_packet_parse_ethernet_header(network_packet_t* packet) {
    if(packet->remaining_data_len < NETWORK_ETHERNET_MIN_FRAME_SIZE) {
        PRINTLOG(NETWORK, LOG_ERROR, "not enough data for Ethernet header");
        return -1;
    }

    packet->ether_header_offset = packet->current_offset;

    network_ethernet_t* eth_hdr = (network_ethernet_t*)packet->ether_header_offset;

    memory_memcopy(eth_hdr->source, packet->ether_source_mac, sizeof(network_mac_address_t));
    memory_memcopy(eth_hdr->destination, packet->ether_destination_mac, sizeof(network_mac_address_t));
    packet->ether_ethertype = BYTE_SWAP16(eth_hdr->type);

    if(packet->ether_ethertype == NETWORK_PROTOCOL_VLAN) {
        network_ethernet_with_vlan_t* vlan_eth_hdr = (network_ethernet_with_vlan_t*)packet->raw_data;

        packet->is_vlan_tagged        = true;
        packet->ether_vlan_id         = BYTE_SWAP16(vlan_eth_hdr->vlan_tag) & 0x0FFF;
        packet->ether_inner_ethertype = BYTE_SWAP16(vlan_eth_hdr->inner_type);
        packet->current_offset        = packet->raw_data + sizeof(network_ethernet_with_vlan_t);
        packet->remaining_data_len   -= sizeof(network_ethernet_with_vlan_t);
    } else {
        packet->is_vlan_tagged      = false;
        packet->current_offset      = packet->raw_data + sizeof(network_ethernet_t);
        packet->remaining_data_len -= sizeof(network_ethernet_t);
    }

    return 0;
}

static int8_t network_packet_parse_arp_header(network_packet_t* packet) {
    if(packet->remaining_data_len < sizeof(network_arp_t)) {
        PRINTLOG(NETWORK, LOG_ERROR, "not enough data for ARP header");
        return -1;
    }

    packet->arp_header_offset = packet->current_offset;

    network_arp_t* arp_hdr = (network_arp_t*)packet->arp_header_offset;

    packet->arp_hardware_type           = BYTE_SWAP16(arp_hdr->hardware_type);
    packet->arp_protocol_type           = BYTE_SWAP16(arp_hdr->protocol_type);
    packet->arp_hardware_address_length = arp_hdr->hardware_address_length;
    packet->arp_protocol_address_length = arp_hdr->protocol_address_length;
    packet->arp_operation_code          = BYTE_SWAP16(arp_hdr->operation_code);
    memory_memcopy(arp_hdr->source_mac, packet->arp_source_mac, sizeof(network_mac_address_t));
    packet->arp_source_ip = arp_hdr->source_ip;
    memory_memcopy(arp_hdr->target_mac, packet->arp_target_mac, sizeof(network_mac_address_t));
    packet->arp_target_ip = arp_hdr->target_ip;

    packet->remaining_data_len -= sizeof(network_arp_t);
    packet->current_offset      = packet->current_offset + sizeof(network_arp_t);
    packet->payload_offset      = packet->current_offset;
    packet->payload_len         = packet->remaining_data_len;


    return 0;
}

static int8_t network_packet_parse_icmpv4_ping_header(network_packet_t* packet) {
    if(packet->raw_data_len < (packet->current_offset - packet->raw_data) + sizeof(network_icmpv4_ping_header_t)) {
        PRINTLOG(NETWORK, LOG_ERROR, "not enough data for ICMPv4 ping header");
        return -1;
    }

    packet->icmpv4_ping_header_offset = packet->current_offset;

    network_icmpv4_ping_header_t* ping_hdr = (network_icmpv4_ping_header_t*)packet->icmpv4_ping_header_offset;

    // convert from big endian to little endian
    packet->icmpv4_ping_timestamp_sec  = BYTE_SWAP32(ping_hdr->timestamp_sec);
    packet->icmpv4_ping_timestamp_usec = BYTE_SWAP32(ping_hdr->timestamp_usec);

    // packet->current_offset = packet->current_offset + sizeof(network_icmpv4_ping_header_t);
    // packet->payload_offset = packet->current_offset;
    // packet->payload_len    = packet->raw_data_len - (packet->current_offset - packet->raw_data);

    return 0;
}

static int8_t network_packet_parse_icmpv4_header(network_packet_t* packet) {
    if(packet->remaining_data_len < sizeof(network_icmpv4_header_t)) {
        PRINTLOG(NETWORK, LOG_ERROR, "not enough data for ICMPv4 header");
        return -1;
    }

    packet->icmpv4_header_offset = packet->current_offset;

    network_icmpv4_header_t* icmp_hdr = (network_icmpv4_header_t*)packet->icmpv4_header_offset;

    packet->icmpv4_type       = icmp_hdr->type;
    packet->icmpv4_code       = icmp_hdr->code;
    packet->icmpv4_checksum   = BYTE_SWAP16(icmp_hdr->checksum);
    packet->icmpv4_identifier = BYTE_SWAP16(icmp_hdr->identifier);
    packet->icmpv4_sequence   = BYTE_SWAP16(icmp_hdr->sequence);

    if(packet->icmpv4_type == NETWORK_ICMPV4_TYPE_ECHO_REQUEST || packet->icmpv4_type == NETWORK_ICMPV4_TYPE_ECHO_REPLY) {
        packet->is_icmpv4_ping_packet = true;
    }

    packet->remaining_data_len -= sizeof(network_icmpv4_header_t);
    packet->current_offset      = packet->current_offset + sizeof(network_icmpv4_header_t);
    packet->payload_offset      = packet->current_offset;
    packet->payload_len         = packet->ipv4_total_length - packet->ipv4_ihl * 4 - sizeof(network_icmpv4_header_t);

    if(packet->is_icmpv4_ping_packet) {
        return network_packet_parse_icmpv4_ping_header(packet);
    }

    return 0;
}

static int8_t network_packet_parse_tcpv4_header(network_packet_t* packet) {
    if(packet->raw_data_len < (packet->current_offset - packet->raw_data) + sizeof(network_tcpv4_header_t)) {
        PRINTLOG(NETWORK, LOG_ERROR, "not enough data for TCPv4 header");
        return -1;
    }

    packet->tcpv4_header_offset = packet->current_offset;

    network_tcpv4_header_t* tcp_hdr = (network_tcpv4_header_t*)packet->tcpv4_header_offset;

    packet->tcpv4_source_port            = BYTE_SWAP16(tcp_hdr->source_port);
    packet->tcpv4_destination_port       = BYTE_SWAP16(tcp_hdr->destination_port);
    packet->tcpv4_sequence_number        = BYTE_SWAP32(tcp_hdr->sequence_number);
    packet->tcpv4_acknowledgement_number = BYTE_SWAP32(tcp_hdr->acknowledgement_number);
    packet->tcpv4_header_length          = tcp_hdr->header_length;
    packet->tcpv4_ns                     = tcp_hdr->ns;
    packet->tcpv4_cwr                    = tcp_hdr->cwr;
    packet->tcpv4_ece                    = tcp_hdr->ece;
    packet->tcpv4_urg                    = tcp_hdr->urg;
    packet->tcpv4_ack                    = tcp_hdr->ack;
    packet->tcpv4_psh                    = tcp_hdr->psh;
    packet->tcpv4_rst                    = tcp_hdr->rst;
    packet->tcpv4_syn                    = tcp_hdr->syn;
    packet->tcpv4_fin                    = tcp_hdr->fin;
    packet->tcpv4_window_size            = BYTE_SWAP16(tcp_hdr->window_size);
    packet->tcpv4_checksum               = BYTE_SWAP16(tcp_hdr->checksum);
    packet->tcpv4_urgent_pointer         = BYTE_SWAP16(tcp_hdr->urgent_pointer);

    if(packet->tcpv4_header_length > 5) {
        uint8_t* options_ptr = (uint8_t*)tcp_hdr + sizeof(network_tcpv4_header_t);
        size_t options_len   = (packet->tcpv4_header_length - 5) * 4;

        size_t parsed_options_len = 0;

        while(parsed_options_len < options_len) {
            network_tcpv4_option_t* option = (network_tcpv4_option_t*)(options_ptr + parsed_options_len);

            switch(option->kind) {
            case NETWORK_TCPV4_OPTION_KIND_END_OF_OPTION_LIST:
                parsed_options_len = options_len; // end of options
                break;
            case NETWORK_TCPV4_OPTION_KIND_NO_OPERATION:
                parsed_options_len += 1; // no operation, move to next byte
                break;
            case NETWORK_TCPV4_OPTION_KIND_MAX_SEGMENT_SIZE:
                if(option->length == 4) {
                    packet->tcpv4_mss = BYTE_SWAP16(*(uint16_t*)(void*)option->data);
                }
                parsed_options_len += option->length;
                break;
            case NETWORK_TCPV4_OPTION_KIND_WINDOW_SCALE:
                if(option->length == 3) {
                    packet->tcpv4_window_scale = option->data[0];
                }
                parsed_options_len += option->length;
                break;
            case NETWORK_TCPV4_OPTION_KIND_SACK_PERMITTED:
                packet->tcpv4_sack_permitted = true;
                parsed_options_len          += option->length;
                break;
            case NETWORK_TCPV4_OPTION_KIND_SACK:
                if(option->length >= 2 && (option->length - 2) % 8 == 0) {
                    packet->tcpv4_sack_blocks_count = (option->length - 2) / 8;

                    for(size_t i = 0; i < packet->tcpv4_sack_blocks_count && i < 4; i++) {
                        packet->tcpv4_sack_blocks[i].left_edge  = BYTE_SWAP32(*(uint32_t*)(void*)(option->data + i * 8));
                        packet->tcpv4_sack_blocks[i].right_edge = BYTE_SWAP32(*(uint32_t*)(void*)(option->data + i * 8 + 4));
                    }
                }
                parsed_options_len += option->length;
                break;
            case NETWORK_TCPV4_OPTION_KIND_TIMESTAMP:
                if(option->length == 10) {
                    packet->tcpv4_timestamp            = BYTE_SWAP32(*(uint32_t*)(void*)option->data);
                    packet->tcpv4_timestamp_echo_reply = BYTE_SWAP32(*(uint32_t*)(void*)(option->data + 4));
                }
                parsed_options_len += option->length;
                break;
            default:
                // unknown option, skip it
                parsed_options_len += option->length;
                break;
            }
        }
    }

    packet->remaining_data_len -= packet->tcpv4_header_length * 4;
    packet->current_offset      = packet->current_offset + (packet->tcpv4_header_length * 4);
    packet->payload_offset      = packet->current_offset;

    // we cannot trust nic's packet length. we should use
    // iphdr total length minus iphdr header length minus tcp header length as the payload length.
    // otherwise, if there are extra bytes after the tcp payload, we will treat them as part of the payload
    // and cause problems for tcp payload parsing.
    packet->payload_len = packet->ipv4_total_length - (packet->ipv4_ihl * 4) - (packet->tcpv4_header_length * 4);

    return 0;
}

static int8_t network_packet_parse_udpv4_header(network_packet_t* packet) {
    if(packet->remaining_data_len < sizeof(network_udpv4_header_t)) {
        PRINTLOG(NETWORK, LOG_ERROR, "not enough data for UDPv4 header");
        return -1;
    }

    packet->udpv4_header_offset = packet->current_offset;

    network_udpv4_header_t* udp_hdr = (network_udpv4_header_t*)packet->udpv4_header_offset;

    packet->udpv4_source_port      = BYTE_SWAP16(udp_hdr->source_port);
    packet->udpv4_destination_port = BYTE_SWAP16(udp_hdr->destination_port);
    packet->udpv4_length           = BYTE_SWAP16(udp_hdr->length);
    packet->udpv4_checksum         = BYTE_SWAP16(udp_hdr->checksum);

    packet->remaining_data_len -= sizeof(network_udpv4_header_t);
    packet->current_offset      = packet->current_offset + sizeof(network_udpv4_header_t);
    packet->payload_offset      = packet->current_offset;
    // similar to tcp, we should use iphdr total length minus iphdr header length minus udp header length as the payload length,
    // rather than using udp header's length field, because some nics may set udp header's length field to 0 or incorrect value.
    packet->payload_len = packet->ipv4_total_length - (packet->ipv4_ihl * 4) - sizeof(network_udpv4_header_t);

    return 0;
}

static int8_t network_packet_parse_ipv4_header(network_packet_t* packet) {
    if(packet->remaining_data_len < sizeof(network_ipv4_header_t)) {
        PRINTLOG(NETWORK, LOG_ERROR, "not enough data for IPv4 header");
        return -1;
    }

    packet->ipv4_header_offset = packet->current_offset;

    network_ipv4_header_t* ipv4_hdr = (network_ipv4_header_t*)packet->current_offset;

    packet->ipv4_version         = ipv4_hdr->version;
    packet->ipv4_ihl             = ipv4_hdr->header_length;
    packet->ipv4_dscp            = ipv4_hdr->dscp;
    packet->ipv4_ecn             = ipv4_hdr->ecn;
    packet->ipv4_total_length    = BYTE_SWAP16(ipv4_hdr->total_length);
    packet->ipv4_identification  = BYTE_SWAP16(ipv4_hdr->identification);
    packet->ipv4_flags           = (BYTE_SWAP16(ipv4_hdr->flags_fragment_offset) >> 13) & 0x7;
    packet->ipv4_fragment_offset = BYTE_SWAP16(ipv4_hdr->flags_fragment_offset) & 0x1FFF;
    packet->ipv4_ttl             = ipv4_hdr->ttl;
    packet->ipv4_protocol        = ipv4_hdr->protocol;
    packet->ipv4_header_checksum = BYTE_SWAP16(ipv4_hdr->header_checksum);
    packet->ipv4_source_ip       = ipv4_hdr->source_ip;
    packet->ipv4_destination_ip  = ipv4_hdr->destination_ip;

    packet->remaining_data_len -= packet->ipv4_ihl * 4;
    packet->current_offset      = packet->current_offset + (packet->ipv4_ihl * 4);

    switch(packet->ipv4_protocol) {
    case NETWORK_IPV4_PROTOCOL_ICMPV4:
        packet->is_icmpv4_packet = true;
        return network_packet_parse_icmpv4_header(packet);
    case NETWORK_IPV4_PROTOCOL_TCPV4:
        packet->is_tcpv4_packet = true;
        return network_packet_parse_tcpv4_header(packet);
    case NETWORK_IPV4_PROTOCOL_UDPV4:
        packet->is_udpv4_packet = true;
        return network_packet_parse_udpv4_header(packet);
    default:
        break;
    }

    PRINTLOG(NETWORK, LOG_WARNING, "unsupported ipv4 protocol 0x%02x", packet->ipv4_protocol);

    return -1;
}

static int8_t network_packet_parse(network_packet_t* packet) {
    if(network_packet_parse_ethernet_header(packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to parse ethernet header");
        return -1;
    }

    network_protocol_t ethertype = network_packet_get_ethernet_ethertype(packet);

    switch(ethertype) {
    case NETWORK_PROTOCOL_ARP:
        packet->is_arp_packet     = true;
        packet->arp_header_offset = packet->current_offset;
        return network_packet_parse_arp_header(packet);
    case NETWORK_PROTOCOL_IPV4:
        packet->is_ipv4_packet     = true;
        packet->ipv4_header_offset = packet->current_offset;
        return network_packet_parse_ipv4_header(packet);
    default:
        break;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "unsupported ethertype 0x%04x", ethertype);

    return -1;
}

static uint16_t network_checksum_calculate(uint8_t* data, uint16_t len) {
    uint32_t checksum = 0;
    uint16_t* data_16 = (uint16_t*)(void*)data;

    for(size_t i = 0; i < len / 2; i++) {
        checksum += data_16[i];
    }

    if(len & 1) {
        checksum += data[len - 1];
    }

    while(checksum >> 16) {
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }

    return ~checksum;
}

static uint16_t network_checksum_calculate_tcp_udp_pseudo_header(network_ipv4_address_t source_ip,
                                                                 network_ipv4_address_t destination_ip,
                                                                 uint8_t                protocol,
                                                                 uint16_t               tcp_udp_length) {
    uint32_t checksum = 0;

    // Add IPs as 16-bit words (high and low)
    checksum += (source_ip.as_dword & 0xFFFF);
    checksum += (source_ip.as_dword >> 16);
    checksum += (destination_ip.as_dword & 0xFFFF);
    checksum += (destination_ip.as_dword >> 16);

    // Protocol is the lower 8 bits of a 16-bit word (00 | Protocol)
    // Length is a standard 16-bit word.
    // We use BYTE_SWAP16 only if your input variables are in Host Byte Order.
    checksum += BYTE_SWAP16((uint16_t)protocol);
    checksum += BYTE_SWAP16(tcp_udp_length);

    while(checksum >> 16) {
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }

    return (uint16_t)checksum;
}

static uint16_t network_checksum_calculate_tcp_udp(uint16_t pseudo_header_checksum, void* tcp_udp_header, uint16_t tcp_udp_length) {
    uint32_t checksum = pseudo_header_checksum;
    uint16_t* data_16 = (uint16_t*)tcp_udp_header;

    // 1. Remove BYTE_SWAP16 here. Sum the raw 16-bit words.
    for(size_t i = 0; i < tcp_udp_length / 2; i++) {
        checksum += data_16[i];
    }

    // 2. Handle the odd byte correctly
    if(tcp_udp_length & 1) {
        // The odd byte is the high byte of a 16-bit word padded with 00
        checksum += ((uint8_t*)tcp_udp_header)[tcp_udp_length - 1];
    }

    // 3. Fold the 32-bit sum into 16 bits
    while(checksum >> 16) {
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }

    // 4. Return the bitwise NOT
    return ~((uint16_t)checksum);
}

static int8_t network_packet_verify_checksum(network_packet_t* packet) {
    if(packet->is_ipv4_packet) {
        network_ipv4_header_t* ipv4_hdr = (network_ipv4_header_t*)packet->ipv4_header_offset;

        uint16_t received_checksum = ipv4_hdr->header_checksum;

        ipv4_hdr->header_checksum = 0;

        uint16_t calculated_checksum = network_checksum_calculate((uint8_t*)ipv4_hdr, packet->ipv4_ihl * 4);

        if(received_checksum != calculated_checksum) {
            PRINTLOG(NETWORK, LOG_ERROR, "invalid ipv4 header checksum: received 0x%04x, calculated 0x%04x", received_checksum, calculated_checksum);
            return -1;
        }

        uint16_t chksum_data_len = packet->ipv4_total_length - (packet->ipv4_ihl * 4);

        switch(packet->ipv4_protocol) {
        case NETWORK_IPV4_PROTOCOL_ICMPV4: {
            network_icmpv4_header_t* icmp_hdr = (network_icmpv4_header_t*)packet->icmpv4_header_offset;

            received_checksum = icmp_hdr->checksum;

            icmp_hdr->checksum = 0;

            calculated_checksum = network_checksum_calculate(packet->icmpv4_header_offset, chksum_data_len);

            if(received_checksum != calculated_checksum) {
                PRINTLOG(NETWORK, LOG_ERROR, "invalid icmpv4 header checksum: received 0x%04x, calculated 0x%04x", received_checksum, calculated_checksum);
                return -1;
            }
        } break;
        case NETWORK_IPV4_PROTOCOL_TCPV4: {
            network_tcpv4_header_t* tcp_hdr = (network_tcpv4_header_t*)packet->tcpv4_header_offset;

            received_checksum = tcp_hdr->checksum;

            tcp_hdr->checksum = 0;

            uint16_t pseudo_header_checksum = network_checksum_calculate_tcp_udp_pseudo_header(packet->ipv4_source_ip, packet->ipv4_destination_ip, packet->ipv4_protocol, chksum_data_len);
            calculated_checksum = network_checksum_calculate_tcp_udp(pseudo_header_checksum, tcp_hdr, chksum_data_len);

            if(received_checksum != calculated_checksum) {
                PRINTLOG(NETWORK, LOG_ERROR, "invalid tcpv4 header checksum: received 0x%04x, calculated 0x%04x", received_checksum, calculated_checksum);
                return -1;
            }
        } break;
        case NETWORK_IPV4_PROTOCOL_UDPV4: {
            network_udpv4_header_t* udp_hdr = (network_udpv4_header_t*)packet->udpv4_header_offset;

            if(udp_hdr->checksum == 0) {
                // checksum is optional for UDP
                break;
            }

            received_checksum = udp_hdr->checksum;

            udp_hdr->checksum = 0;

            uint16_t pseudo_header_checksum = network_checksum_calculate_tcp_udp_pseudo_header(packet->ipv4_source_ip, packet->ipv4_destination_ip, packet->ipv4_protocol, chksum_data_len);
            calculated_checksum = network_checksum_calculate_tcp_udp(pseudo_header_checksum, udp_hdr, chksum_data_len);

            if(received_checksum != calculated_checksum) {
                PRINTLOG(NETWORK, LOG_ERROR, "invalid udpv4 header checksum: received 0x%04x, calculated 0x%04x", received_checksum, calculated_checksum);
                return -1;
            }
        } break;
        default:
            break;
        }
    }

    return 0;
}

int8_t network_packet_craft(const network_info_t* ni,
                            network_packet_t*     packet,
                            uint8_t*              buffer,
                            uint16_t              buffer_len) {
    if(!ni || !packet) {
        return -1;
    }

    memory_heap_t* heap = list_get_heap(ni->return_queue);

    if(!heap) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to get heap from return queue");
        return -1;
    }

    packet->raw_data = memory_malloc_ext(heap, ni->mtu + 22, 0);

    if(!packet->raw_data) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to allocate memory for packet raw data");
        return -1;
    }

    packet->ether_header_offset = packet->raw_data;

    packet->is_vlan_tagged = ni->is_vlan_tagged && !ni->has_hw_vlan_support;

    network_ethernet_t* eth_hdr = (network_ethernet_t*)packet->ether_header_offset;

    memory_memcopy(packet->ether_destination_mac, eth_hdr->destination, sizeof(network_mac_address_t));
    memory_memcopy(packet->ether_source_mac, eth_hdr->source, sizeof(network_mac_address_t));

    if(packet->is_vlan_tagged) {
        network_ethernet_with_vlan_t* vlan_eth_hdr = (network_ethernet_with_vlan_t*)packet->ether_header_offset;

        vlan_eth_hdr->type       = BYTE_SWAP16(NETWORK_PROTOCOL_VLAN);
        vlan_eth_hdr->vlan_tag   = BYTE_SWAP16(ni->vlan_id & 0x0FFF);
        vlan_eth_hdr->inner_type = BYTE_SWAP16(network_packet_get_ethernet_ethertype(packet));
        packet->current_offset   = packet->raw_data + sizeof(network_ethernet_with_vlan_t);
    } else {
        eth_hdr->type          = BYTE_SWAP16(network_packet_get_ethernet_ethertype(packet));
        packet->current_offset = packet->raw_data + sizeof(network_ethernet_t);
    }

    if(packet->is_arp_packet) {
        network_arp_t* arp_hdr = (network_arp_t*)packet->current_offset;

        arp_hdr->hardware_type           = BYTE_SWAP16(NETWORK_ARP_HARDWARE_TYPE_ETHERNET);
        arp_hdr->protocol_type           = BYTE_SWAP16(NETWORK_ARP_PROTOCOL_TYPE_IP);
        arp_hdr->hardware_address_length = NETWORK_ARP_HARDWARE_ADDRESS_LENGTH;
        arp_hdr->protocol_address_length = NETWORK_ARP_PROTOCOL_ADDRESS_LENGTH;
        arp_hdr->operation_code          = BYTE_SWAP16(packet->arp_operation_code);
        memory_memcopy(packet->arp_source_mac, arp_hdr->source_mac, sizeof(network_mac_address_t));
        arp_hdr->source_ip = packet->arp_source_ip;
        memory_memcopy(packet->arp_target_mac, arp_hdr->target_mac, sizeof(network_mac_address_t));
        arp_hdr->target_ip = packet->arp_target_ip;

        packet->current_offset = packet->current_offset + sizeof(network_arp_t);
        packet->payload_offset = packet->current_offset;
        packet->payload_len    = 0;
        packet->raw_data_len   = packet->current_offset - packet->raw_data;
    } else if(packet->is_ipv4_packet) {
        network_ipv4_header_t* ipv4_hdr = (network_ipv4_header_t*)packet->current_offset;

        packet->ipv4_flags = NETWORK_IPV4_FLAG_DONT_FRAGMENT;

        ipv4_hdr->version               = NETWORK_IPV4_VERSION;
        ipv4_hdr->header_length         = sizeof(network_ipv4_header_t) / 4;
        ipv4_hdr->dscp                  = 0;
        ipv4_hdr->ecn                   = 0;
        ipv4_hdr->total_length          = 0;
        ipv4_hdr->identification        = 0;
        ipv4_hdr->flags_fragment_offset = BYTE_SWAP16((packet->ipv4_flags << 13) | (packet->ipv4_fragment_offset & 0x1FFF));
        ipv4_hdr->ttl                   = NETWORK_IPV4_TTL;
        ipv4_hdr->protocol              = 0;
        ipv4_hdr->header_checksum       = 0;
        ipv4_hdr->source_ip             = packet->ipv4_source_ip;
        ipv4_hdr->destination_ip        = packet->ipv4_destination_ip;

        packet->current_offset = packet->current_offset + sizeof(network_ipv4_header_t);

        if(packet->is_icmpv4_packet) {
            network_icmpv4_header_t* icmp_hdr = (network_icmpv4_header_t*)packet->current_offset;

            icmp_hdr->type       = packet->icmpv4_type;
            icmp_hdr->code       = packet->icmpv4_code;
            icmp_hdr->checksum   = 0;
            icmp_hdr->identifier = BYTE_SWAP16(packet->icmpv4_identifier);
            icmp_hdr->sequence   = BYTE_SWAP16(packet->icmpv4_sequence);

            packet->current_offset = packet->current_offset + sizeof(network_icmpv4_header_t);

            if(packet->is_icmpv4_ping_packet) {
                network_icmpv4_ping_header_t* ping_hdr = (network_icmpv4_ping_header_t*)packet->current_offset;

                ping_hdr->timestamp_sec  = BYTE_SWAP32(packet->icmpv4_ping_timestamp_sec);
                ping_hdr->timestamp_usec = BYTE_SWAP32(packet->icmpv4_ping_timestamp_usec);

                packet->current_offset = packet->current_offset + sizeof(network_icmpv4_ping_header_t);
            }

            if(buffer_len > 0) {
                if(packet->current_offset + buffer_len > packet->raw_data + ni->mtu + 22) {
                    PRINTLOG(NETWORK, LOG_ERROR, "buffer too large for mtu");
                    memory_free_ext(heap, packet->raw_data);
                    return -1;
                }

                memory_memcopy(buffer, packet->current_offset, buffer_len);
                packet->current_offset = packet->current_offset + buffer_len;
            }

            ipv4_hdr->protocol     = NETWORK_IPV4_PROTOCOL_ICMPV4;
            ipv4_hdr->total_length = BYTE_SWAP16(packet->current_offset - (uint8_t*)ipv4_hdr);

            icmp_hdr->checksum = network_checksum_calculate((uint8_t*)icmp_hdr, packet->current_offset - (uint8_t*)icmp_hdr);
        } else if(packet->is_tcpv4_packet) {
            network_tcpv4_header_t* tcp_hdr = (network_tcpv4_header_t*)packet->current_offset;

            tcp_hdr->source_port            = BYTE_SWAP16(packet->tcpv4_source_port);
            tcp_hdr->destination_port       = BYTE_SWAP16(packet->tcpv4_destination_port);
            tcp_hdr->sequence_number        = BYTE_SWAP32(packet->tcpv4_sequence_number);
            tcp_hdr->acknowledgement_number = BYTE_SWAP32(packet->tcpv4_acknowledgement_number);
            tcp_hdr->ns                     = packet->tcpv4_ns;
            tcp_hdr->cwr                    = packet->tcpv4_cwr;
            tcp_hdr->ece                    = packet->tcpv4_ece;
            tcp_hdr->urg                    = packet->tcpv4_urg;
            tcp_hdr->ack                    = packet->tcpv4_ack;
            tcp_hdr->psh                    = packet->tcpv4_psh;
            tcp_hdr->rst                    = packet->tcpv4_rst;
            tcp_hdr->syn                    = packet->tcpv4_syn;
            tcp_hdr->fin                    = packet->tcpv4_fin;
            tcp_hdr->window_size            = BYTE_SWAP16(packet->tcpv4_window_size);
            tcp_hdr->checksum               = 0;
            tcp_hdr->urgent_pointer         = BYTE_SWAP16(packet->tcpv4_urgent_pointer);

            packet->current_offset = packet->current_offset + sizeof(network_tcpv4_header_t);

            boolean_t has_options = false;

            if(packet->tcpv4_syn) {
                if(packet->tcpv4_mss > 0) {
                    has_options = true;
                    network_tcpv4_option_t* mss_option = (network_tcpv4_option_t*)(packet->current_offset);
                    mss_option->kind                    = NETWORK_TCPV4_OPTION_KIND_MAX_SEGMENT_SIZE;
                    mss_option->length                  = 4;
                    *(uint16_t*)(void*)mss_option->data = BYTE_SWAP16(packet->tcpv4_mss);

                    packet->current_offset = packet->current_offset + sizeof(network_tcpv4_option_t) + 2; // option header + mss value
                }

                if(packet->tcpv4_window_scale > 0) {
                    has_options = true;
                    network_tcpv4_option_t* window_scale_option = (network_tcpv4_option_t*)(packet->current_offset);
                    window_scale_option->kind    = NETWORK_TCPV4_OPTION_KIND_WINDOW_SCALE;
                    window_scale_option->length  = 3;
                    window_scale_option->data[0] = packet->tcpv4_window_scale;

                    packet->current_offset = packet->current_offset + sizeof(network_tcpv4_option_t) + 1; // option header + window scale value
                }

                if(packet->tcpv4_sack_permitted) {
                    has_options = true;
                    network_tcpv4_option_t* sack_permitted_option = (network_tcpv4_option_t*)(packet->current_offset);
                    sack_permitted_option->kind   = NETWORK_TCPV4_OPTION_KIND_SACK_PERMITTED;
                    sack_permitted_option->length = 2;

                    packet->current_offset = packet->current_offset + sizeof(network_tcpv4_option_t); // option header only, no data
                }
            }

            if(packet->tcpv4_sack_blocks_count > 0) {
                has_options = true;
                network_tcpv4_option_t* sack_option = (network_tcpv4_option_t*)(packet->current_offset);
                sack_option->kind   = NETWORK_TCPV4_OPTION_KIND_SACK;
                sack_option->length = 2 + (packet->tcpv4_sack_blocks_count * 8);

                for(size_t i = 0; i < packet->tcpv4_sack_blocks_count && i < 4; i++) {
                    *(uint32_t*)(void*)(sack_option->data + i * 8)     = BYTE_SWAP32(packet->tcpv4_sack_blocks[i].left_edge);
                    *(uint32_t*)(void*)(sack_option->data + i * 8 + 4) = BYTE_SWAP32(packet->tcpv4_sack_blocks[i].right_edge);
                }

                packet->current_offset = packet->current_offset + sizeof(network_tcpv4_option_t) + (packet->tcpv4_sack_blocks_count * 8);
            }

            if(packet->tcpv4_timestamp > 0) {
                has_options = true;
                network_tcpv4_option_t* timestamp_option = (network_tcpv4_option_t*)(packet->current_offset);
                timestamp_option->kind                          = NETWORK_TCPV4_OPTION_KIND_TIMESTAMP;
                timestamp_option->length                        = 10;
                *(uint32_t*)(void*)timestamp_option->data       = BYTE_SWAP32(packet->tcpv4_timestamp);
                *(uint32_t*)(void*)(timestamp_option->data + 4) = BYTE_SWAP32(packet->tcpv4_timestamp_echo_reply);

                packet->current_offset = packet->current_offset + sizeof(network_tcpv4_option_t) + 8;
            }

            // if there are options and the header length is not a multiple of 4, add padding wtih end of option list option
            if(has_options) {
                network_tcpv4_option_t* end_option = (network_tcpv4_option_t*)(packet->current_offset);
                end_option->kind       = NETWORK_TCPV4_OPTION_KIND_NO_OPERATION;
                packet->current_offset = packet->current_offset + 1;

                uint8_t padding_len = (4 - (packet->current_offset - (uint8_t*)tcp_hdr) % 4) % 4;
                for(size_t i = 0; i < padding_len; i++) {
                    *(uint8_t*)(packet->current_offset + i) = NETWORK_TCPV4_OPTION_KIND_NO_OPERATION;
                }
                packet->current_offset = packet->current_offset + padding_len;
            }

            tcp_hdr->header_length = (packet->current_offset - (uint8_t*)tcp_hdr) / 4;

            memory_memcopy(buffer, packet->current_offset, buffer_len);
            packet->current_offset = packet->current_offset + buffer_len;

            ipv4_hdr->protocol     = NETWORK_IPV4_PROTOCOL_TCPV4;
            ipv4_hdr->total_length = BYTE_SWAP16(packet->current_offset - (uint8_t*)ipv4_hdr);

            uint16_t pseudo_header_checksum = network_checksum_calculate_tcp_udp_pseudo_header(packet->ipv4_source_ip, packet->ipv4_destination_ip, ipv4_hdr->protocol, packet->current_offset - (uint8_t*)tcp_hdr);
            tcp_hdr->checksum = network_checksum_calculate_tcp_udp(pseudo_header_checksum, tcp_hdr, packet->current_offset - (uint8_t*)tcp_hdr);
        } else if(packet->is_udpv4_packet) {
            network_udpv4_header_t* udp_hdr = (network_udpv4_header_t*)packet->current_offset;

            udp_hdr->source_port      = BYTE_SWAP16(packet->udpv4_source_port);
            udp_hdr->destination_port = BYTE_SWAP16(packet->udpv4_destination_port);
            udp_hdr->length           = BYTE_SWAP16(sizeof(network_udpv4_header_t) + buffer_len);
            udp_hdr->checksum         = 0;

            packet->current_offset = packet->current_offset + sizeof(network_udpv4_header_t);

            memory_memcopy(buffer, packet->current_offset, buffer_len);
            packet->current_offset = packet->current_offset + buffer_len;

            ipv4_hdr->protocol     = NETWORK_IPV4_PROTOCOL_UDPV4;
            ipv4_hdr->total_length = BYTE_SWAP16(packet->current_offset - (uint8_t*)ipv4_hdr);

            uint16_t pseudo_header_checksum = network_checksum_calculate_tcp_udp_pseudo_header(packet->ipv4_source_ip, packet->ipv4_destination_ip, ipv4_hdr->protocol, packet->current_offset - (uint8_t*)udp_hdr);
            udp_hdr->checksum = network_checksum_calculate_tcp_udp(pseudo_header_checksum, udp_hdr, packet->current_offset - (uint8_t*)udp_hdr);
        } else {
            PRINTLOG(NETWORK, LOG_ERROR, "unsupported ipv4 protocol for crafting");
            memory_free_ext(heap, packet->raw_data);
            return -1;
        }

        ipv4_hdr->header_checksum = network_checksum_calculate((uint8_t*)ipv4_hdr, sizeof(network_ipv4_header_t));

        packet->raw_data_len = packet->current_offset - packet->raw_data;

        if(packet->raw_data_len < NETWORK_ETHERNET_MIN_FRAME_SIZE) {
            packet->raw_data_len = NETWORK_ETHERNET_MIN_FRAME_SIZE;
        }
    } else {
        PRINTLOG(NETWORK, LOG_ERROR, "unsupported packet type for crafting");
        return -1;
    }

    return 0;

}

int8_t network_packet_process(network_info_t* ni,
                              uint8_t*        packet_data,
                              uint16_t        packet_len,
                              boolean_t       checksums_verified,
                              uint8_t**       response_packet_data,
                              uint16_t*       response_packet_len) {
    network_packet_t packet;
    memory_memclean(&packet, sizeof(network_packet_t));

    network_packet_t return_packet;
    memory_memclean(&return_packet, sizeof(network_packet_t));

    packet.raw_data           = packet_data;
    packet.raw_data_len       = packet_len;
    packet.remaining_data_len = packet_len;
    packet.current_offset     = packet_data;

    time_t parse_start_time, parse_end_time,
           filter_start_time, filter_end_time,
           handle_start_time, handle_end_time;

    parse_start_time = time_ns(NULL);
    if(network_packet_parse(&packet) < 0) {
        PRINTLOG(NETWORK, LOG_TRACE, "failed to parse ethernet header");
        return -1;
    }
    parse_end_time = time_ns(NULL);

    // if packet is fragmented, return icmp fragmentation needed message
    if(packet.is_ipv4_packet && (packet.ipv4_flags & NETWORK_IPV4_FLAG_MORE_FRAGMENTS || packet.ipv4_fragment_offset != 0)) {
        return_packet.is_ipv4_packet      = true;
        return_packet.ipv4_source_ip      = packet.ipv4_destination_ip;
        return_packet.ipv4_destination_ip = packet.ipv4_source_ip;
        return_packet.is_icmpv4_packet    = true;
        return_packet.icmpv4_type         = NETWORK_ICMPV4_TYPE_DESTINATION_UNREACHABLE;
        return_packet.icmpv4_code         = NETWORK_ICMPV4_DESTINATION_UNREACHABLE_CODE_FRAGMENTATION_NEEDED;

        network_packet_craft(ni, &return_packet, NULL, 0);

        *response_packet_data = return_packet.raw_data;
        *response_packet_len  = return_packet.raw_data_len;

        return 0;
    }

    if(!checksums_verified && network_packet_verify_checksum(&packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "checksum verification failed");
        return -1;
    }

    filter_start_time = time_ns(NULL);
    if(network_filter_apply(ni, &packet, NETWORK_FILTER_DIRECTION_INBOUND) == NETWORK_FILTER_ACTION_DROP) {
        PRINTLOG(NETWORK, LOG_TRACE, "packet dropped by filter");
        return -1;
    }
    filter_end_time = time_ns(NULL);

    handle_start_time = time_ns(NULL);
    if(network_connection_packet_handle(ni, &packet) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to handle packet with connection");
        return -1;
    }
    handle_end_time = time_ns(NULL);

    PRINTLOG(NETWORK, LOG_TRACE, "packet processing times: parse=%lld us, filter=%lld us, handle=%lld us",
             (parse_end_time - parse_start_time) / 1000,
             (filter_end_time - filter_start_time) / 1000,
             (handle_end_time - handle_start_time) / 1000);

    return 0;
}
