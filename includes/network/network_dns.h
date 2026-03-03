/**
 * @file network_dns.h
 * @brief DNS protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_DNS_H
#define ___NETWORK_DNS_H 0


#include <types.h>
#include <network/network_info.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef enum network_dns_record_type_t : uint16_t {
    NETWORK_DNS_RECORD_TYPE_A     = 1,
    NETWORK_DNS_RECORD_TYPE_NS    = 2,
    NETWORK_DNS_RECORD_TYPE_CNAME = 5,
    NETWORK_DNS_RECORD_TYPE_SOA   = 6,
    NETWORK_DNS_RECORD_TYPE_NULL  = 10,
    NETWORK_DNS_RECORD_TYPE_PTR   = 12,
    NETWORK_DNS_RECORD_TYPE_MX    = 15,
    NETWORK_DNS_RECORD_TYPE_TXT   = 16,
} network_dns_record_type_t;

typedef struct network_dns_query_result_t network_dns_query_result_t;

network_dns_query_result_t* network_dns_query(network_ipv4_address_t    dns_server,
                                              const char_t*             domain_name,
                                              network_dns_record_type_t record_type);

void network_dns_query_result_free(network_dns_query_result_t* result);

network_ipv4_address_t network_dns_query_result_get_ipv4_address(network_dns_query_result_t* result, uint32_t index);
uint32_t               network_dns_query_result_get_ipv4_address_count(network_dns_query_result_t* result);

#ifdef __cplusplus
}
#endif

#endif
