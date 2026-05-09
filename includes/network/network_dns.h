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

typedef enum network_dns_class_t : uint16_t {
    NETWORK_DNS_CLASS_IN = 1, // Internet
} network_dns_class_t;

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

uint32_t               network_dns_query_result_get_record_count(network_dns_query_result_t* result);
network_ipv4_address_t network_dns_query_result_get_ipv4_address(network_dns_query_result_t* result, uint32_t index);
const char_t*          network_dns_query_result_get_name(network_dns_query_result_t* result, uint32_t index);

typedef struct network_dns_query_mx_result_t {
    const char_t** exchange;
    uint16_t*      preference;
    uint32_t       index;
} network_dns_query_mx_result_t;

boolean_t network_dns_query_result_get_mx_internal(network_dns_query_result_t* result, network_dns_query_mx_result_t mx_result);

#define network_dns_query_result_get_mx(r, ex, ...) ({ \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Woverride-init\"") \
        int8_t __rc = network_dns_query_result_get_mx_internal(r,(network_dns_query_mx_result_t){ .index = 0, .preference = NULL, .exchange = (ex) ,##__VA_ARGS__}); \
        _Pragma("GCC diagnostic pop") \
        __rc; \
        })

typedef struct network_dns_query_soa_result_t {
    const char_t** rname;
    uint32_t*      serial;
    const char_t** mname;
    uint32_t*      refresh;
    uint32_t*      retry;
    uint32_t*      expire;
    uint32_t*      minimum;
    uint32_t       index;
} network_dns_query_soa_result_t;

boolean_t network_dns_query_result_get_soa_internal(network_dns_query_result_t* result, network_dns_query_soa_result_t soa_result);

#define network_dns_query_result_get_soa(r, rn, ...) ({ \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Woverride-init\"") \
        int8_t __rc = network_dns_query_result_get_soa_internal(r,(network_dns_query_soa_result_t){ .index = 0, .mname = NULL, .serial = NULL, .refresh = NULL, .retry = NULL, .expire = NULL, .minimum = NULL, .rname = (rn) ,##__VA_ARGS__}); \
        _Pragma("GCC diagnostic pop") \
        __rc; \
        })

#ifdef __cplusplus
}
#endif

#endif
