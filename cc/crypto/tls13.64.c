/**
 * @file tls13.64.c
 * @brief TLS 1.3 Structures and Context Management
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crypto/tls13.h>
#include <crypto/sha2.h>
#include <crypto/x509.h>
#include <crypto/x25519.h>
#include <crypto/aes-gcm.h>
#include <strings.h>
#include <pipeline.h>
#include <logging.h>
#include <random.h>

MODULE("turnstone.lib.crypto.tls13");

typedef enum tls13_extension_type_t {
    TLS_EXTENSION_SNI                = 0x0000,
    TLS_EXTENSION_ALPN               = 0x0010,
    TLS_EXTENSION_SUPPORTED_VERSIONS = 0x002b,
    TLS_EXTENSION_KEY_SHARE          = 0x0033,
} tls13_extension_type_t;

typedef enum tls13_key_exchange_group_t {
    TLS_GROUP_X25519 = 0x001d,
    TLS_GROUP_SECP256R1 = 0x0017,
} tls13_key_exchange_group_t;

typedef enum tls13_hash_algorithm_t {
    TLS_HASH_NONE   = 0x00,
    TLS_HASH_SHA256 = 0x04,
    TLS_HASH_SHA384 = 0x05,
} tls13_hash_algorithm_t;

typedef enum tls13_cipher_suite_t {
    TLS_AES_128_GCM_SHA256       = 0x1301,
    TLS_AES_256_GCM_SHA384       = 0x1302,
    TLS_CHACHA20_POLY1305_SHA256 = 0x1303,
} tls13_cipher_suite_t;

struct tls13_context_t {
    const char_t*          default_host_port;
    uint16_t               version;
    boolean_t              tls13_supported;
    uint8_t                client_random[32];
    uint8_t                server_random[32];
    uint8_t                session_id_len;
    uint8_t*               session_id;
    tls13_cipher_suite_t   cipher_suite;
    char_t                 sni_hostname[256];
    boolean_t              has_alpn;
    boolean_t              alpn_h2;
    boolean_t              alpn_http11;
    boolean_t              x25519_supported;
    uint8_t                client_key_exchange_public_key[X25519_PUBLIC_KEY_RAW_LEN];
    uint8_t                server_key_exchange_private_key[X25519_PRIVATE_KEY_RAW_LEN];
    uint8_t                server_key_exchange_public_key[X25519_PUBLIC_KEY_RAW_LEN];
    tls13_hash_algorithm_t selected_hash_algorithm;
    union {
        sha256_ctx_t* sha256;
        sha384_ctx_t* sha384;
    }                   handshake_hash_ctx;
    uint8_t*            handshake_hash_value;
    x509_certificate_t* server_certificate;
    struct {
        uint8_t* data;
        uint32_t length;
    }                   server_private_key;
    x509_certificate_t* ca_certificate;
    boolean_t           require_client_certificate;
    x509_certificate_t* client_certificate;
    size_t              shared_secret_len;
    uint8_t             shared_secret[32]; // X25519 shared secret
    uint8_t             server_handshake_key[AES256_KEY_SIZE]; // max size
    uint8_t             client_handshake_key[AES256_KEY_SIZE]; // max size
    uint8_t             server_handshake_iv[12];
    uint8_t             client_handshake_iv[12];
    uint8_t             server_handshake_traffic_secret[SHA384_OUTPUT_SIZE];
    uint8_t             client_handshake_traffic_secret[SHA384_OUTPUT_SIZE];
    int32_t             handshake_hash_len;
    int32_t             handshake_key_len;
    int32_t             handshake_iv_len;
    uint8_t             server_finished_key[SHA384_OUTPUT_SIZE];
    uint8_t             client_finished_key[SHA384_OUTPUT_SIZE];
    uint8_t             master_secret[SHA384_OUTPUT_SIZE];
    uint8_t             server_application_key[AES256_KEY_SIZE];
    uint8_t             client_application_key[AES256_KEY_SIZE];
    uint8_t             server_application_iv[12];
    uint8_t             client_application_iv[12];
    int32_t             write_seq_num;
    int32_t             read_seq_num;

    // read buffer for application data
    // to handle fragmented records
    pipeline_t* read_buffer;

    // send,recv function pointers
    tls13_network_send_f network_send;
    tls13_network_recv_f network_recv;
    int64_t              network_client_identifier;
};

// Constants for the "Empty Hash" (Hash of a zero-length string)
static const uint8_t sha256_empty_hash[] = {
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
    0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
    0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
    0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

_Static_assert(sizeof(sha256_empty_hash) == SHA256_OUTPUT_SIZE, "SHA256 empty hash size mismatch");

static const uint8_t sha384_empty_hash[] = {
    0x38, 0xb0, 0x60, 0xa7, 0x51, 0xac, 0x96, 0x38,
    0x4c, 0xd9, 0x32, 0x7e, 0xb1, 0xb1, 0xe3, 0x6a,
    0x21, 0xfd, 0xb7, 0x11, 0x14, 0xbe, 0x07, 0x43,
    0x4c, 0x0c, 0xc7, 0xbf, 0x63, 0xf6, 0xe1, 0xda,
    0x27, 0x4e, 0xde, 0xbf, 0xe7, 0x6f, 0x65, 0xfb,
    0xd5, 0x1a, 0xd2, 0xf1, 0x48, 0x98, 0xb9, 0x5b
};

_Static_assert(sizeof(sha384_empty_hash) == SHA384_OUTPUT_SIZE, "SHA384 empty hash size mismatch");

static int8_t tls13_hash_final(tls13_context_t* ctx) {
    if(!ctx) {
        return -1;
    }

    if(ctx->handshake_hash_value) {
        // Already finalized
        return 0;
    }

    if (ctx->selected_hash_algorithm == TLS_HASH_SHA256) {
        if (ctx->handshake_hash_ctx.sha256) {
            ctx->handshake_hash_value = sha256_final(ctx->handshake_hash_ctx.sha256);
            ctx->handshake_hash_ctx.sha256 = NULL;
            if (ctx->handshake_hash_value == NULL) {
                return -1; // Finalization Failed
            }

            return 0;
        }
    } else if (ctx->selected_hash_algorithm == TLS_HASH_SHA384) {
        if (ctx->handshake_hash_ctx.sha384) {
            ctx->handshake_hash_value = sha384_final(ctx->handshake_hash_ctx.sha384);
            ctx->handshake_hash_ctx.sha384 = NULL;
            if (ctx->handshake_hash_value == NULL) {
                return -1; // Finalization Failed
            }

            return 0;
        }
    }

    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for handshake hash finalization");
    return -1; // Unsupported hash algorithm
}

void tls13_destroy_context(tls13_context_t* ctx) {
    if (!ctx) {
        return;
    }
    // Don't forget to free session_id and ctx when done
    if (ctx->session_id) {
        memory_free(ctx->session_id);
    }
    tls13_hash_final(ctx);
    memory_free(ctx->handshake_hash_value);

    if (ctx->server_certificate) {
        x509_certificate_free(ctx->server_certificate);
    }

    if (ctx->ca_certificate) {
        x509_certificate_free(ctx->ca_certificate);
    }

    if (ctx->client_certificate) {
        x509_certificate_free(ctx->client_certificate);
    }

    if (ctx->server_private_key.data) {
        memory_free(ctx->server_private_key.data);
    }

    if(ctx->read_buffer) {
        pipeline_destroy(ctx->read_buffer);
    }

    memory_free(ctx);
}

tls13_context_t* tls13_create_server_context(const char_t*        host_port,
                                             tls13_network_send_f network_send,
                                             tls13_network_recv_f network_recv,
                                             int64_t              network_client_identifier,
                                             boolean_t            require_client_certificate) {
    if (!host_port || !network_send || !network_recv) {
        return NULL;
    }

    tls13_context_t* ctx = (tls13_context_t*)memory_malloc(sizeof(tls13_context_t));
    if (!ctx) {
        return NULL;
    }

    ctx->default_host_port = host_port;
    ctx->network_send = network_send;
    ctx->network_recv = network_recv;
    ctx->network_client_identifier = network_client_identifier;
    ctx->require_client_certificate = require_client_certificate;

    return ctx;
}
int8_t tls13_set_ca_certificate(tls13_context_t* ctx, x509_certificate_t* ca_cert) {
    if (!ctx || !ca_cert) {
        return -1;
    }

    ctx->ca_certificate = ca_cert;

    return 0;
}

int8_t tls13_set_server_certificate(tls13_context_t* ctx, x509_certificate_t* server_cert,
                                    uint8_t* private_key, size_t private_key_len) {
    if (!ctx || !server_cert || !private_key || private_key_len == 0) {
        return -1;
    }

    ctx->server_certificate = server_cert;
    ctx->server_private_key.data = (uint8_t*)memory_malloc(private_key_len);
    if (!ctx->server_private_key.data) {
        return -1;
    }

    memory_memcopy(private_key, ctx->server_private_key.data, private_key_len);
    ctx->server_private_key.length = (uint32_t)private_key_len;
    return 0;
}

boolean_t tls13_has_alpn_h2(tls13_context_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->alpn_h2;
}

static int8_t tls13_hash_update(tls13_context_t* ctx, uint8_t* data, uint32_t len) {
    if (ctx->selected_hash_algorithm == TLS_HASH_SHA256) {
        if (!ctx->handshake_hash_ctx.sha256) {
            ctx->handshake_hash_ctx.sha256 = sha256_init();
            if (!ctx->handshake_hash_ctx.sha256) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to initialize SHA256 context");
                return -1; // Initialization Failed
            }
        }
        sha256_update(ctx->handshake_hash_ctx.sha256, data, len);
        return 0;
    } else if (ctx->selected_hash_algorithm == TLS_HASH_SHA384) {
        if (!ctx->handshake_hash_ctx.sha384) {
            ctx->handshake_hash_ctx.sha384 = sha384_init();
            if (!ctx->handshake_hash_ctx.sha384) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to initialize SHA384 context");
                return -1; // Initialization Failed
            }
        }
        sha384_update(ctx->handshake_hash_ctx.sha384, data, len);
        return 0;
    }
    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for handshake hash update");
    return -1; // Unsupported hash algorithm
}

static int8_t tls13_hash_hmac(tls13_hash_algorithm_t hash_alg,
                              uint8_t* key, uint32_t key_len,
                              uint8_t* data, uint32_t data_len,
                              uint8_t** out) {
    if (hash_alg == TLS_HASH_SHA256) {
        *out = sha256_hmac(key, key_len, data, data_len);
    } else if (hash_alg == TLS_HASH_SHA384) {
        *out = sha384_hmac(key, key_len, data, data_len);
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm in HMAC");
        return -1; // Unsupported hash algorithm
    }

    if (!*out) {
        return -1; // HMAC computation failed
    }

    return 0;
}

static int8_t tls13_hash_get_current(tls13_context_t* ctx, uint8_t* out_hash) {
    uint32_t hlen = ctx->handshake_hash_len;
    if (ctx->selected_hash_algorithm == TLS_HASH_SHA256) {
        sha256_ctx_t* tmp = sha256_clone(ctx->handshake_hash_ctx.sha256);
        uint8_t* h = sha256_final(tmp);
        memory_memcopy(h, out_hash, hlen);
        memory_free(h);
    } else if (ctx->selected_hash_algorithm == TLS_HASH_SHA384) {
        sha384_ctx_t* tmp = sha384_clone(ctx->handshake_hash_ctx.sha384);
        uint8_t* h = sha384_final(tmp);
        memory_memcopy(h, out_hash, hlen);
        memory_free(h);
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for getting current hash");
        return -1;
    }

    return 0;
}

static int8_t tls13_hash_get_empty(tls13_context_t* ctx, uint8_t* out_hash) {
    uint32_t hlen = ctx->handshake_hash_len;

    if (ctx->selected_hash_algorithm == TLS_HASH_SHA256) {
        memory_memcopy(sha256_empty_hash, out_hash, hlen);
    } else if (ctx->selected_hash_algorithm == TLS_HASH_SHA384) {
        memory_memcopy(sha384_empty_hash, out_hash, hlen);
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for getting empty hash");
        return -1;
    }

    return 0;
}

static void tls13_make_nonce(uint8_t* iv, uint64_t seq_num, uint8_t* out_nonce) {
    memory_memcopy(iv, out_nonce, 12);
    for (int i = 0; i < 8; i++) {
        // XOR the last 8 bytes of the IV with the big-endian sequence number
        out_nonce[4 + i] ^= (uint8_t)(seq_num >> (56 - (i * 8)));
    }
}

static int8_t tls13_parse_client_hello(tls13_context_t* ctx) {
    if(!ctx) {
        return -1;
    }

    uint8_t header[5];

    int32_t received = ctx->network_recv(ctx->network_client_identifier, header, 5, 0);

    if(received != 5) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to receive TLS record header");
        return -1;
    }

    if(header[0] != 0x16 || header[1] != 0x03 || (header[2] < 0x01 || header[2] > 0x04)) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Not a handshake record");

        // check for GET request (HTTP)
        if(memory_memcompare(header, "GET ", 4) == 0 // GET
           || (memory_memcompare(header, "HEAD ", 5) == 0) // HEAD
           || (memory_memcompare(header, "POST ", 5) == 0) // POST
           ) {
            PRINTLOG(CRYPTOLIB, LOG_INFO, "Received HTTP request on TLS port, sending 308 redirect to HTTPS");
            uint8_t buffer[512];
            memory_memclean(buffer, sizeof(buffer));
            memory_memcopy(header, &buffer[0], 5);
            received = ctx->network_recv(ctx->network_client_identifier, &buffer[5], 506, 0);
            if(received > 0) {
                buffer[5 + received] = '\0';
                // find Host header
                char_t default_host[256];
                memory_memclean(default_host, sizeof(default_host));
                memory_memcopy(ctx->default_host_port, default_host, strlen(ctx->default_host_port));
                char_t* host_header = strstr((char_t*)buffer, "Host: ");
                if(host_header) {
                    char_t* host_end = strstr(host_header, "\r\n");
                    if(host_end) {
                        size_t host_len = host_end - (host_header + 6);
                        if(host_len < sizeof(default_host)) {
                            memory_memcopy(host_header + 6, default_host, host_len);
                            default_host[host_len] = '\0';
                        }
                    }
                }

                char_t* response = strprintf(
                    "HTTP/1.1 308 Permanent Redirect\r\n"
                    "Location: https://%s/\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n",
                    default_host
                    );
                ctx->network_send(ctx->network_client_identifier, (uint8_t*)response, strlen(response), 0);
                memory_free(response);
                PRINTLOG(CRYPTOLIB, LOG_INFO, "Sent 308 redirect to https://%s/", default_host);

                return -2;
            }

            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to receive complete HTTP request");
            return -1;
        }

        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Not a handshake record (Type: 0x%02x)", header[0]);
        return -1;
    }

    int32_t record_len = (header[3] << 8) | header[4];

    uint8_t* buffer = memory_malloc(record_len);
    if(!buffer) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for TLS record failed");
        return -1;
    }

    received = ctx->network_recv(ctx->network_client_identifier, buffer, record_len, 0);

    if(received != record_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to receive complete TLS record");
        memory_free(buffer);
        return -1;
    }

    // 2. Move to Handshake Layer (Offset 5)
    uint8_t * handshake = buffer;
    uint8_t msg_type = handshake[0];
    if (msg_type != 0x01) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Not a Client Hello (Type: 0x%02x)", msg_type);
        memory_free(buffer);
        return -1;
    }

    // 3. Skip Handshake header (1 byte type + 3 bytes length = 4 bytes)
    // Client Version (2 bytes)
    uint16_t client_version = (handshake[4] << 8) | handshake[5];
    ctx->version = client_version;

    // 4. Client Random (32 bytes)
    uint8_t* client_random = &handshake[6];
    memory_memcopy(client_random, ctx->client_random, 32);

    // 5. Session ID (Variable length)
    uint8_t session_id_len = handshake[38];
    uint8_t* session_id = &handshake[39];
    ctx->session_id_len = session_id_len;
    if (session_id_len > 0) {
        ctx->session_id = (uint8_t*)memory_malloc(session_id_len);
        if (!ctx->session_id) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for session_id failed");
            memory_free(buffer);
            return -1;
        }
        memory_memcopy(session_id, ctx->session_id, session_id_len);
    } else {
        ctx->session_id = NULL;
    }


    // 6. Cipher Suites (Variable length)
    // The offset depends on session_id_len
    int offset = 39 + session_id_len;
    uint16_t cipher_suites_len = (handshake[offset] << 8) | handshake[offset + 1];
    uint8_t* cipher_suites = &handshake[offset + 2];

    boolean_t cipher_suit_found = false;

    for (int i = 0; i < cipher_suites_len; i += 2) {
        uint16_t suite = (cipher_suites[i] << 8) | cipher_suites[i + 1];

        if (suite == TLS_AES_128_GCM_SHA256) { // TLS_AES_128_GCM_SHA256
            ctx->cipher_suite = TLS_AES_128_GCM_SHA256;
            ctx->selected_hash_algorithm = TLS_HASH_SHA256;
            ctx->handshake_hash_len = SHA256_OUTPUT_SIZE;
            ctx->handshake_key_len = AES128_KEY_SIZE; // AES-128 key length
            ctx->handshake_iv_len = 12; // AES-GCM standard IV length
            cipher_suit_found = true;
            // You can break here or continue to see what else the client offers
        } else if (suite == TLS_AES_256_GCM_SHA384) {
            ctx->cipher_suite = TLS_AES_256_GCM_SHA384;
            ctx->selected_hash_algorithm = TLS_HASH_SHA384;
            ctx->handshake_hash_len = SHA384_OUTPUT_SIZE;
            ctx->handshake_key_len = AES256_KEY_SIZE; // AES-256 key length
            ctx->handshake_iv_len = 12; // AES-GCM standard IV length
            cipher_suit_found = true;
        }
    }

    if(!cipher_suit_found) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No supported cipher suites found");
        memory_free(buffer);
        return -1;
    }

    // Calculate offset to compression methods
    int32_t comp_offset = offset + 2 + cipher_suites_len;
    uint8_t comp_len = handshake[comp_offset];

    // Calculate offset to extensions length
    int32_t ext_len_offset = comp_offset + 1 + comp_len;
    uint16_t extensions_total_len = (handshake[ext_len_offset] << 8) | handshake[ext_len_offset + 1];

    uint8_t * ext_ptr = &handshake[ext_len_offset + 2];

    int32_t parsed_len = 0;
    while (parsed_len < extensions_total_len) {
        uint16_t ext_type = (ext_ptr[0] << 8) | ext_ptr[1];
        uint16_t ext_len = (ext_ptr[2] << 8) | ext_ptr[3];

        if (ext_type == TLS_EXTENSION_SNI) { // Server Name Indication
            // Parse SNI to extract hostname
            uint8_t * sni_data = ext_ptr + 4;
            uint16_t sni_list_len = (sni_data[0] << 8) | sni_data[1];
            uint8_t * sni_list_ptr = sni_data + 2;
            int32_t sni_parsed = 0;
            while (sni_parsed < sni_list_len) {
                uint8_t name_type = sni_list_ptr[0];
                uint16_t name_len = (sni_list_ptr[1] << 8) | sni_list_ptr[2];
                if (name_type == 0) { // hostname
                    if (name_len < sizeof(ctx->sni_hostname)) {
                        memory_memcopy(sni_list_ptr + 3, ctx->sni_hostname, name_len);
                        ctx->sni_hostname[name_len] = '\0';
                    } else {
                        PRINTLOG(CRYPTOLIB, LOG_ERROR, "SNI hostname too long: %d", name_len);
                        memory_free(buffer);
                        return -1;
                    }
                }
                sni_parsed += 3 + name_len;
                sni_list_ptr += 3 + name_len;
            }
        } else if (ext_type == TLS_EXTENSION_ALPN) { // ALPN
            ctx->has_alpn = true;
            uint8_t * alpn_data = ext_ptr + 4;
            uint16_t alpn_list_len = (alpn_data[0] << 8) | alpn_data[1];
            uint8_t * ptr = alpn_data + 2;
            uint16_t processed = 0;

            while (processed < alpn_list_len) {
                uint8_t str_len = ptr[0];
                char protocol[32]; // Protocol names are usually short

                if (str_len < sizeof(protocol)) {
                    memory_memcopy(ptr + 1, protocol, str_len);
                    protocol[str_len] = '\0';

                    if (strcmp(protocol, "h2") == 0) {
                        ctx->alpn_h2 = true;
                    } else if (strcmp(protocol, "http/1.1") == 0) {
                        ctx->alpn_http11 = true;
                    }
                }

                ptr += (1 + str_len);
                processed += (1 + str_len);
            }
        } else if (ext_type == TLS_EXTENSION_SUPPORTED_VERSIONS) { // Supported Versions
            ctx->tls13_supported = true;
        } else if (ext_type == TLS_EXTENSION_KEY_SHARE) { // Client Key Share
            uint8_t * share_ptr = ext_ptr + 4;
            uint16_t total_shares_len = (share_ptr[0] << 8) | share_ptr[1];
            uint8_t * current_share = share_ptr + 2;
            uint16_t processed = 0;

            while (processed < total_shares_len) {
                uint16_t group = (current_share[0] << 8) | current_share[1];
                uint16_t key_len = (current_share[2] << 8) | current_share[3];
                uint8_t * key_data = current_share + 4;

                if (group == TLS_GROUP_X25519) { // X25519
                    ctx->x25519_supported = true;
                    if (key_len == X25519_PUBLIC_KEY_RAW_LEN) {
                        memory_memcopy(key_data, ctx->client_key_exchange_public_key, X25519_PUBLIC_KEY_RAW_LEN);
                    } else {
                        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid X25519 public key length: %d", key_len);
                        return false;
                    }
                } else if (group == TLS_GROUP_SECP256R1) { // Secp256r1 (P-256)
                    PRINTLOG(CRYPTOLIB, LOG_ERROR, "    Found P-256 Key Share (Group 0x0017)");
                    // If you choose P-256, handle it here
                }

                int32_t jump = 4 + key_len;
                current_share += jump;
                processed += jump;
            }
        }


        ext_ptr += 4 + ext_len;
        parsed_len += 4 + ext_len;
    }

    if (parsed_len != extensions_total_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Extensions length mismatch");
        memory_free(buffer);
        return -1;
    }

    if (!ctx->tls13_supported) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client does not support TLS 1.3");
        memory_free(buffer);
        return -1;
    }

    if (!ctx->x25519_supported) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client does not support X25519 key exchange");
        memory_free(buffer);
        return -1;
    }

    if(ctx->selected_hash_algorithm == TLS_HASH_NONE) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No supported hash algorithm selected");
        memory_free(buffer);
        return -1;
    }

    if(tls13_hash_update(ctx, handshake, record_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update handshake hash");
        memory_free(buffer);
        return -1;
    }

    memory_free(buffer);

    return 0;
}

static int32_t tls13_send_server_hello(tls13_context_t* ctx) {
    uint8_t msg[256];
    int32_t p = 5; // Start after Record Header

    // Handshake Type & Placeholder for Length
    msg[p++] = 0x02;
    int32_t hs_len_ptr = p;
    p += 3;

    // Legacy Version
    msg[p++] = 0x03; msg[p++] = 0x03;

    // Server Random
    memory_memcopy(ctx->server_random, &msg[p], 32);
    p += 32;

    // Echo Session ID
    msg[p++] = ctx->session_id_len;
    if (ctx->session_id_len > 0) {
        memory_memcopy(ctx->session_id, &msg[p], ctx->session_id_len);
        p += ctx->session_id_len;
    }

    // Selected Cipher Suite
    msg[p++] = (ctx->cipher_suite >> 8) & 0xFF;
    msg[p++] = ctx->cipher_suite & 0xFF;

    // Compression Method (null)
    msg[p++] = 0x00;

    // Extensions
    int32_t ext_len_ptr = p;
    p += 2;

    // Extension: Supported Versions (0x002b)
    msg[p++] = 0x00; msg[p++] = 0x2b;
    msg[p++] = 0x00; msg[p++] = 0x02;
    msg[p++] = 0x03; msg[p++] = 0x04; // TLS 1.3

    // Extension: Key Share (0x0033)
    msg[p++] = 0x00; msg[p++] = 0x33;
    msg[p++] = 0x00; msg[p++] = 0x24; // Len 36
    msg[p++] = 0x00; msg[p++] = 0x1d; // X25519
    msg[p++] = 0x00; msg[p++] = 0x20; // Key Len 32
    memory_memcopy(ctx->server_key_exchange_public_key, &msg[p], 32);
    p += 32;

    // Fix up Lengths
    uint32_t hs_body_len = p - hs_len_ptr - 3;
    msg[hs_len_ptr] = (hs_body_len >> 16) & 0xFF;
    msg[hs_len_ptr + 1] = (hs_body_len >> 8) & 0xFF;
    msg[hs_len_ptr + 2] = hs_body_len & 0xFF;

    uint32_t ext_total_len = p - ext_len_ptr - 2;
    msg[ext_len_ptr] = (ext_total_len >> 8) & 0xFF;
    msg[ext_len_ptr + 1] = ext_total_len & 0xFF;

    // Fix Record Header
    msg[0] = 0x16;
    msg[1] = 0x03; msg[2] = 0x03;
    uint16_t rec_len = p - 5;
    msg[3] = (rec_len >> 8) & 0xFF;
    msg[4] = rec_len & 0xFF;

    // IMPORTANT: Update Handshake Hash with Handshake Data ONLY (msg + 5)
    if(tls13_hash_update(ctx, msg + 5, p - 5) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update handshake hash with Server Hello");
        return -1;
    }

    return ctx->network_send(ctx->network_client_identifier, msg, p, 0);
}

static int32_t hkdf_expand(tls13_context_t* ctx,
                           uint8_t* prk, uint8_t* info, uint16_t info_len,
                           uint8_t* out, uint16_t out_len) {

    uint16_t hash_len = ctx->handshake_hash_len;
    tls13_hash_algorithm_t hash_alg = ctx->selected_hash_algorithm;
    uint16_t n = (out_len + hash_len - 1) / hash_len; // Number of iterations

    if (n > 255) {
        return -1; // RFC limit

    }
    uint8_t T[64]; // Buffer for T(n)
    uint8_t* hash_result = NULL;
    uint16_t out_offset = 0;

    // T(0) is empty string
    // T(1) = HMAC-Hash(PRK, T(0) | info | 0x01)
    // T(2) = HMAC-Hash(PRK, T(1) | info | 0x02)

    for (uint16_t i = 1; i <= n; i++) {
        // Prepare data to hash: T(i-1) | info | counter
        uint32_t data_len = ((i == 1) ? 0 : hash_len) + info_len + 1;
        uint8_t* data = (uint8_t*)memory_malloc(data_len);
        if (!data) {
            return -1;
        }

        uint32_t p = 0;
        if (i > 1) {
            memory_memcopy(T, data, hash_len);
            p += hash_len;
        }
        memory_memcopy(info, data + p, info_len);
        p += info_len;
        data[p] = i; // Counter byte

        // Perform HMAC based on selected algorithm
        if (tls13_hash_hmac(hash_alg, prk, hash_len, data, data_len, &hash_result) != 0) {
            memory_free(data);
            return -1;
        }

        memory_free(data);

        if (!hash_result) {
            return -1;
        }

        // Copy hash result to T and to final output
        memory_memcopy(hash_result, T, hash_len);

        uint16_t copy_len = (out_len - out_offset > hash_len) ? hash_len : (out_len - out_offset);
        memory_memcopy(hash_result, out + out_offset, copy_len);

        memory_free(hash_result);
        out_offset += copy_len;
    }

    return 0;
}

static int32_t hkdf_expand_label_ext(tls13_context_t* ctx, uint8_t* secret,
                                     const char* label, uint8_t* context,
                                     uint8_t context_len, uint8_t* out, uint16_t out_len) {
    uint8_t info[255];
    int p = 0;

    // [length (2 bytes)]
    info[p++] = (out_len >> 8);
    info[p++] = (out_len & 0xFF);

    // [label_len (1 byte)] ["tls13 " + label]
    uint8_t label_full_len = 6 + strlen(label);
    info[p++] = label_full_len;
    const char tls13_label_prefix[] = "tls13 ";
    memory_memcopy(tls13_label_prefix, info + p, strlen(tls13_label_prefix)); p += strlen(tls13_label_prefix);
    memory_memcopy(label, info + p, strlen(label)); p += strlen(label);

    // [context_len (1 byte)] + [context bytes]
    info[p++] = context_len;
    if (context_len > 0 && context != NULL) {
        memory_memcopy(context, info + p, context_len);
        p += context_len;
    }

    return hkdf_expand(ctx, secret, info, p, out, out_len);
}

static int32_t hkdf_extract(tls13_context_t* ctx,
                            uint8_t* salt, uint32_t salt_len,
                            uint8_t* ikm, uint32_t ikm_len,
                            uint8_t* out_prk) {
    uint8_t* hash_result = NULL;
    uint32_t hash_len = ctx->handshake_hash_len;

    // If salt is NULL, use a string of zeros of hash_len
    uint8_t zero_salt[64] = {0};
    if (salt == NULL) {
        salt = zero_salt;
        salt_len = hash_len;
    }

    if (tls13_hash_hmac(ctx->selected_hash_algorithm,
                        salt, salt_len,
                        ikm, ikm_len,
                        &hash_result) != 0) {
        return -1;
    }

    memory_memcopy(hash_result, out_prk, hash_len);
    memory_free(hash_result);
    return 0;
}

static int8_t tls13_generate_handshake_key_and_iv(tls13_context_t* ctx) {
    uint32_t hlen = ctx->handshake_hash_len;
    uint32_t key_len = ctx->handshake_key_len;
    uint32_t iv_len = ctx->handshake_iv_len;

    uint8_t empty_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_empty(ctx, empty_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get empty hash");
        return -1;
    }

    // Snapshot of current transcript hash (ClientHello + ServerHello)
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash");
        return -1;
    }

    uint8_t zero_ikm[SHA384_OUTPUT_SIZE] = {0};
    uint8_t early_secret[SHA384_OUTPUT_SIZE], derived_early[SHA384_OUTPUT_SIZE],
            handshake_secret[SHA384_OUTPUT_SIZE], s_hs_traffic_secret[SHA384_OUTPUT_SIZE],
            c_hs_traffic_secret[SHA384_OUTPUT_SIZE];

    // 1. Early Secret
    if(hkdf_extract(ctx, NULL, 0, zero_ikm, hlen, early_secret) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive early secret");
        return -1;
    }

    // 2. Derived Secret
    if(hkdf_expand_label_ext(ctx, early_secret, "derived", (uint8_t*)empty_hash, hlen, derived_early, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive derived early secret");
        return -1;
    }

    // 3. Handshake Secret (Shared Secret is X25519 output)
    if(hkdf_extract(ctx, derived_early, hlen, ctx->shared_secret, ctx->shared_secret_len, handshake_secret) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive handshake secret");
        return -1;
    }

    // 4a. Server Handshake Traffic Secret
    if(hkdf_expand_label_ext(ctx, handshake_secret, "s hs traffic", current_hash, hlen, s_hs_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server handshake traffic secret");
        return -1;
    }
    memory_memcopy(s_hs_traffic_secret, ctx->server_handshake_traffic_secret, hlen);

    // 4b. Client Handshake Traffic Secret (Uses the same current_hash)
    if(hkdf_expand_label_ext(ctx, handshake_secret, "c hs traffic", current_hash, hlen, c_hs_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client handshake traffic secret");
        return -1;
    }
    memory_memcopy(c_hs_traffic_secret, ctx->client_handshake_traffic_secret, hlen);

    // 5. SERVER HANDSHAKE KEYS
    if(hkdf_expand_label_ext(ctx, s_hs_traffic_secret, "key", NULL, 0, ctx->server_handshake_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server handshake key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, s_hs_traffic_secret, "iv", NULL, 0, ctx->server_handshake_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server handshake IV");
        return -1;
    }

    // 6. CLIENT HANDSHAKE KEYS (Used to decrypt the Client Finished message)
    if(hkdf_expand_label_ext(ctx, c_hs_traffic_secret, "key", NULL, 0, ctx->client_handshake_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client handshake key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, c_hs_traffic_secret, "iv", NULL, 0, ctx->client_handshake_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client handshake IV");
        return -1;
    }

    // 7. Finished Keys
    if(hkdf_expand_label_ext(ctx, ctx->server_handshake_traffic_secret, "finished", NULL, 0, ctx->server_finished_key, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server finished key");
        return -1;
    }

    if(hkdf_expand_label_ext(ctx, ctx->client_handshake_traffic_secret, "finished", NULL, 0, ctx->client_finished_key, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client finished key");
        return -1;
    }

    // 8. Master Secret
    uint8_t derived_hs[SHA384_OUTPUT_SIZE];
    if(hkdf_expand_label_ext(ctx, handshake_secret, "derived", (uint8_t*)empty_hash, hlen, derived_hs, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive derived handshake secret");
        return -1;
    }

    uint8_t master_secret[SHA384_OUTPUT_SIZE];
    if(hkdf_extract(ctx, derived_hs, hlen, zero_ikm, hlen, master_secret) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive master secret");
        return -1;
    }

    memory_memcopy(master_secret, ctx->master_secret, hlen);

    return 0;
}

static int8_t tls13_send_encrypted_extensions(tls13_context_t* ctx) {
    uint8_t plaintext[256]; // Increased slightly for safety
    uint8_t ciphertext[256 + 16];

    // We start reverse filling from the end of the DATA part,
    // leaving 1 byte for the Inner Content Type (0x16)
    int32_t reverse_p = 200;
    int32_t start_pos = reverse_p;
    uint16_t ext_len = 0;

    /* --- ALPN Extension (Reverse) --- */
    if(ctx->has_alpn) {
        const char* alpn_selected = ctx->alpn_h2 ? "h2" : "http/1.1";
        int name_len = strlen(alpn_selected);

        // 1. The actual string
        reverse_p -= name_len;
        memory_memcopy(alpn_selected, &plaintext[reverse_p], name_len);

        // 2. Protocol name length (1 byte)
        plaintext[--reverse_p] = (uint8_t)name_len;

        // 3. Protocol List Length (2 bytes)
        uint16_t list_len = name_len + 1;
        plaintext[--reverse_p] = list_len & 0xff;
        plaintext[--reverse_p] = (list_len >> 8) & 0xff;

        // 4. Extension Type (0x0010) and Extension Length
        uint16_t this_ext_data_len = list_len + 2;
        plaintext[--reverse_p] = this_ext_data_len & 0xff;
        plaintext[--reverse_p] = (this_ext_data_len >> 8) & 0xff;

        plaintext[--reverse_p] = 0x10; // ALPN Type 0x0010
        plaintext[--reverse_p] = 0x00;

        ext_len = (start_pos - reverse_p);
    }

    /* --- Extensions Wrapper Length --- */
    plaintext[--reverse_p] = ext_len & 0xff;
    plaintext[--reverse_p] = (ext_len >> 8) & 0xff;

    /* --- Handshake Header --- */
    // The handshake body length is ext_len + 2 (for the extensions wrapper length bytes)
    uint32_t handshake_body_len = ext_len + 2;

    // Length (3 bytes in Handshake Header)
    plaintext[--reverse_p] = (handshake_body_len) & 0xff;
    plaintext[--reverse_p] = (handshake_body_len >> 8) & 0xff;
    plaintext[--reverse_p] = (handshake_body_len >> 16) & 0xff;

    // Type: Encrypted Extensions (0x08)
    plaintext[--reverse_p] = 0x08;

    /* --- Calculation for AEAD --- */
    // 'len' is the total bytes of the handshake message
    int32_t handshake_total_len = (start_pos - reverse_p);
    uint8_t* handshake_start = &plaintext[reverse_p];

    // Update Transcript Hash
    tls13_hash_update(ctx, handshake_start, handshake_total_len);

    // --- Content Type (Inner) ---
    // The 0x16 byte MUST immediately follow the handshake data
    plaintext[start_pos] = 0x16;
    int32_t aead_plaintext_len = handshake_total_len + 1;

    /* --- Nonce and AAD --- */
    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_handshake_iv, ctx->write_seq_num, nonce);

    uint16_t encrypted_record_len = aead_plaintext_len + 16;
    uint8_t aad[5] = { 0x17, 0x03, 0x03, (encrypted_record_len >> 8), (encrypted_record_len & 0xff) };

    /* --- Encrypt --- */
    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        ciphertext,
        handshake_start, aead_plaintext_len, // Encrypt Handshake + 0x16
        ctx->server_handshake_key, ctx->handshake_key_len,
        nonce, 12, aad, 5,
        ciphertext + aead_plaintext_len, 16);

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS Encryption failed");
        return -1;
    }

    /* --- Send record --- */
    if (ctx->network_send(ctx->network_client_identifier, aad, 5, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send TLS record header");
        return -1;
    }

    if (ctx->network_send(ctx->network_client_identifier, ciphertext, encrypted_record_len, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send encrypted extensions");
        return -1;
    }

    ctx->write_seq_num++;
    return 0;
}

static int8_t tls13_send_certificate_request(tls13_context_t* ctx) {
    uint8_t plaintext[128];
    uint8_t ciphertext[128 + 16];

    int32_t reverse_p = 100; // Start offset
    int32_t start_pos = reverse_p;

    // --- Extensions: signature_algorithms (Reverse) ---
    // 1. The Algorithm ID: Ed25519 (0x0807)
    plaintext[--reverse_p] = 0x07;
    plaintext[--reverse_p] = 0x08;

    // 2. The Algorithm List Length (2 bytes: 0x0002)
    plaintext[--reverse_p] = 0x02;
    plaintext[--reverse_p] = 0x00;

    // 3. Extension Data Length (same as list length + 2: 0x0004)
    // Actually, it's just the list length here: 0x0004
    uint16_t ext_data_len = 2 + 2; // list_len_field + list_data
    plaintext[--reverse_p] = (uint8_t)(ext_data_len & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((ext_data_len >> 8) & 0xFF);

    // 4. Extension Type: signature_algorithms (0x000d)
    plaintext[--reverse_p] = 0x0d;
    plaintext[--reverse_p] = 0x00;

    // --- Handshake Body (Reverse) ---
    uint16_t extensions_vec_len = 2 + 2 + ext_data_len; // Type + Len + Data

    // 5. Extensions Vector Length (2 bytes)
    plaintext[--reverse_p] = (uint8_t)(extensions_vec_len & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((extensions_vec_len >> 8) & 0xFF);

    // 6. Request Context Length (0x00 for handshake)
    plaintext[--reverse_p] = 0x00;

    // --- Handshake Header (Reverse) ---
    uint32_t handshake_body_len = 1 + 2 + extensions_vec_len; // context_len + ext_vec_len + ext_data

    // 7. Handshake Length (3 bytes: Big Endian)
    plaintext[--reverse_p] = (uint8_t)(handshake_body_len & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((handshake_body_len >> 8) & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((handshake_body_len >> 16) & 0xFF);

    // 8. Handshake Type: CertificateRequest (0x0d)
    plaintext[--reverse_p] = 0x0d;

    /* --- Handshake calculation and Encryption --- */
    int32_t handshake_total_len = (start_pos - reverse_p);
    uint8_t* handshake_start = &plaintext[reverse_p];

    // Update Transcript
    tls13_hash_update(ctx, handshake_start, handshake_total_len);

    // Append Inner Content Type
    plaintext[start_pos] = 0x16;
    int32_t aead_plaintext_len = handshake_total_len + 1;

    /* --- Nonce and AAD --- */
    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_handshake_iv, ctx->write_seq_num, nonce);

    uint16_t encrypted_record_len = aead_plaintext_len + 16;
    uint8_t aad[5] = { 0x17, 0x03, 0x03, (encrypted_record_len >> 8), (encrypted_record_len & 0xff) };

    /* --- Encrypt --- */
    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        ciphertext,
        handshake_start, aead_plaintext_len, // Encrypt Handshake + 0x16
        ctx->server_handshake_key, ctx->handshake_key_len,
        nonce, 12, aad, 5,
        ciphertext + aead_plaintext_len, 16);

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS Encryption failed");
        return -1;
    }

    /* --- Send record --- */
    if (ctx->network_send(ctx->network_client_identifier, aad, 5, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send TLS record header");
        return -1;
    }

    if (ctx->network_send(ctx->network_client_identifier, ciphertext, encrypted_record_len, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send encrypted extensions");
        return -1;
    }

    ctx->write_seq_num++;

    return 0;
}

static int8_t tls13_send_certificate(tls13_context_t* ctx) {
    // We need the raw DER for both
    size_t server_der_len = 0;
    uint8_t* server_der = x509_certificate_get_der(ctx->server_certificate, &server_der_len);

    size_t ca_der_len = 0;
    uint8_t* ca_der = x509_certificate_get_der(ctx->ca_certificate, &ca_der_len);

    if (!server_der || !ca_der) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get DER encoding of certificates server %d ca %d",
                 server_der ? 1 : 0, ca_der ? 1 : 0);
        memory_free(server_der);
        memory_free(ca_der);
        return -1;
    }

    size_t total_cert_len = 3 + server_der_len + 2 + 3 + ca_der_len + 2;
    size_t estimated_hs_len = 1 + 3 + 1 + 3 + total_cert_len;
    estimated_hs_len += 4096 - (estimated_hs_len % 4096); // Padding for safety

    uint8_t* plaintext = (uint8_t*)memory_malloc(estimated_hs_len);
    uint8_t* ciphertext = (uint8_t*)memory_malloc(estimated_hs_len + 16);

    if (!plaintext || !ciphertext) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Certificate message");
        memory_free(plaintext);
        memory_free(ciphertext);
        memory_free(server_der);
        memory_free(ca_der);
        return -1;
    }

    int32_t p = 0;

    /* --- Build Handshake Body --- */
    plaintext[p++] = 0x0b; // Type: Certificate
    int32_t hs_len_ptr = p; p += 3; // Placeholder for total HS length

    plaintext[p++] = 0x00; // Certificate Request Context (empty)

    // Certificate List Length
    int32_t list_len_ptr = p; p += 3;

    // --- Entry 1: Server Certificate ---
    plaintext[p++] = (server_der_len >> 16) & 0xFF;
    plaintext[p++] = (server_der_len >> 8) & 0xFF;
    plaintext[p++] = (server_der_len & 0xFF);
    memory_memcopy(server_der, &plaintext[p], server_der_len);
    p += server_der_len;
    plaintext[p++] = 0x00; plaintext[p++] = 0x00; // Extensions Len (0)

    memory_free(server_der);

    // --- Entry 2: CA Certificate ---
    plaintext[p++] = (ca_der_len >> 16) & 0xFF;
    plaintext[p++] = (ca_der_len >> 8) & 0xFF;
    plaintext[p++] = (ca_der_len & 0xFF);
    memory_memcopy(ca_der, &plaintext[p], ca_der_len);
    p += ca_der_len;
    plaintext[p++] = 0x00; plaintext[p++] = 0x00; // Extensions Len (0)

    memory_free(ca_der);

    /* --- Fix Lengths --- */
    uint32_t total_list_len = p - list_len_ptr - 3;
    plaintext[list_len_ptr] = (total_list_len >> 16) & 0xFF;
    plaintext[list_len_ptr + 1] = (total_list_len >> 8) & 0xFF;
    plaintext[list_len_ptr + 2] = (total_list_len & 0xFF);

    uint32_t total_hs_len = p - hs_len_ptr - 3;
    plaintext[hs_len_ptr] = (total_hs_len >> 16) & 0xFF;
    plaintext[hs_len_ptr + 1] = (total_hs_len >> 8) & 0xFF;
    plaintext[hs_len_ptr + 2] = (total_hs_len & 0xFF);

    /* --- Transcript Hash Update --- */
    tls13_hash_update(ctx, plaintext, p);

    /* --- Encrypt and Send --- */
    plaintext[p++] = 0x16; // Inner Type: Handshake

    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_handshake_iv, ctx->write_seq_num, nonce);

    size_t key_len = ctx->handshake_key_len;
    uint16_t encrypted_len = p + 16;

    uint8_t aad[5] = { 0x17, 0x03, 0x03, (encrypted_len >> 8), (encrypted_len & 0xFF) };

    aes_gcm_encrypt_with_aad_with_tag(ciphertext, plaintext, p,
                                      ctx->server_handshake_key, key_len,
                                      nonce, 12, aad, 5, ciphertext + p, 16);

    ctx->network_send(ctx->network_client_identifier, aad, 5, 0);
    ctx->network_send(ctx->network_client_identifier, ciphertext, encrypted_len, 0);

    ctx->write_seq_num++;

    memory_free(plaintext);
    memory_free(ciphertext);
    return 0;
}

static int8_t tls13_send_certificate_verify(tls13_context_t* ctx) {
    const size_t space_count = 64;
    const char_t* sign_string = "TLS 1.3, server CertificateVerify";
    uint8_t sign_buffer[space_count + strlen(sign_string) + 1 + SHA384_OUTPUT_SIZE];
    // 1. Construct the buffer to be signed
    memory_memset(sign_buffer, 0x20, space_count); // 64 spaces
    memory_memcopy(sign_string, sign_buffer + space_count, strlen(sign_string));
    sign_buffer[64 + strlen(sign_string)] = 0x00; // Null terminator

    // Get the current snapshot of the handshake hash
    uint32_t hlen = ctx->handshake_hash_len;

    // Note: This must include the Certificate message bytes!
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for CertificateVerify");
        return -1;
    }

    memory_memcopy(current_hash, sign_buffer + space_count + strlen(sign_string) + 1, hlen);

    // 2. Sign the buffer using Ed25519
    uint8_t signature[ED25519_SIGNATURE_LEN];
    if (ed25519_sign(signature, sign_buffer, space_count + strlen(sign_string) + 1 + hlen,
                     ctx->server_private_key.data) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Ed25519 signing failed");
        return -1;
    }

    uint8_t plaintext[256];
    int32_t p = 0;

    /* --- Build Handshake Message --- */
    plaintext[p++] = 0x0f; // Type: Certificate Verify
    // Length: 2 (Algorithm) + 2 (Sig Len) + 64 (Signature) = 68 bytes
    plaintext[p++] = 0x00; plaintext[p++] = 0x00; plaintext[p++] = 2 + 2 + ED25519_SIGNATURE_LEN;

    // Algorithm: Ed25519 is 0x0807
    plaintext[p++] = 0x08; plaintext[p++] = 0x07;

    // Signature Length: 64 bytes
    plaintext[p++] = 0x00; plaintext[p++] = ED25519_SIGNATURE_LEN;
    memory_memcopy(signature, &plaintext[p], ED25519_SIGNATURE_LEN);
    p += ED25519_SIGNATURE_LEN;

    /* --- Finalize Transcript and Send --- */
    // 1. Update hash with the Certificate Verify message (p bytes)
    tls13_hash_update(ctx, plaintext, p);

    // 2. Wrap in encrypted record
    plaintext[p++] = 0x16; // Inner Type: Handshake

    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_handshake_iv, ctx->write_seq_num, nonce);

    size_t key_len = ctx->handshake_key_len;
    uint16_t encrypted_len = p + 16;
    uint8_t aad[5] = { 0x17, 0x03, 0x03, (encrypted_len >> 8), (encrypted_len & 0xFF) };

    uint8_t ciphertext[256 + 16];
    aes_gcm_encrypt_with_aad_with_tag(ciphertext, plaintext, p,
                                      ctx->server_handshake_key, key_len,
                                      nonce, 12, aad, 5, ciphertext + p, 16);

    ctx->network_send(ctx->network_client_identifier, aad, 5, 0);
    ctx->network_send(ctx->network_client_identifier, ciphertext, encrypted_len, 0);

    ctx->write_seq_num++;
    return 0;
}

static int8_t tls13_send_finished(tls13_context_t* ctx) {
    uint8_t verify_data[SHA384_OUTPUT_SIZE];
    uint8_t hlen = ctx->handshake_hash_len;
    size_t key_len = ctx->handshake_key_len;

    // Get the current Transcript Hash (includes ClientHello...CertificateVerify)
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for Server Finished");
        return -1;
    }

    // Compute HMAC(finished_key, current_hash)
    uint8_t* hmac_out;
    if(tls13_hash_hmac(ctx->selected_hash_algorithm,
                       ctx->server_finished_key, hlen,
                       current_hash, hlen,
                       &hmac_out) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute HMAC for Server Finished");
        return -1;
    }
    memory_memcopy(hmac_out, verify_data, hlen);
    memory_free(hmac_out);

    /* --- Build Handshake Message --- */
    uint8_t plaintext[128];
    uint8_t ciphertext[128 + 16];
    int32_t p = 0;

    plaintext[p++] = 0x14; // Type: Finished
    plaintext[p++] = 0x00; plaintext[p++] = 0x00; plaintext[p++] = hlen; // Length
    memory_memcopy(verify_data, &plaintext[p], hlen);
    p += hlen;

    /* --- Update Hash (The Finished message IS hashed for the next steps) --- */
    tls13_hash_update(ctx, plaintext, p);

    /* --- Wrap in Encrypted Record --- */
    plaintext[p++] = 0x16; // Inner Type: Handshake

    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_handshake_iv, ctx->write_seq_num, nonce);

    uint16_t encrypted_len = p + 16;
    uint8_t aad[5] = { 0x17, 0x03, 0x03, (encrypted_len >> 8), (encrypted_len & 0xFF) };

    aes_gcm_encrypt_with_aad_with_tag(ciphertext, plaintext, p,
                                      ctx->server_handshake_key, key_len,
                                      nonce, 12, aad, 5, ciphertext + p, 16);

    ctx->network_send(ctx->network_client_identifier, aad, 5, 0);
    ctx->network_send(ctx->network_client_identifier, ciphertext, encrypted_len, 0);

    ctx->write_seq_num++;

    return 0;
}

static int8_t tls13_process_client_certificate(tls13_context_t* ctx,
                                               uint8_t*         received_verify_data,
                                               uint16_t         verify_data_len) {

    uint8_t* p = received_verify_data;
    uint8_t* end = p + verify_data_len;

    // 1. request_context (1 byte length prefix)
    if (p + 1 > end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
        return -1;
    }

    uint8_t context_len = *p++;
    p += context_len; // Skip context

    // 2. certificate_list (3 bytes length prefix)
    if (p + 3 > end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
        return -1;
    }

    uint32_t cert_list_len = (p[0] << 16) | (p[1] << 8) | p[2];
    p += 3;

    if (cert_list_len == 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Empty client certificate list - Auth Failed");
        return -1;
    }

    x509_certificate_t* client_certificate = NULL;
    boolean_t client_verified = false;
    size_t ca_public_key_len = 0;
    uint8_t* ca_public_key = x509_certificate_get_public_key_data(ctx->ca_certificate, &ca_public_key_len);
    if (!ca_public_key) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get CA public key");
        return -1;
    }

    while(cert_list_len > 0) {
        // 3. Parse each certificate entry
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Parsing client certificate entry");
        // cert_data (3 bytes length prefix)
        if (p + 3 > end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
            memory_free(ca_public_key);
            return -1;
        }

        uint32_t cert_data_len = (p[0] << 16) | (p[1] << 8) | p[2];
        p += 3;

        if (p + cert_data_len > end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
            memory_free(ca_public_key);
            return -1;
        }

        uint8_t* cert_der = p;
        p += cert_data_len;

        x509_certificate_t* client_cert = x509_certificate_from_der(cert_der, cert_data_len);
        if (!client_cert) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to parse client certificate");
            memory_free(ca_public_key);
            return -1;
        } else {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client Certificate parsed successfully");
        }

        if(x509_certificate_verify_signature_with_rebuild(client_cert, ca_public_key, ca_public_key_len, false) == 0) {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client certificate signature verified successfully");
            client_verified = true;
            if(client_certificate) {
                x509_certificate_free(client_certificate);
            } else {
                client_certificate = client_cert; // Keep the first valid certificate
            }
            break; // Stop after first valid certificate
        } else {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client certificate signature verification failed");
        }

        // Free the certificate if not kept
        if(client_cert != client_certificate) {
            x509_certificate_free(client_cert);
        }

        // 4. Certificate Extensions (2 bytes length prefix)
        if (p + 2 > end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
            return -1;
        }

        uint16_t ext_len = (p[0] << 8) | p[1];
        p += 2 + ext_len;


        cert_list_len -= (3 + cert_data_len + 2 + ext_len);
    }

    memory_free(ca_public_key);

    // Safety check: ensure we didn't overrun the handshake message
    if (p > end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
        return -1;
    }

    if (client_verified) {
        ctx->client_certificate = client_certificate;
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client Certificate processed and verified successfully");
        return 0;
    }

    return -1;
}

static int8_t tls13_process_client_certificate_verify(tls13_context_t* ctx,
                                                      uint8_t*         received_verify_data,
                                                      uint16_t         verify_data_len) {
    if (verify_data_len < 4) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed CertificateVerify message");
        return -1;
    }

    if(!ctx->client_certificate) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No client certificate available for CertificateVerify");
        return -1;
    }

    uint16_t algorithm = (received_verify_data[0] << 8) | received_verify_data[1];;
    if (algorithm != 0x0807) { // Ed25519
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported signature algorithm in CertificateVerify: 0x%04x", algorithm);
        return -1;
    }

    uint16_t sig_len = (received_verify_data[2] << 8) | received_verify_data[3];
    if (sig_len != ED25519_SIGNATURE_LEN || (4 + sig_len) != verify_data_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid signature length in CertificateVerify");
        return -1;
    }

    const size_t space_count = 64;
    const char_t* verify_string = "TLS 1.3, client CertificateVerify";
    uint8_t verify_buffer[space_count + strlen(verify_string) + 1 + SHA384_OUTPUT_SIZE];
    memory_memset(verify_buffer, 0x20, space_count); // 64 spaces
    memory_memcopy(verify_string, verify_buffer + space_count, strlen(verify_string));
    verify_buffer[64 + strlen(verify_string)] = 0x00; // Null terminator

    // Get the current snapshot of the handshake hash
    uint32_t hlen = ctx->handshake_hash_len;

    // Note: This must include the Certificate message bytes!
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for CertificateVerify");
        return -1;
    }

    memory_memcopy(current_hash, verify_buffer + space_count + strlen(verify_string) + 1, hlen);

    uint8_t signature[ED25519_SIGNATURE_LEN];
    memory_memcopy(&received_verify_data[4], signature, ED25519_SIGNATURE_LEN);

    size_t total_len = space_count + strlen(verify_string) + 1 + hlen;

    size_t public_key_len = 0;
    uint8_t* public_key = x509_certificate_get_public_key_data(ctx->client_certificate, &public_key_len);
    if (!public_key) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get public key from client certificate");
        return -1;
    }

    if (ed25519_verify(signature, verify_buffer, total_len, public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client CertificateVerify signature verification failed");
        memory_free(public_key);
        return -1;
    }

    memory_free(public_key);

    PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client CertificateVerify processed successfully");

    return 0;
}

static int8_t tls13_process_client_finished(tls13_context_t* ctx,
                                            uint8_t*         received_verify_data,
                                            uint16_t         verify_data_len) {
    uint8_t hlen = ctx->handshake_hash_len;

    if (verify_data_len != hlen) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client Finished: Invalid verify_data length");
        return -1;
    }

    // 1. Get the Transcript Hash
    // This snapshot must include everything up to your Server Finished
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for Client Finished");
        return -1;
    }

    // 2. Compute the expected HMAC
    uint8_t expected_verify_data[SHA384_OUTPUT_SIZE];
    uint8_t* hmac_out;
    if(tls13_hash_hmac(ctx->selected_hash_algorithm,
                       ctx->client_finished_key, hlen,
                       current_hash, hlen,
                       &hmac_out) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute expected HMAC for Client Finished");
        return -1;
    }
    memory_memcopy(hmac_out, expected_verify_data, hlen);
    memory_free(hmac_out);

    // 3. Constant-time comparison (if available in your library)
    if (memory_memcompare(expected_verify_data, received_verify_data, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client Finished: HMAC verification failed!");
        return -1;
    }

    return 0;
}

static int8_t tls13_handle_client_handshake_read(tls13_context_t* ctx) {
    boolean_t done = false;

    while(!done) {
        uint8_t header[5];
        int32_t ret = ctx->network_recv(ctx->network_client_identifier, header, 5, 0);
        if (ret != 5) {
            return -1;
        }

        // Handle Dummy ChangeCipherSpec (CCS is Type 0x14)
        if (header[0] == 0x14) {
            uint16_t ccs_len = (header[3] << 8) | header[4];
            uint8_t dummy[16];
            ctx->network_recv(ctx->network_client_identifier, dummy, ccs_len, 0);
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Received Dummy ChangeCipherSpec before Client Finished");
            continue;
        }

        if (header[0] != 0x17) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected encrypted record (0x17), got 0x%02x", header[0]);
            return -1;
        }

        int32_t record_len = (header[3] << 8) | header[4];

        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Reading Client Handshake record of length %d", record_len);

        uint8_t* buffer = (uint8_t*)memory_malloc(record_len);

        if (!buffer) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for record buffer");
            return -1;
        }

        ret = ctx->network_recv(ctx->network_client_identifier, buffer, record_len, 0);
        if (ret != record_len) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read full Client Message");
            memory_free(buffer);
            return -1;
        }

        uint32_t ciphertext_len = record_len - 16;
        uint8_t* tag = buffer + ciphertext_len;

        uint8_t nonce[12];
        tls13_make_nonce(ctx->client_handshake_iv, ctx->read_seq_num, nonce);

        uint8_t* plaintext = (uint8_t*)memory_malloc(ciphertext_len);
        if (!plaintext) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Client Message plaintext");
            memory_free(buffer);
            return -1;
        }

        // FIX: Pass 'header' as AAD, not 'buffer'
        int32_t status = aes_gcm_decrypt_with_aad_with_tag(
            plaintext,
            buffer, ciphertext_len,
            ctx->client_handshake_key, ctx->handshake_key_len,
            nonce, 12,
            header, 5, // AAD is the header!
            tag, 16
            );

        memory_free(buffer);

        if (status != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Decryption Failed! Nonce/Key/AAD mismatch.");
            memory_free(plaintext);
            return -1;
        }

        // Identify real content type (ignores potential padding)
        int32_t type_pos = ciphertext_len - 1;
        while (type_pos > 0 && plaintext[type_pos] == 0x00) {type_pos--;}
        uint8_t inner_type = plaintext[type_pos];

        if (inner_type == 0x15) { // ALERT
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Client sent Alert: %d %d", plaintext[0], plaintext[1]);
            memory_free(plaintext);
            return -1;
        }

        if(inner_type != 0x16) { // Handshake
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unexpected Inner Content Type 0x%02x received", inner_type);
            memory_free(plaintext);
            return -1;
        }

        uint32_t verify_data_len = (plaintext[1] << 16) | (plaintext[2] << 8) | plaintext[3];

        if(plaintext[0] == 0x0B) { // Certificate
            if(!ctx->require_client_certificate) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Received unexpected Client Certificate without request");
                memory_free(plaintext);
                return -1;
            }

            if(tls13_process_client_certificate(ctx, &plaintext[4], verify_data_len) != 0) {
                memory_free(plaintext);
                return -1;
            }
            ctx->read_seq_num++;
        } else if(plaintext[0] == 0x0F) { // Certificate Verify
            if(!ctx->require_client_certificate) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Received unexpected Client CertificateVerify without request");
                memory_free(plaintext);
                return -1;
            }

            if(tls13_process_client_certificate_verify(ctx, &plaintext[4], verify_data_len) != 0) {
                memory_free(plaintext);
                return -1;
            }
            ctx->read_seq_num++;
        } else if (plaintext[0] == 0x14) { // Finished
            if (tls13_process_client_finished(ctx, &plaintext[4], verify_data_len) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client Finished processing failed");
                memory_free(plaintext);
                return -1;
            }

            // Sequence numbers are reset in the generate_application_keys step
            ctx->read_seq_num = 0;
            done = true; // Finished processed
        } else {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unexpected Handshake message type 0x%02x received", plaintext[0]);
            memory_free(plaintext);
            return -1;
        }

        // Update transcript with decrypted Handshake message
        if(tls13_hash_update(ctx, plaintext, 4 + verify_data_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update transcript hash with Client Handshake message");
            memory_free(plaintext);
            return -1;
        }

        memory_free(plaintext);
    }

    PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Completed processing Client Handshake messages");

    return 0;
}

static int8_t tls13_generate_application_keys(tls13_context_t* ctx) {
    uint32_t hlen = ctx->handshake_hash_len;
    uint32_t key_len = ctx->handshake_key_len;
    uint32_t iv_len = ctx->handshake_iv_len;

    // 1. Get current transcript hash (Includes EVERYTHING up to Client Finished)
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for application key generation");
        return -1;
    }

    // 2. Derive Application Traffic Secrets (using transcript hash)
    uint8_t s_ap_traffic_secret[64];
    uint8_t c_ap_traffic_secret[64];

    if(hkdf_expand_label_ext(ctx, ctx->master_secret, "s ap traffic", current_hash, hlen, s_ap_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server application traffic secret");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, ctx->master_secret, "c ap traffic", current_hash, hlen, c_ap_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client application traffic secret");
        return -1;
    }

    // 3. Generate the actual Application Keys/IVs (Context length must be 0)
    if(hkdf_expand_label_ext(ctx, s_ap_traffic_secret, "key", NULL, 0, ctx->server_application_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server application key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, s_ap_traffic_secret, "iv", NULL, 0, ctx->server_application_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server application IV");
        return -1;
    }

    if(hkdf_expand_label_ext(ctx, c_ap_traffic_secret, "key", NULL, 0, ctx->client_application_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client application key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, c_ap_traffic_secret, "iv", NULL, 0, ctx->client_application_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client application IV");
        return -1;
    }

    // 5. CRITICAL: Reset sequence numbers for Application Phase
    ctx->write_seq_num = 0;
    // ctx->read_seq_num = 0; // it should set to 0 after processing Client Finished

    return 0;
}

static int32_t tls13_write_chunk(tls13_context_t* ctx, const uint8_t* data, uint32_t len) {
    // 16384 is the max TLS record size
    uint32_t p_len = len + 1;
    uint8_t plaintext[p_len];
    uint8_t ciphertext[p_len + 16];

    // Copy payload and append Inner Content Type (0x17 for Application Data)
    memory_memcopy(data, plaintext, len);
    plaintext[len] = 0x17;

    // Prepare Nonce (IV ^ write_seq_num)
    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_application_iv, ctx->write_seq_num, nonce);

    // Prepare AAD (5-byte Record Header)
    uint16_t encrypted_record_len = p_len + 16;
    uint8_t aad[5] = { 0x17, 0x03, 0x03, (encrypted_record_len >> 8), (encrypted_record_len & 0xFF) };

    // Encrypt
    size_t key_len = ctx->handshake_key_len;
    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        ciphertext, plaintext, p_len,
        ctx->server_application_key, key_len,
        nonce, 12, aad, 5, ciphertext + p_len, 16
        );

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Encryption Failed! Nonce/Key/AAD mismatch.");
        return -1;
    }

    // Send Header + Ciphertext
    if (ctx->network_send(ctx->network_client_identifier, aad, 5, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send TLS record header");
        return -1;
    }
    if (ctx->network_send(ctx->network_client_identifier, ciphertext, encrypted_record_len, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send TLS record ciphertext");
        return -1;
    }

    ctx->write_seq_num++;
    return len;
}

int32_t tls13_write(tls13_context_t* ctx, const uint8_t* data, uint32_t len) {
    if(!ctx || !data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS context or data buffer is NULL");
        return -1;
    }

    if(len == 0) {
        return 0;
    }

    int64_t remaining = len;
    int32_t total_sent = 0;

    while(remaining > 0) {
        uint32_t chunk_size = remaining > 16384 ? 16384 : (uint32_t)remaining;
        int32_t sent = tls13_write_chunk(ctx, data + total_sent, chunk_size);
        if(sent < 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to write TLS chunk");
            return -1;
        }
        total_sent += sent;
        remaining -= sent;
    }

    return len;
}

int32_t tls13_read(tls13_context_t* ctx, uint8_t* out_data, uint32_t max_len) {
    if(!ctx || !out_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS context or output buffer is NULL");
        return -1;
    }

    if(max_len == 0) {
        return 0;
    }

    if(!ctx->read_buffer) {
        ctx->read_buffer = pipeline_create(16384); // 16KB buffer
        if(!ctx->read_buffer) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create read buffer pipeline");
            return -1;
        }
    }

    int32_t remaining = max_len;

    int32_t total_read = pipeline_read(ctx->read_buffer, remaining, out_data);
    remaining -= total_read;

    if(remaining == 0) {
        return total_read;
    }

    uint8_t header[5];

    if (ctx->network_recv(ctx->network_client_identifier, header, 5, 0) <= 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read TLS record header");
        return -1;
    }

    // Handle legacy ChangeCipherSpec if it pops up mid-stream (unlikely but possible)
    if (header[0] == 0x14) {
        uint16_t ccs_len = (header[3] << 8) | header[4];
        uint8_t dummy[16];
        ctx->network_recv(ctx->network_client_identifier, dummy, ccs_len, 0);
        return tls13_read(ctx, out_data, max_len);
    }

    if (header[0] != 0x17) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected encrypted record (0x17), got 0x%02x", header[0]);
        return -1;
    }

    uint16_t record_len = (header[3] << 8) | header[4];
    uint8_t* buffer = memory_malloc(record_len);
    if (ctx->network_recv(ctx->network_client_identifier, buffer, record_len, 0) <= 0) {
        memory_free(buffer);
        return -1;
    }

    // Decrypt using Application Keys
    uint8_t nonce[12];
    tls13_make_nonce(ctx->client_application_iv, ctx->read_seq_num, nonce);

    uint8_t* plaintext = memory_malloc(record_len);
    uint32_t ciphertext_len = record_len - 16;

    int32_t status = aes_gcm_decrypt_with_aad_with_tag(
        plaintext, buffer, ciphertext_len,
        ctx->client_application_key, ctx->handshake_key_len,
        nonce, 12, header, 5, buffer + ciphertext_len, 16
        );

    memory_free(buffer);
    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Decryption Failed! Nonce/Key/AAD mismatch.");
        memory_free(plaintext);
        return -1;
    }

    // --- STEP 1: Handle Padding ---
    // The Inner Content Type is the last NON-ZERO byte.
    int32_t type_pos = ciphertext_len - 1;
    while (type_pos > 0 && plaintext[type_pos] == 0x00) {
        type_pos--;
    }
    uint8_t inner_type = plaintext[type_pos];
    int32_t real_data_len = type_pos; // Data ends before the type byte

    // --- STEP 2: Handle Inner Types ---
    if (inner_type == 0x15) { // ALERT
        if (plaintext[0] == 0x01 && plaintext[1] == 0x00) {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Received Close Notify.");
        }
        memory_free(plaintext);
        return -1;
    }

    if (inner_type == 0x16) { // POST-HANDSHAKE (e.g. KeyUpdate or NewSessionTicket)
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Received post-handshake message type 0x%02x", plaintext[0]);
        // Note: KeyUpdate is 0x18. If you don't handle it,
        // the next record will fail decryption because keys didn't rotate!
        memory_free(plaintext);
        return tls13_read(ctx, out_data, max_len);
    }

    if (inner_type != 0x17) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unexpected inner type 0x%02x", inner_type);
        memory_free(plaintext);
        return -1;
    }

    // Success: Copy application data
    int32_t to_copy = (real_data_len < remaining) ? real_data_len : remaining;
    memory_memcopy(plaintext, out_data, to_copy);

    // Buffer any excess data for future reads
    if (real_data_len > to_copy) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Buffering %d excess bytes for future reads", real_data_len - to_copy);
        pipeline_write(ctx->read_buffer, real_data_len - to_copy, &plaintext[to_copy]);
    }

    ctx->read_seq_num++;
    memory_free(plaintext);
    return to_copy + total_read;
}

int8_t tls13_send_close_notify(tls13_context_t* ctx) {
    uint8_t plaintext[3] = { 0x01, 0x00, 0x15 }; // Warning, CloseNotify, InnerType: Alert
    uint8_t ciphertext[3 + 16];
    uint8_t nonce[12];

    tls13_make_nonce(ctx->server_application_iv, ctx->write_seq_num, nonce);

    uint16_t encrypted_len = 3 + 16;
    uint8_t aad[5] = { 0x17, 0x03, 0x03, (encrypted_len >> 8), (encrypted_len & 0xFF) };

    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        ciphertext, plaintext, 3,
        ctx->server_application_key, ctx->handshake_key_len,
        nonce, 12, aad, 5, ciphertext + 3, 16
        );

    if (status == 0) {
        ctx->network_send(ctx->network_client_identifier, aad, 5, 0);
        ctx->network_send(ctx->network_client_identifier, ciphertext, encrypted_len, 0);
        ctx->write_seq_num++;
    }

    return status;
}

int8_t tls13_handle_handshake(tls13_context_t* ctx) {
    int32_t res_client_hello = tls13_parse_client_hello(ctx);

    if(res_client_hello == -2) {
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Redirecting HTTP/1.1 client to HTTPS URL");
        return -1;
    }

    if(res_client_hello == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to parse Client Hello");
        return -1;
    }

    get_random_bytes(ctx->server_random, sizeof(ctx->server_random));
    if(x25519_generate_keypair(ctx->server_key_exchange_private_key, ctx->server_key_exchange_public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate X25519 keypair");
        return -1;
    }

    if(x25519_shared_secret(ctx->shared_secret,
                            ctx->server_key_exchange_private_key,
                            ctx->client_key_exchange_public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute shared secret");
        return -1;
    }

    ctx->shared_secret_len = X25519_PUBLIC_KEY_RAW_LEN;

    if(tls13_send_server_hello(ctx) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send Server Hello");
        return -1;
    }

    if(tls13_generate_handshake_key_and_iv(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate handshake key and IV");
        return -1;
    }

    if(tls13_send_encrypted_extensions(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send Encrypted Extensions");
        return -1;
    }

    if(ctx->require_client_certificate) {
        if(tls13_send_certificate_request(ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send Certificate Request");
            return -1;
        }
    }

    if(tls13_send_certificate(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send Certificate");
        return -1;
    }

    if(tls13_send_certificate_verify(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send Certificate Verify");
        return -1;
    }

    if(tls13_send_finished(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to ctx->network_send Finished");
        return -1;
    }

    if(tls13_generate_application_keys(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate application keys");
        return -1;
    }

    if(tls13_handle_client_handshake_read(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to handle Client Handshake Read related messages (Certificate, CertificateVerify, Finished)");
        return -1;
    }

    return 0;
}


