/**
 * @file tls13.h
 * @brief TLS 1.3 Protocol Definitions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___TLS13_H
#define ___TLS13_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <crypto/x509.h>

typedef struct tls13_context_t tls13_context_t;

typedef int32_t (*tls13_network_send_f)(int64_t network_client_identifier, const uint8_t* buf, int32_t len, int32_t flags);
typedef int32_t (*tls13_network_recv_f)(int64_t network_client_identifier, uint8_t* buf, int32_t len, int32_t flags);


tls13_context_t* tls13_create_server_context(const char_t*        host_port,
                                             tls13_network_send_f network_send,
                                             tls13_network_recv_f network_recv,
                                             int64_t              network_client_identifier,
                                             boolean_t            require_client_certificate,
                                             uint8_t*             psk_encryption_key,
                                             uint8_t*             psk_encryption_iv,
                                             uint8_t*             psk_aed_key);
int8_t tls13_set_ca_certificate(tls13_context_t* ctx, x509_certificate_t* ca_cert);
int8_t tls13_set_server_certificate(tls13_context_t* ctx, x509_certificate_t* server_cert,
                                    uint8_t* private_key, size_t private_key_len);
void      tls13_destroy_context(tls13_context_t* tls13_ctx);
int32_t   tls13_read(tls13_context_t* ctx, uint8_t* out_data, uint32_t max_len);
int32_t   tls13_write(tls13_context_t* ctx, const uint8_t* data, uint32_t len);
int8_t    tls13_send_close_notify(tls13_context_t* ctx);
int8_t    tls13_handle_handshake(tls13_context_t* ctx);
boolean_t tls13_has_alpn_h2(tls13_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* ___TLS13_H */
