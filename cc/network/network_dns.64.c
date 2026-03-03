/**
 * @file network_dns.64.c
 * @brief DNS protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_dns.h>
#include <network/network_connection.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>
#include <random.h>

MODULE("turnstone.lib.network");

extern memory_heap_t* network_packet_heap;

typedef enum network_dns_class_t : uint16_t {
    NETWORK_DNS_CLASS_IN = 1, // Internet
} network_dns_class_t;

typedef struct network_dns_query_header_t {
    uint16_t id;
    uint16_t flags;
    uint16_t question_count;
    uint16_t answer_count;
    uint16_t authority_count;
    uint16_t additional_count;
} __attribute((packed)) network_dns_query_header_t;

#define NETWORK_DNS_MAX_IPV4_ADDRESSES 16

struct network_dns_query_result_t {
    network_dns_record_type_t record_type;
    uint32_t                  record_count;
    union {
        network_ipv4_address_t ipv4_addresses[NETWORK_DNS_MAX_IPV4_ADDRESSES];
        // other record types can be added here
    };
};

network_dns_query_result_t* network_dns_query(network_ipv4_address_t    dns_server,
                                              const char_t*             domain_name,
                                              network_dns_record_type_t record_type) {
    network_info_t* ni = network_get_network_info_by_ipv4_address(dns_server);

    if(!ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "No network interface found for DNS server: %i.%i.%i.%i",
                 dns_server.as_bytes[0], dns_server.as_bytes[1], dns_server.as_bytes[2], dns_server.as_bytes[3]);
        return NULL;
    }

    network_listener_t* listener = network_listener_create_udpv4_client(ni, ni->ipv4_address, 0);

    if(!listener) {
        PRINTLOG(NETWORK, LOG_ERROR, "Failed to create UDPv4 client listener for DNS query");
        return NULL;
    }

    network_connection_t* connection = network_connection_connect(listener, dns_server, 53);

    if(!connection) {
        PRINTLOG(NETWORK, LOG_ERROR, "Failed to connect to DNS server: %i.%i.%i.%i",
                 dns_server.as_bytes[0], dns_server.as_bytes[1], dns_server.as_bytes[2], dns_server.as_bytes[3]);
        network_listener_destroy(listener);
        return NULL;
    }

    network_dns_query_result_t* result = NULL;
    int8_t err                         = -1;

    uint8_t dns_query_packet[512];

    network_dns_query_header_t* header = (network_dns_query_header_t*)dns_query_packet;

    header->id               = rand();
    header->flags            = BYTE_SWAP16(0x0100); // standard query with recursion desired
    header->question_count   = BYTE_SWAP16(1);
    header->answer_count     = 0;
    header->authority_count  = 0;
    header->additional_count = 0;

    uint8_t* qname_ptr = dns_query_packet + sizeof(network_dns_query_header_t);

    const char_t* label_start = domain_name;
    const char_t* label_end   = domain_name;

    while(*label_end) {
        if(*label_end == '.') {
            uint8_t label_length = label_end - label_start;

            if(label_length > 63) {
                PRINTLOG(NETWORK, LOG_ERROR, "Label in domain name is too long: %.*s", label_length, label_start);
                goto cleanup;
            }

            *qname_ptr++ = label_length;
            memory_memcopy(label_start, qname_ptr, label_length);
            qname_ptr += label_length;

            label_end++;
            label_start = label_end;
        } else {
            label_end++;
        }
    }

    uint8_t label_length = label_end - label_start;

    if(label_length > 63) {
        PRINTLOG(NETWORK, LOG_ERROR, "Label in domain name is too long: %.*s", label_length, label_start);
        goto cleanup;
    }

    *qname_ptr++ = label_length;
    memory_memcopy(label_start, qname_ptr, label_length);
    qname_ptr += label_length;

    *qname_ptr++ = 0; // end of qname

    uint16_t* qtype_ptr  = (uint16_t*)(void*)qname_ptr;
    uint16_t* qclass_ptr = (uint16_t*)(void*)(qname_ptr + sizeof(uint16_t));

    *qtype_ptr  = BYTE_SWAP16(record_type);
    *qclass_ptr = BYTE_SWAP16(NETWORK_DNS_CLASS_IN); // IN class

    uint16_t dns_query_packet_len = sizeof(network_dns_query_header_t) + (qname_ptr - dns_query_packet) + sizeof(uint16_t) * 2;

    if(network_connection_send(connection, dns_query_packet, dns_query_packet_len) < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "Failed to send DNS query to server: %i.%i.%i.%i",
                 dns_server.as_bytes[0], dns_server.as_bytes[1], dns_server.as_bytes[2], dns_server.as_bytes[3]);
        goto cleanup;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "Sent DNS query for domain '%s' to server %i.%i.%i.%i",
             domain_name,
             dns_server.as_bytes[0], dns_server.as_bytes[1], dns_server.as_bytes[2], dns_server.as_bytes[3]);

    uint8_t dns_response_packet[512];

    int32_t recv_len = network_connection_receive(connection, dns_response_packet, sizeof(dns_response_packet));

    if(recv_len < 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "Failed to receive DNS response from server: %i.%i.%i.%i",
                 dns_server.as_bytes[0], dns_server.as_bytes[1], dns_server.as_bytes[2], dns_server.as_bytes[3]);
        goto cleanup;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "Received DNS response of length %i", recv_len);

    network_dns_query_header_t* response_header = (network_dns_query_header_t*)dns_response_packet;

    if(response_header->id != header->id) {
        PRINTLOG(NETWORK, LOG_ERROR, "DNS response ID mismatch. Expected %u, got %u", header->id, response_header->id);
        goto cleanup;
    }

    if((BYTE_SWAP16(response_header->flags) & 0x8000) == 0) { // QR bit, 0 for query, 1 for response
        PRINTLOG(NETWORK, LOG_ERROR, "DNS response is not a response packet.");
        goto cleanup;
    }

    uint16_t rcode = BYTE_SWAP16(response_header->flags) & 0x000F;
    if(rcode != 0) {
        PRINTLOG(NETWORK, LOG_ERROR, "DNS response error code: %u", rcode);
        goto cleanup;
    }

    uint16_t question_count = BYTE_SWAP16(response_header->question_count);
    uint16_t answer_count   = BYTE_SWAP16(response_header->answer_count);

    if(question_count != 1) {
        PRINTLOG(NETWORK, LOG_ERROR, "DNS response has unexpected question count: %u", question_count);
        goto cleanup;
    }

    uint8_t* current_ptr = dns_response_packet + sizeof(network_dns_query_header_t);

    // Skip question section
    boolean_t is_pointer = false;
    while(*current_ptr != 0) {
        if((*current_ptr & 0xC0) == 0xC0) { // Pointer
            current_ptr += 2;
            is_pointer   = true;
            break;
        } else {
            current_ptr += *current_ptr + 1;
        }
    }
    if(!is_pointer) {
        current_ptr++; // Skip the 0 byte at the end of the name
    }

    current_ptr += sizeof(uint16_t) * 2; // Skip QTYPE and QCLASS

    result = memory_malloc_ext(network_packet_heap, sizeof(network_dns_query_result_t), 0);
    if(!result) {
        PRINTLOG(NETWORK, LOG_ERROR, "Failed to allocate memory for DNS query result");
        goto cleanup;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "Parsing DNS answer section with %u answers", answer_count);

    for(uint16_t i = 0; i < answer_count; ++i) {
        // Skip name
        is_pointer = false;
        while(*current_ptr != 0) {
            if((*current_ptr & 0xC0) == 0xC0) { // Pointer
                current_ptr += 2;
                is_pointer   = true;
                break;
            } else {
                current_ptr += *current_ptr + 1;
            }
        }
        if(!is_pointer) {
            current_ptr++; // Skip the 0 byte at the end of the name
        }

        uint16_t type = BYTE_SWAP16(*(uint16_t*)(void*)current_ptr);
        current_ptr += sizeof(uint16_t);
        uint16_t class = BYTE_SWAP16(*(uint16_t*)(void*)current_ptr);
        current_ptr += sizeof(uint16_t);
        uint32_t ttl = BYTE_SWAP32(*(uint32_t*)(void*)current_ptr);
        current_ptr += sizeof(uint32_t);
        uint16_t rdlength = BYTE_SWAP16(*(uint16_t*)(void*)current_ptr);
        current_ptr += sizeof(uint16_t);

        PRINTLOG(NETWORK, LOG_TRACE, "DNS answer %u: type=%u class=%u ttl=%u rdlength=%u", i + 1, type, class, ttl, rdlength);

        UNUSED(ttl);

        if(type == NETWORK_DNS_RECORD_TYPE_A && class == NETWORK_DNS_CLASS_IN) { // A record, IN class
            if(rdlength == sizeof(network_ipv4_address_t)) {
                if(result->record_count < NETWORK_DNS_MAX_IPV4_ADDRESSES) {
                    memory_memcopy(current_ptr, &result->ipv4_addresses[result->record_count], sizeof(network_ipv4_address_t));
                    result->record_count++;
                } else {
                    PRINTLOG(NETWORK, LOG_WARNING, "Too many IPv4 addresses in DNS response, skipping some.");
                }
            }
        } else {
            PRINTLOG(NETWORK, LOG_WARNING, "Unsupported DNS record type %u or class %u, skipping.", type, class);
        }
        current_ptr += rdlength;
    }

    err = 0;

cleanup:
    if(err < 0 && result) {
        memory_free_ext(network_packet_heap, result);
        result = NULL;
    }

    network_connection_close(connection);
    network_connection_destroy(connection);
    network_listener_destroy(listener);

    return result;
}

void network_dns_query_result_free(network_dns_query_result_t* result) {
    if(result) {
        memory_free_ext(network_packet_heap, result);
    }
}

network_ipv4_address_t network_dns_query_result_get_ipv4_address(network_dns_query_result_t* result, uint32_t index) {
    if(result == NULL || index >= result->record_count) {
        return (network_ipv4_address_t){0};
    }

    return result->ipv4_addresses[index];
}

uint32_t network_dns_query_result_get_ipv4_address_count(network_dns_query_result_t* result) {
    if(result == NULL) {
        return 0;
    }

    return result->record_count;
}
