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
#include <strings.h>

MODULE("turnstone.lib.network");

extern memory_heap_t* network_packet_heap;

typedef struct network_dns_query_header_t {
    uint16_t id;
    uint16_t flags;
    uint16_t question_count;
    uint16_t answer_count;
    uint16_t authority_count;
    uint16_t additional_count;
} __attribute((packed)) network_dns_query_header_t;

#define NETWORK_DNS_MAX_RECORDS 16

typedef struct network_dns_mx_record_t {
    uint16_t preference;
    char_t*  exchange;
} network_dns_mx_record_t;

typedef struct network_dns_soa_record_t {
    char_t*  mname;
    char_t*  rname;
    uint32_t serial;
    uint32_t refresh;
    uint32_t retry;
    uint32_t expire;
    uint32_t minimum;
} network_dns_soa_record_t;

struct network_dns_query_result_t {
    network_dns_record_type_t record_type;
    uint32_t                  record_count;
    union {
        network_ipv4_address_t   ipv4_addresses[NETWORK_DNS_MAX_RECORDS];
        char_t*                  names[NETWORK_DNS_MAX_RECORDS]; // For NS, CNAME, PTR, TXT
        network_dns_mx_record_t  mx_records[NETWORK_DNS_MAX_RECORDS];
        network_dns_soa_record_t soa_records[NETWORK_DNS_MAX_RECORDS];
    };
};

static int32_t parse_dns_name(const uint8_t* packet, uint32_t packet_len, const uint8_t* name_ptr, char_t* out_name, uint32_t max_len) {
    uint32_t out_pos       = 0;
    uint32_t bytes_read    = 0;
    boolean_t jumped       = false;
    const uint8_t* current = name_ptr;
    uint32_t jumps         = 0;

    while (true) {
        if (current < packet || current >= packet + packet_len) {
            return -1;
        }
        if (*current == 0) {
            break;
        }

        if (jumps > 100) {
            return -1; // Prevent infinite loops
        }
        if ((*current & 0xC0) == 0xC0) {
            if (!jumped) {
                bytes_read += 2;
            }
            if (current + 1 >= packet + packet_len) {
                return -1;
            }
            uint16_t offset = (BYTE_SWAP16(*(uint16_t*)(void*)current)) & 0x3FFF;
            if (offset >= packet_len) {
                return -1;
            }
            current = packet + offset;
            jumped  = true;
            jumps++;
        } else {
            uint8_t label_len = *current;
            if (!jumped) {
                bytes_read += label_len + 1;
            }
            current++;
            if (current + label_len > packet + packet_len) {
                return -1;
            }
            if (out_name) {
                if (out_pos + label_len + 1 > max_len) {
                    return -1;
                }
                memory_memcopy(current, out_name + out_pos, label_len);
                out_pos            += label_len;
                out_name[out_pos++] = '.';
            }
            current += label_len;
        }
    }
    if (!jumped) {
        bytes_read++;
    }
    if (out_name) {
        if (out_pos > 0) {
            out_name[out_pos - 1] = '\0'; // Remove trailing dot
        } else {
            if (max_len > 0) {
                out_name[0] = '\0';
            }
        }
    }
    return bytes_read;
}

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
    int32_t qname_len = parse_dns_name(dns_response_packet, recv_len, current_ptr, NULL, 0);
    if (qname_len < 0) {
        goto cleanup;
    }
    current_ptr += qname_len;

    current_ptr += sizeof(uint16_t) * 2; // Skip QTYPE and QCLASS

    result = memory_malloc_ext(network_packet_heap, sizeof(network_dns_query_result_t), 0);
    if(!result) {
        PRINTLOG(NETWORK, LOG_ERROR, "Failed to allocate memory for DNS query result");
        goto cleanup;
    }

    result->record_type  = record_type;
    result->record_count = 0;

    PRINTLOG(NETWORK, LOG_TRACE, "Parsing DNS answer section with %u answers", answer_count);

    for(uint16_t i = 0; i < answer_count; ++i) {
        // Skip name
        int32_t name_len = parse_dns_name(dns_response_packet, recv_len, current_ptr, NULL, 0);
        if (name_len < 0) {
            goto cleanup;
        }
        current_ptr += name_len;

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

        if(type == record_type && class == NETWORK_DNS_CLASS_IN) {
            if(result->record_count < NETWORK_DNS_MAX_RECORDS) {
                if (type == NETWORK_DNS_RECORD_TYPE_A) {
                    if(rdlength == sizeof(network_ipv4_address_t)) {
                        memory_memcopy(current_ptr, &result->ipv4_addresses[result->record_count], sizeof(network_ipv4_address_t));
                        result->record_count++;
                    }
                } else if (type == NETWORK_DNS_RECORD_TYPE_NS || type == NETWORK_DNS_RECORD_TYPE_CNAME || type == NETWORK_DNS_RECORD_TYPE_PTR) {
                    char_t name_buf[256];
                    if (parse_dns_name(dns_response_packet, recv_len, current_ptr, name_buf, sizeof(name_buf)) > 0) {
                        result->names[result->record_count] = strdup_at_heap(network_packet_heap, name_buf);
                        result->record_count++;
                    }
                } else if (type == NETWORK_DNS_RECORD_TYPE_TXT) {
                    if (rdlength > 0) {
                        uint8_t txt_len = *current_ptr;
                        if (txt_len <= rdlength - 1) {
                            char_t* txt = memory_malloc_ext(network_packet_heap, txt_len + 1, 0);
                            if (txt) {
                                memory_memcopy(current_ptr + 1, txt, txt_len);
                                txt[txt_len]                        = '\0';
                                result->names[result->record_count] = txt;
                                result->record_count++;
                            }
                        }
                    }
                } else if (type == NETWORK_DNS_RECORD_TYPE_MX) {
                    if (rdlength >= 2) {
                        uint16_t pref = BYTE_SWAP16(*(uint16_t*)(void*)current_ptr);
                        char_t name_buf[256];
                        if (parse_dns_name(dns_response_packet, recv_len, current_ptr + 2, name_buf, sizeof(name_buf)) > 0) {
                            result->mx_records[result->record_count].preference = pref;
                            result->mx_records[result->record_count].exchange   = strdup_at_heap(network_packet_heap, name_buf);
                            result->record_count++;
                        }
                    }
                } else if (type == NETWORK_DNS_RECORD_TYPE_SOA) {
                    char_t mname_buf[256];
                    char_t rname_buf[256];
                    int32_t mname_len = parse_dns_name(dns_response_packet, recv_len, current_ptr, mname_buf, sizeof(mname_buf));
                    if (mname_len > 0) {
                        int32_t rname_len = parse_dns_name(dns_response_packet, recv_len, current_ptr + mname_len, rname_buf, sizeof(rname_buf));
                        if (rname_len > 0 && rdlength >= mname_len + rname_len + 20) {
                            const uint8_t* nums = current_ptr + mname_len + rname_len;
                            result->soa_records[result->record_count].mname   = strdup_at_heap(network_packet_heap, mname_buf);
                            result->soa_records[result->record_count].rname   = strdup_at_heap(network_packet_heap, rname_buf);
                            result->soa_records[result->record_count].serial  = BYTE_SWAP32(*(uint32_t*)(void*)nums);
                            result->soa_records[result->record_count].refresh = BYTE_SWAP32(*(uint32_t*)(void*)(nums + 4));
                            result->soa_records[result->record_count].retry   = BYTE_SWAP32(*(uint32_t*)(void*)(nums + 8));
                            result->soa_records[result->record_count].expire  = BYTE_SWAP32(*(uint32_t*)(void*)(nums + 12));
                            result->soa_records[result->record_count].minimum = BYTE_SWAP32(*(uint32_t*)(void*)(nums + 16));
                            result->record_count++;
                        }
                    }
                }
            } else {
                PRINTLOG(NETWORK, LOG_WARNING, "Too many records in DNS response, skipping some.");
            }
        } else {
            PRINTLOG(NETWORK, LOG_WARNING, "Unsupported DNS record type %u or class %u, skipping.", type, class);
        }
        current_ptr += rdlength;
    }

    err = 0;

cleanup:
    if(err < 0 && result) {
        network_dns_query_result_free(result);
        result = NULL;
    }

    network_connection_close(connection);
    network_connection_destroy(connection);
    network_listener_destroy(listener);

    return result;
}

void network_dns_query_result_free(network_dns_query_result_t* result) {
    if(result) {
        if (result->record_type == NETWORK_DNS_RECORD_TYPE_NS ||
            result->record_type == NETWORK_DNS_RECORD_TYPE_CNAME ||
            result->record_type == NETWORK_DNS_RECORD_TYPE_PTR ||
            result->record_type == NETWORK_DNS_RECORD_TYPE_TXT) {
            for (uint32_t i = 0; i < result->record_count; i++) {
                if (result->names[i]) {
                    memory_free_ext(network_packet_heap, result->names[i]);
                }
            }
        } else if (result->record_type == NETWORK_DNS_RECORD_TYPE_MX) {
            for (uint32_t i = 0; i < result->record_count; i++) {
                if (result->mx_records[i].exchange) {
                    memory_free_ext(network_packet_heap, result->mx_records[i].exchange);
                }
            }
        } else if (result->record_type == NETWORK_DNS_RECORD_TYPE_SOA) {
            for (uint32_t i = 0; i < result->record_count; i++) {
                if (result->soa_records[i].mname) {
                    memory_free_ext(network_packet_heap, result->soa_records[i].mname);
                }
                if (result->soa_records[i].rname) {
                    memory_free_ext(network_packet_heap, result->soa_records[i].rname);
                }
            }
        }
        memory_free_ext(network_packet_heap, result);
    }
}

uint32_t network_dns_query_result_get_record_count(network_dns_query_result_t* result) {
    if(result == NULL) {
        return 0;
    }

    return result->record_count;
}

network_ipv4_address_t network_dns_query_result_get_ipv4_address(network_dns_query_result_t* result, uint32_t index) {
    if(result == NULL || index >= result->record_count || result->record_type != NETWORK_DNS_RECORD_TYPE_A) {
        return (network_ipv4_address_t){0};
    }

    return result->ipv4_addresses[index];
}

const char_t* network_dns_query_result_get_name(network_dns_query_result_t* result, uint32_t index) {
    if(result == NULL || index >= result->record_count) {
        return NULL;
    }
    if (result->record_type == NETWORK_DNS_RECORD_TYPE_NS ||
        result->record_type == NETWORK_DNS_RECORD_TYPE_CNAME ||
        result->record_type == NETWORK_DNS_RECORD_TYPE_PTR ||
        result->record_type == NETWORK_DNS_RECORD_TYPE_TXT) {
        return result->names[index];
    }
    return NULL;
}

boolean_t network_dns_query_result_get_mx_internal(network_dns_query_result_t* result, network_dns_query_mx_result_t mx_result) {
    if(result == NULL || mx_result.index >= result->record_count || result->record_type != NETWORK_DNS_RECORD_TYPE_MX) {
        return false;
    }
    if (mx_result.preference) {
        *mx_result.preference = result->mx_records[mx_result.index].preference;
    }
    if (mx_result.exchange) {
        *mx_result.exchange = result->mx_records[mx_result.index].exchange;
    }
    return true;
}

boolean_t network_dns_query_result_get_soa_internal(network_dns_query_result_t* result, network_dns_query_soa_result_t soa_result) {
    if(result == NULL || soa_result.index >= result->record_count || result->record_type != NETWORK_DNS_RECORD_TYPE_SOA) {
        return false;
    }
    if (soa_result.mname) {
        *soa_result.mname = result->soa_records[soa_result.index].mname;
    }
    if (soa_result.rname) {
        *soa_result.rname = result->soa_records[soa_result.index].rname;
    }
    if (soa_result.serial) {
        *soa_result.serial = result->soa_records[soa_result.index].serial;
    }
    if (soa_result.refresh) {
        *soa_result.refresh = result->soa_records[soa_result.index].refresh;
    }
    if (soa_result.retry) {
        *soa_result.retry = result->soa_records[soa_result.index].retry;
    }
    if (soa_result.expire) {
        *soa_result.expire = result->soa_records[soa_result.index].expire;
    }
    if (soa_result.minimum) {
        *soa_result.minimum = result->soa_records[soa_result.index].minimum;
    }
    return true;
}
