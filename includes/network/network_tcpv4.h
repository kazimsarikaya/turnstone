/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_TCPV4_H
#define ___NETWORK_TCPV4_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>


typedef struct network_tcp_header_t {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t acknowledgment;
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
}__attribute__((packed)) network_tcp_header_t;

#endif
