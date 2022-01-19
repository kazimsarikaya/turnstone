/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_PROTOCOLS
#define ___NETWORK_PROTOCOLS 0

#include <types.h>
#include <driver/network.h>

#define NETWORK_PROTOCOL_ARP 0x0806
#define NETWORK_PROTOCOL_IP  0x0800

typedef uint8_t mac_address_t[6];
typedef uint8_t ipv4_address_t[4];

typedef enum {
	NETWORK_ETHERNET_TYPE_ARP=NETWORK_PROTOCOL_ARP,
	NETWORK_ETHERNET_TYPE_IP=NETWORK_PROTOCOL_IP,
} network_ethernet_type_t;

typedef struct {
	mac_address_t destination;
	mac_address_t source;
	network_ethernet_type_t type : 16;
}__attribute__((packed)) network_ethernet_t;


#define NETWORK_ARP_HARDWARE_TYPE_ETHERNET 1
#define NETWORK_ARP_PROTOCOL_TYPE_IP 0x0800

#define NETWORK_ARP_HARDWARE_ADDRESS_LENGTH 6
#define NETWORK_ARP_PROTOCOL_ADDRESS_LENGTH 4

#define NETWORK_ARP_OPERATION_CODE_REQUEST 1
#define NETWORK_ARP_OPERATION_CODE_ANSWER 2

typedef struct {
	uint16_t hardware_type;
	uint16_t protocol_type;
	uint8_t hardware_address_length;
	uint8_t protocol_address_length;
	uint16_t operation_code;
	mac_address_t source_mac;
	ipv4_address_t source_ip;
	mac_address_t target_mac;
	ipv4_address_t target_ip;
}__attribute__((packed)) network_arp_t;

#define NETWORK_ARP_ETHERNET_PACKET_LEN (sizeof(network_ethernet_t) + sizeof(network_arp_t))

#define NETWORK_IPV4_VERSION 4
#define NETWORK_IPV4_TTL 128

#define NETWORK_IPV4_FLAG_DONT_FRAGMENT  1
#define NETWORK_IPV4_FLAG_MORE_FRAGMENTS 2

typedef enum {
	NETWORK_IPV4_PROTOCOL_ICMP=1,
	NETWORK_IPV4_PROTOCOL_IGMP=2,
	NETWORK_IPV4_PROTOCOL_TCP=6,
	NETWORK_IPV4_PROTOCOL_UDP=17,
}network_ipv4_protocol_t;

typedef struct {
	uint32_t header_length : 4;
	uint32_t version : 4;
	uint32_t ecn : 2;
	uint32_t dscp : 6;
	uint32_t total_length : 16;
	uint32_t identification : 16;
	uint32_t fragment_offset : 13;
	uint32_t flags : 3;
	uint32_t ttl : 8;
	network_ipv4_protocol_t protocol : 8;
	uint32_t header_checksum : 16;
	ipv4_address_t source_ip;
	ipv4_address_t destination_ip;
}__attribute__((packed)) network_ipv4_header_t;

typedef enum {
	NETWORK_ICMP_ECHO_REQUEST=8,
	NETWORK_ICMP_ECHO_REPLY=0,
}network_icmp_type_t;

#define NETWORK_ICMP_ECHO_CODE 0
#define NETWORK_ICMP_MIN_DATA_SIZE 48

typedef struct {
	network_icmp_type_t type : 8;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence;
}__attribute__((packed)) network_icmp_header_t;

typedef struct {
	network_icmp_header_t header;
	uint32_t timestamp_sec;
	uint32_t timestamp_usec;
}__attribute__((packed)) network_icmp_ping_header_t;

typedef struct {
	uint16_t source_port;
	uint16_t destination_port;
	uint16_t length;
	uint16_t checksum;
}__attribute__((packed)) network_udp_header_t;

typedef struct {
	uint16_t source_port;
	uint16_t destination_port;
	uint32_t sequence;
	uint32_t acknowledgment;
	uint8_t ns : 1;
	uint8_t reserved : 3;
	uint8_t header_length : 4;
	uint8_t fin : 1;
	uint8_t syn : 1;
	uint8_t rst : 1;
	uint8_t psh : 1;
	uint8_t ack : 1;
	uint8_t urg : 1;
	uint8_t ece : 1;
	uint8_t cwr : 1;
	uint16_t window_size;
	uint16_t checksum;
	uint16_t urgent_pointer;
}__attribute__((packed)) network_tcp_header_t;

typedef struct {
	uint64_t packet_len;
	uint8_t* packet_data;
	linkedlist_t return_queue;
	mac_address_t device_mac_address;
} network_received_packet_t;

typedef struct {
	uint64_t packet_len;
	uint8_t* packet_data;
} network_transmit_packet_t;

uint16_t network_ipv4_header_checksum(network_ipv4_header_t* ipv4_hdr);
int8_t network_ipv4_header_checksum_verify(network_ipv4_header_t* ipv4_hdr);

network_icmp_ping_header_t* network_create_ping_packet(boolean_t is_reply, uint16_t identifier, uint16_t sequence, uint16_t data_len, uint8_t* data, uint16_t* packet_len);

network_udp_header_t* network_create_udp_packet_from_data(uint16_t sp, uint16_t dp, uint16_t len, uint8_t* data);
network_ipv4_header_t* network_create_ipv4_packet_from_udp_packet(ipv4_address_t sip, ipv4_address_t dip, network_udp_header_t* udp_hdr);
network_ipv4_header_t* network_create_ipv4_packet_from_icmp_packet(ipv4_address_t sip, ipv4_address_t dip, network_icmp_header_t* icmp_hdr, uint16_t icmp_packet_len);

network_ethernet_t* network_create_ethernet_packet_from_ipv4_packet(mac_address_t dst, mac_address_t src, network_ipv4_header_t* ipv4_hdr, uint16_t* len);

network_ethernet_t* network_create_arp_request(mac_address_t src_mac, ipv4_address_t src_ip, ipv4_address_t tgt_ip);
network_ethernet_t* network_create_arp_reply_from_packet(network_received_packet_t* packet);

network_ethernet_t* network_process_packet(network_received_packet_t* packet, uint16_t* return_packet_len);
network_ethernet_t* network_process_ipv4_packet_from_packet(network_received_packet_t* packet, uint16_t* return_packet_len);

#endif
