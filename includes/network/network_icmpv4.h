/**
 * @file network_icmpv4.h
 * @brief ICMPv4 header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_ICMPV4_H
#define ___NETWORK_ICMPV4_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum network_icmpv4_type_t {
    NETWORK_ICMP_ECHO_REQUEST=8,
    NETWORK_ICMP_ECHO_REPLY=0,
}network_icmpv4_type_t;

#define NETWORK_ICMP_ECHO_CODE 0
#define NETWORK_ICMP_MIN_DATA_SIZE 48

typedef struct network_icmpv4_header_t {
    network_icmpv4_type_t type : 8;
    uint8_t               code;
    uint16_t              checksum;
    uint16_t              identifier;
    uint16_t              sequence;
}__attribute__((packed)) network_icmpv4_header_t;

typedef struct network_icmpv4_ping_header_t {
    network_icmpv4_header_t header;
    uint32_t                timestamp_sec;
    uint32_t                timestamp_usec;
}__attribute__((packed)) network_icmpv4_ping_header_t;

uint8_t* network_icmpv4_process_packet(network_icmpv4_header_t* recv_icmpv4_packet, void* network_info, uint16_t* return_packet_len);

#ifdef __cplusplus
}
#endif

#endif
