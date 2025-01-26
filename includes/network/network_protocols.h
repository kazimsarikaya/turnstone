/**
 * @file network_protocols.h
 * @brief Network protocols header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_PROTOCOLS_H
#define ___NETWORK_PROTOCOLS_H 0

#include <types.h>
#include <network.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_PROTOCOL_ARP 0x0806
#define NETWORK_PROTOCOL_IPV4  0x0800


typedef union network_ipv4_address_t {
    uint8_t  as_bytes[4];
    uint16_t as_words[2];
    uint32_t as_dword;
} network_ipv4_address_t;

typedef uint8_t network_mac_address_t[6];

#ifdef __cplusplus
}
#endif


#endif
