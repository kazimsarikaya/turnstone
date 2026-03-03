/**
 * @file network_packet.h
 * @brief Network packet header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_PACKET_H
#define ___NETWORK_PACKET_H 0

#include <types.h>
#include <network/network_info.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum network_protocol_t : uint16_t {
    NETWORK_PROTOCOL_IPV4 = 0x0800,
    NETWORK_PROTOCOL_ARP  = 0x0806,
    NETWORK_PROTOCOL_IPV6 = 0x86DD,
    NETWORK_PROTOCOL_VLAN = 0x8100,
} network_protocol_t;

typedef enum network_application_port_t : uint16_t {
    NETWORK_APPLICATION_PORT_ECHO_SERVER = 7,
    NETWORK_APPLICATION_PORT_DHCP_SERVER = 67,
    NETWORK_APPLICATION_PORT_DHCP_CLIENT = 68,
} network_application_port_t;

typedef enum network_ipv4_protocol_t : uint8_t {
    NETWORK_IPV4_PROTOCOL_ICMPV4 = 1,
    NETWORK_IPV4_PROTOCOL_IGMPV4 = 2,
    NETWORK_IPV4_PROTOCOL_TCPV4  = 6,
    NETWORK_IPV4_PROTOCOL_UDPV4  = 17,
} network_ipv4_protocol_t;

typedef enum network_icmpv4_type_t : uint8_t {
    NETWORK_ICMPV4_TYPE_ECHO_REPLY              = 0,
    NETWORK_ICMPV4_TYPE_DESTINATION_UNREACHABLE = 3,
    NETWORK_ICMPV4_TYPE_SOURCE_QUENCH           = 4,
    NETWORK_ICMPV4_TYPE_REDIRECT                = 5,
    NETWORK_ICMPV4_TYPE_ECHO_REQUEST            = 8,
    NETWORK_ICMPV4_TYPE_TIME_EXCEEDED           = 11,
    NETWORK_ICMPV4_TYPE_PARAMETER_PROBLEM       = 12,
} network_icmpv4_type_t;

typedef enum network_icmpv4_echo_request_reply_code_t {
    NETWORK_ICMPV4_ECHO_REQUEST_REPLY_CODE_NO_CODE = 0,
} network_icmpv4_echo_request_reply_code_t;

typedef enum network_icmpv4_destination_unreachable_code_t {
    NETWORK_ICMPV4_DESTINATION_UNREACHABLE_CODE_NET_UNREACHABLE      = 0,
    NETWORK_ICMPV4_DESTINATION_UNREACHABLE_CODE_HOST_UNREACHABLE     = 1,
    NETWORK_ICMPV4_DESTINATION_UNREACHABLE_CODE_PROTOCOL_UNREACHABLE = 2,
    NETWORK_ICMPV4_DESTINATION_UNREACHABLE_CODE_PORT_UNREACHABLE     = 3,
    NETWORK_ICMPV4_DESTINATION_UNREACHABLE_CODE_FRAGMENTATION_NEEDED = 4,
    NETWORK_ICMPV4_DESTINATION_UNREACHABLE_CODE_SOURCE_ROUTE_FAILED  = 5,
} network_icmpv4_destination_unreachable_code_t;

#define NETWORK_ETHERNET_MIN_FRAME_SIZE 60

typedef enum network_arp_hardware_type_t : uint16_t {
    NETWORK_ARP_HARDWARE_TYPE_ETHERNET = 1,
} network_arp_hardware_type_t;

typedef enum network_arp_protocol_type_t : uint16_t {
    NETWORK_ARP_PROTOCOL_TYPE_IP = 0x0800,
} network_arp_protocol_type_t;

#define NETWORK_ARP_HARDWARE_ADDRESS_LENGTH 6
#define NETWORK_ARP_PROTOCOL_ADDRESS_LENGTH 4

typedef enum network_arp_operation_code_t : uint16_t {
    NETWORK_ARP_OPERATION_CODE_REQUEST = 1,
    NETWORK_ARP_OPERATION_CODE_REPLY   = 2,
} network_arp_operation_code_t;

#define NETWORK_IPV4_HEADER_LENGTH 20
#define NETWORK_TCPV4_HEADER_LENGTH 20

typedef struct network_packet_t {
    uint8_t*  raw_data;
    uint16_t  raw_data_len;
    uint16_t  remaining_data_len; // used for parsing to keep track of remaining data length after parsing each header
    uint8_t*  current_offset;
    uint8_t*  ether_header_offset;
    boolean_t is_arp_packet;
    uint8_t*  arp_header_offset;
    boolean_t is_ipv4_packet;
    uint8_t*  ipv4_header_offset;
    boolean_t is_icmpv4_packet;
    uint8_t*  icmpv4_header_offset;
    boolean_t is_icmpv4_ping_packet;
    uint8_t*  icmpv4_ping_header_offset;
    boolean_t is_tcpv4_packet;
    uint8_t*  tcpv4_header_offset;
    boolean_t is_udpv4_packet;
    uint8_t*  udpv4_header_offset;
    uint8_t*  payload_offset;
    uint16_t  payload_len;

    // ethernet header fields
    network_mac_address_t ether_source_mac;
    network_mac_address_t ether_destination_mac;
    network_protocol_t    ether_ethertype;

    // vlan fields
    boolean_t          is_vlan_tagged;
    uint16_t           ether_vlan_id;
    network_protocol_t ether_inner_ethertype;

    // arp header fields
    uint16_t               arp_hardware_type;
    uint16_t               arp_protocol_type;
    uint8_t                arp_hardware_address_length;
    uint8_t                arp_protocol_address_length;
    uint16_t               arp_operation_code;
    network_mac_address_t  arp_source_mac;
    network_ipv4_address_t arp_source_ip;
    network_mac_address_t  arp_target_mac;
    network_ipv4_address_t arp_target_ip;


    // ipv4 header fields
    uint8_t                 ipv4_version;
    uint8_t                 ipv4_ihl;
    uint8_t                 ipv4_dscp;
    uint8_t                 ipv4_ecn;
    uint16_t                ipv4_total_length;
    uint16_t                ipv4_identification;
    uint8_t                 ipv4_flags;
    uint16_t                ipv4_fragment_offset;
    uint8_t                 ipv4_ttl;
    uint16_t                ipv4_header_checksum;
    network_ipv4_address_t  ipv4_source_ip;
    network_ipv4_address_t  ipv4_destination_ip;
    network_ipv4_protocol_t ipv4_protocol;

    // icmpv4 header fields
    network_icmpv4_type_t icmpv4_type;
    uint8_t               icmpv4_code;
    uint16_t              icmpv4_checksum;
    uint16_t              icmpv4_identifier;
    uint16_t              icmpv4_sequence;

    // icmpv4 ping header fields
    uint32_t icmpv4_ping_timestamp_sec;
    uint32_t icmpv4_ping_timestamp_usec;

    // tcpv4 header fields
    uint16_t  tcpv4_source_port;
    uint16_t  tcpv4_destination_port;
    uint32_t  tcpv4_sequence_number;
    uint32_t  tcpv4_acknowledgement_number;
    uint8_t   tcpv4_header_length;
    boolean_t tcpv4_ns;
    boolean_t tcpv4_fin;
    boolean_t tcpv4_syn;
    boolean_t tcpv4_rst;
    boolean_t tcpv4_psh;
    boolean_t tcpv4_ack;
    boolean_t tcpv4_urg;
    boolean_t tcpv4_ece;
    boolean_t tcpv4_cwr;
    uint16_t  tcpv4_window_size;
    uint16_t  tcpv4_checksum;
    uint16_t  tcpv4_urgent_pointer;

    // tcpv4 options.
    uint16_t  tcpv4_mss;
    uint16_t  tcpv4_window_scale;
    boolean_t tcpv4_sack_permitted;
    uint8_t   tcpv4_sack_blocks_count;
    struct {
        uint32_t left_edge;
        uint32_t right_edge;
    }        tcpv4_sack_blocks[4];
    uint32_t tcpv4_timestamp;
    uint32_t tcpv4_timestamp_echo_reply;

    // udpv4 header fields
    uint16_t udpv4_source_port;
    uint16_t udpv4_destination_port;
    uint16_t udpv4_length;
    uint16_t udpv4_checksum;


} network_packet_t;

int8_t network_packet_process(network_info_t* ni,
                              uint8_t*        packet_data,
                              uint16_t        packet_len,
                              boolean_t       checksums_verified,
                              uint8_t**       response_packet_data,
                              uint16_t*       response_packet_len);

int8_t network_packet_craft(const network_info_t* ni,
                            network_packet_t*     packet,
                            uint8_t*              buffer,
                            uint16_t              buffer_len);


#ifdef __cplusplus
}
#endif

#endif
