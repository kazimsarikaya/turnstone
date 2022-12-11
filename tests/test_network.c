/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <driver/network_protocols.h>
#include <strings.h>
#include <utils.h>

typedef uint32_t time_t;

time_t time(time_t* t);

struct timeb {
    time_t         time;
    unsigned short millitm;
    short          timezone;
    short          dstflag;
};

int ftime(struct timeb* tp);

uint64_t time_ns(uint64_t* t){
    struct timeb timer_msec;

    ftime(&timer_msec);

    uint64_t res = ((uint64_t) timer_msec.time) * 1000ll +
                   (uint64_t) timer_msec.millitm;
    res *= 1000000;

    if(t) {
        *t = res;
    }

    return res;
}

#define PCAP_MAGIC 0xA1B2C3D4
#define PCAP_VERSION_MAJOR 2
#define PCAP_VERSION_MINOR 4

typedef struct {
    uint32_t magic;
    uint16_t major_version;
    uint16_t minor_version;
    uint64_t reserved;
    uint32_t snaplen;
    uint32_t link_type;
}__attribute__((packed)) pcap_file_header_t;

typedef struct {
    uint32_t timestamp;
    uint32_t timestamp2;
    uint32_t packet_len_captured;
    uint32_t packet_len_original;
}__attribute__((packed)) pcap_packet_header_t;

uint32_t main(uint32_t argc, char_t** argv) {
    setup_ram();

    UNUSED(argc);
    UNUSED(argv);

    char_t* test = "hello world packet!";

    PRINTLOG(KERNEL, LOG_INFO, "deneme mesajı %i", 1234);

    pcap_file_header_t* pcap = memory_malloc(sizeof(pcap_file_header_t));

    pcap->magic = PCAP_MAGIC;
    pcap->major_version = PCAP_VERSION_MAJOR;
    pcap->minor_version = PCAP_VERSION_MINOR;
    pcap->snaplen = 262144;
    pcap->link_type = NETWORK_ARP_HARDWARE_TYPE_ETHERNET;

    FILE* pcap_file = fopen("tmp/test.pcap", "w");

    fwrite(pcap, sizeof(pcap_file_header_t), 1, pcap_file);

    memory_free(pcap);

    pcap_packet_header_t ph;

    ph.timestamp = time(NULL);
    ph.packet_len_captured = NETWORK_ARP_ETHERNET_PACKET_LEN;
    ph.packet_len_original = NETWORK_ARP_ETHERNET_PACKET_LEN;

    fwrite(&ph, sizeof(pcap_packet_header_t), 1, pcap_file);

    network_mac_address_t src_mac = {0x54, 0x50, 0x00, 0x01, 0x02, 0x03};
    network_mac_address_t dst_mac = {0x54, 0x50, 0x00, 0x03, 0x02, 0x01};

    network_ipv4_address_t src_ip = {192, 168, 122, 5};
    network_ipv4_address_t dst_ip = {192, 168, 122, 1};

    network_ethernet_t* arp1 = network_create_arp_request(src_mac, src_ip, dst_ip);

    fwrite(arp1, NETWORK_ARP_ETHERNET_PACKET_LEN, 1, pcap_file);

    memory_free(arp1);

    network_udp_header_t* up = network_create_udp_packet_from_data(5060, 8070, strlen(test), (uint8_t*)test);
    network_ipv4_header_t* ip = network_create_ipv4_packet_from_udp_packet(src_ip, dst_ip, up);
    uint16_t ep_len = 0;
    network_ethernet_t* ep1 = network_create_ethernet_packet_from_ipv4_packet(dst_mac, src_mac, ip, &ep_len);

    ph.timestamp = time(NULL);
    ph.packet_len_captured = ep_len;
    ph.packet_len_original = ep_len;

    fwrite(&ph, sizeof(pcap_packet_header_t), 1, pcap_file);

    fwrite(ep1, ep_len, 1, pcap_file);

    memory_free(ep1);

    uint8_t ping_data[NETWORK_ICMP_MIN_DATA_SIZE];

    for(int16_t i = 0; i < NETWORK_ICMP_MIN_DATA_SIZE; i++) {
        ping_data[i] = 0x08 + i;
    }

    uint16_t pp_len = 0;
    network_icmp_ping_header_t* ping = network_create_ping_packet(0, 0x1234, 0x0, NETWORK_ICMP_MIN_DATA_SIZE, ping_data, &pp_len);
    ip = network_create_ipv4_packet_from_icmp_packet(src_ip, dst_ip, (network_icmp_header_t*)ping, pp_len);
    ep1 = network_create_ethernet_packet_from_ipv4_packet(dst_mac, src_mac, ip, &ep_len);

    ph.timestamp = time(NULL);
    ph.packet_len_captured = ep_len;
    ph.packet_len_original = ep_len;

    fwrite(&ph, sizeof(pcap_packet_header_t), 1, pcap_file);

    fwrite(ep1, ep_len, 1, pcap_file);

    memory_free(ep1);

    ping = network_create_ping_packet(1, 0x1234, 0x0, NETWORK_ICMP_MIN_DATA_SIZE, ping_data, &pp_len);
    ip = network_create_ipv4_packet_from_icmp_packet(dst_ip, src_ip, (network_icmp_header_t*)ping, pp_len);
    ep1 = network_create_ethernet_packet_from_ipv4_packet(src_mac, dst_mac, ip, &ep_len);

    ph.timestamp = time(NULL);
    ph.packet_len_captured = ep_len;
    ph.packet_len_original = ep_len;

    fwrite(&ph, sizeof(pcap_packet_header_t), 1, pcap_file);

    fwrite(ep1, ep_len, 1, pcap_file);

    memory_free(ep1);


    fclose(pcap_file);

    printf("0x%04x\n", BYTE_SWAP16(0x1234));

    return 0;
}
