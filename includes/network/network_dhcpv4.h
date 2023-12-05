/**
 * @file network_dhcpv4.h
 * @brief DHCPv4 header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_DHCPV4_H
#define ___NETWORK_DHCPV4_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>

typedef enum network_dhcpv4_options_t {
    NETWORK_DHCPV4_OPTION_PAD = 0,
    NETWORK_DHCPV4_OPTION_SUBNETMASK = 1,
    NETWORK_DHCPV4_OPTION_TIMEOFFSET = 2,
    NETWORK_DHCPV4_OPTION_ROUTER = 3,
    NETWORK_DHCPV4_OPTION_TIMESERVER = 4,
    NETWORK_DHCPV4_OPTION_NAMESERVER = 5,
    NETWORK_DHCPV4_OPTION_DOMAINNAMESERVER = 6,
    NETWORK_DHCPV4_OPTION_LOGSERVER = 7,
    NETWORK_DHCPV4_OPTION_COOKIESERVER = 8,
    NETWORK_DHCPV4_OPTION_LPRSERVER = 9,
    NETWORK_DHCPV4_OPTION_IMPRESSSERVER = 10,
    NETWORK_DHCPV4_OPTION_RESOURCELOCATIONSERVER = 11,
    NETWORK_DHCPV4_OPTION_HOSTNAME = 12,
    NETWORK_DHCPV4_OPTION_BOOTFILESIZE = 13,
    NETWORK_DHCPV4_OPTION_MERITDUMPFILE = 14,
    NETWORK_DHCPV4_OPTION_DOMAINNAME = 15,
    NETWORK_DHCPV4_OPTION_SWAPSERVER = 16,
    NETWORK_DHCPV4_OPTION_ROOTPATH = 17,
    NETWORK_DHCPV4_OPTION_EXTENSIONPATH = 18,
    NETWORK_DHCPV4_OPTION_BROADCAST = 28,
    NETWORK_DHCPV4_OPTION_REQUESTIP = 50,
    NETWORK_DHCPV4_OPTION_IPLEASETIME = 51,
    NETWORK_DHCPV4_OPTION_MESSAGETYPE = 53,
    NETWORK_DHCPV4_OPTION_SERVERIP = 54,
    NETWORK_DHCPV4_OPTION_PARAMETER_REQUEST_LIST = 55,
    NETWORK_DHCPV4_OPTION_RENEWALTIME = 58,
    NETWORK_DHCPV4_OPTION_REBINDTIME = 59,
    NETWORK_DHCPV4_OPTION_END = 255,
} network_dhcpv4_options_t;

typedef enum network_dhcpv4_opcode_t {
    NETWORK_DHCPV4_OPCODE_DISCOVER=1,
    NETWORK_DHCPV4_OPCODE_OFFER=2,
    NETWORK_DHCPV4_OPCODE_REQUEST=3,
    NETWORK_DHCPV4_OPCODE_DECLINE=4,
    NETWORK_DHCPV4_OPCODE_ACK=5,
    NETWORK_DHCPV4_OPCODE_NACK=6,
    NETWORK_DHCPV4_OPCODE_RELEASE=7,
    NETWORK_DHCPV4_OPCODE_INFORM=8,
    NETWORK_DHCPV4_OPCODE_FORCERENEW=9,
} network_dhcpv4_opcode_t;

#define NETWORK_DHCPV4_HTYPE 0x01
#define NETWORK_DHCPV4_HLEN 0x06
#define NETWORK_DHCPV4_HOPS 0x00

#define NETWORK_DHCPV4_MAGICCOOKIE 0x63825363

#define NETWORK_DHCPV4_SOURCE_PORT 68
#define NETWORK_DHCPV4_DESTINATION_PORT 67

typedef struct network_dhcpv4_t{
    network_dhcpv4_opcode_t opcode : 8;
    uint8_t hardware_type;
    uint8_t hardware_address_length;
    uint8_t hops;
    uint32_t xid;
    uint16_t seconds;
    uint16_t flags;
    network_ipv4_address_t client_ip;
    network_ipv4_address_t your_ip;
    network_ipv4_address_t server_ip;
    network_ipv4_address_t relay_ip;
    network_mac_address_t client_mac_address;
    uint8_t padding[10]; ///< wtf?
    uint8_t sname[64];
    uint8_t bootp_file[128];
    uint32_t magic_cookie;
    uint8_t options[];
} __attribute((packed)) network_dhcpv4_t;

int32_t  network_dhcpv4_send_discover(uint64_t args_cnt, void** args);
uint8_t* network_dhcpv4_process_packet(network_dhcpv4_t* recv_dhcpv4_packet, void* network_info, uint16_t* return_packet_len);

#endif
