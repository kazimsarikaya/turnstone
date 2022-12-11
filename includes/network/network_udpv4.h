/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_UDPV4_H
#define ___NETWORK_UDPV4_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>



typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
}__attribute__((packed)) network_udpv4_header_t;



#endif
