/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_ARP_H
#define ___NETWORK_ARP_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>
#include <network/network_ethernet.h>
#include <network/network_ipv4.h>

#define NETWORK_ARP_HARDWARE_TYPE_ETHERNET 1
#define NETWORK_ARP_PROTOCOL_TYPE_IP 0x0800

#define NETWORK_ARP_HARDWARE_ADDRESS_LENGTH 6
#define NETWORK_ARP_PROTOCOL_ADDRESS_LENGTH 4

#define NETWORK_ARP_OPERATION_CODE_REQUEST 1
#define NETWORK_ARP_OPERATION_CODE_ANSWER 2

typedef struct {
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

uint8_t* network_arp_process_packet(network_arp_t* recv_arp_packet, void* network_info, uint16_t* return_packet_len);


#endif
