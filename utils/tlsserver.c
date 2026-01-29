/*n*
 * @file tlsserver.c
 * @brief tls server test application.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000
#include "setup.h"
#include <strings.h>
#include <crypto/aes-gcm.h>
#include <crypto/sha2.h>
#include <bigint.h>
#include <crypto/x25519.h>
#include <errno.h>
#include <crypto/der.h>
#include <crypto/pem.h>
#include <crypto/x509.h>
#include <base64.h>
#include <list.h>

#define PORT 10443

#define MAKE_STRING_HELPER(x) #x

#define MAKE_STRING(x) MAKE_STRING_HELPER(x)

#define MAKE_HOST_WITH_PORT(hostname) "" hostname "" ":" MAKE_STRING(PORT)

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

typedef struct tls13_context_t {
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
    uint8_t                client_pubkey[X25519_PUBLIC_KEY_RAW_LEN];
    uint8_t                server_privkey[X25519_PRIVATE_KEY_RAW_LEN];
    uint8_t                server_pubkey[X25519_PUBLIC_KEY_RAW_LEN];
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
} tls13_context_t;

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

static void tls13_destroy_context(tls13_context_t* tls13_ctx) {
    if (!tls13_ctx) {
        return;
    }
    // Don't forget to free session_id and tls13_ctx when done
    if (tls13_ctx->session_id) {
        memory_free(tls13_ctx->session_id);
    }
    tls13_hash_final(tls13_ctx);
    memory_free(tls13_ctx->handshake_hash_value);

    if (tls13_ctx->server_certificate) {
        x509_certificate_free(tls13_ctx->server_certificate);
    }

    if (tls13_ctx->ca_certificate) {
        x509_certificate_free(tls13_ctx->ca_certificate);
    }

    if (tls13_ctx->server_private_key.data) {
        memory_free(tls13_ctx->server_private_key.data);
    }

    memory_free(tls13_ctx);
}

static int8_t tls13_load_certificate_and_key(tls13_context_t* tls13_ctx) {
    // first check build/ca.pem and build/ca.key exists
    // if exists load them else generate new CA certificate and key
    boolean_t ca_exists = false;
    FILE* f = fopen("build/ca.pem", "rb");
    if(f) {
        ca_exists = true;
        fclose(f);
    }

    f = fopen("build/ca.key", "rb");

    if(f) {
        ca_exists = ca_exists && true;
        fclose(f);
    } else {
        ca_exists = false;
    }

    if(!ca_exists) {
        // generate new CA certificate and KEY    x509_certificate_t* cert = x509_certificate_new();
        x509_certificate_t* cert = x509_certificate_new();
        if (cert == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create new X509 certificate");
            return -1;
        }

        if (x509_certificate_add_issuer_common_name(cert, "Test CA") != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add issuer common name");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_add_subject_common_name(cert, "Test CA") != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject common name");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_add_duration(cert, 365) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add certificate duration");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_set_is_ca(cert, true, -1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set certificate as CA");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_add_key_usage(cert, X509_KEY_USAGE_KEY_CERT_SIGN | X509_KEY_USAGE_CRL_SIGN) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add key usage");
            x509_certificate_free(cert);
            return -1;
        }

        uint8_t private_key[32];
        uint8_t public_key[32];

        if(ed25519_generate_keypair(private_key, public_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate X25519 keypair");
            x509_certificate_free(cert);
            return -1;
        }

        uint8_t* skid = sha256_hash(public_key, 32);
        if(skid == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate SKID");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_add_subject_key_identifier(cert, skid, 32) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject key identifier");
            memory_free(skid);
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_add_authority_key_identifier(cert, skid, 32) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add authority key identifier");
            memory_free(skid);
            x509_certificate_free(cert);
            return -1;
        }

        memory_free(skid);

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ED25519, public_key, 32) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to certificate");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_sign(cert, X509_ALGORITHM_ED25519,
                                  private_key, sizeof(private_key)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign certificate");
            x509_certificate_free(cert);
            return -1;
        }

        char_t* final_cert_data = x509_certificate_get_pem(cert);
        if (final_cert_data == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get final certificate data");
            x509_certificate_free(cert);
            return -1;
        }

        x509_certificate_free(cert);

        f = fopen("build/ca.pem", "wb");
        if (f == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open certificate.der for writing");
            memory_free(final_cert_data);
            return -1;
        }

        fwrite(final_cert_data, 1, strlen(final_cert_data), f);
        fclose(f);

        memory_free(final_cert_data);

        char_t* final_key_data = NULL;
        if(pem_write_ed25519_private_key(private_key, &final_key_data) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to write private key to PEM format");
            return -1;
        }

        f = fopen("build/ca.key", "wb");
        if (f == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open ca.key for writing");
            memory_free(final_key_data);
            return -1;
        }

        fwrite(final_key_data, 1, strlen(final_key_data), f);
        fclose(f);

        memory_free(final_key_data);

        print_success("CA certificate generated successfully: ca.pem ca.key");
    }

    f = fopen("build/ca.pem", "rb");
    if(!f) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open CA certificate file");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long ca_cert_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* ca_cert_data = memory_malloc(ca_cert_size);
    if(!ca_cert_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA certificate");
        fclose(f);
        return -1;
    }
    fread(ca_cert_data, 1, ca_cert_size, f);
    fclose(f);

    tls13_ctx->ca_certificate = x509_certificate_from_pem((char_t*)ca_cert_data);
    memory_free(ca_cert_data);

    if(!tls13_ctx->ca_certificate) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load CA certificate from PEM");
        return -1;
    }

    f = fopen("build/ca.key", "rb");
    if(!f) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open CA private key file");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long ca_key_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* ca_key_data = memory_malloc(ca_key_size);
    if(!ca_key_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
        fclose(f);
        return -1;
    }
    fread(ca_key_data, 1, ca_key_size, f);
    fclose(f);

    uint8_t ca_private_key[ED25519_PRIVATE_KEY_RAW_LEN];

    if(pem_read_ed25519_private_key((char_t*)ca_key_data, ca_private_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read CA private key from PEM");
        memory_free(ca_key_data);
        return -1;
    }
    memory_free(ca_key_data);

    uint8_t ca_public_key[ED25519_PUBLIC_KEY_RAW_LEN];
    if(ed25519_derive_pubkey(ca_public_key, ca_private_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive CA public key from private key");
        return -1;
    }

    if(x509_certificate_verify_signature(tls13_ctx->ca_certificate, ca_public_key, sizeof(ca_public_key)) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to verify CA certificate signature");
        return -1;
    }

    // now generate server certificate signed by CA
    x509_certificate_t* cert = x509_certificate_new();
    if (cert == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create new X509 certificate");
        return -1;
    }

    if (x509_certificate_add_issuer_common_name(cert, "Test CA") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add issuer common name");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_common_name(cert, "Test Server") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject common name");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_duration(cert, 365) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add certificate duration");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_set_is_ca(cert, false, -1) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set certificate as non-CA");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_key_usage(cert, X509_KEY_USAGE_DIGITAL_SIGNATURE | X509_KEY_USAGE_KEY_ENCIPHERMENT) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add key usage");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_extended_key_usage(cert, X509_EXTENDED_KEY_USAGE_SERVER_AUTH) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add extended key usage");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_alternative_name(cert, X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS, "localhost") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject alternative name");
        x509_certificate_free(cert);
        return -1;
    }

    if(x509_certificate_add_subject_alternative_name(cert, X509_SUBJECT_ALTERNATIVE_NAME_TYPE_IP, "127.0.0.1") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject alternative name IP");
        x509_certificate_free(cert);
        return -1;
    }

    uint8_t server_private_key[32];
    uint8_t server_public_key[32];

    if(ed25519_generate_keypair(server_private_key, server_public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server X25519 keypair");
        x509_certificate_free(cert);
        return -1;
    }

    uint8_t* skid = sha256_hash(server_public_key, 32);
    if(skid == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server SKID");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_key_identifier(cert, skid, 32) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add server subject key identifier");
        memory_free(skid);
        x509_certificate_free(cert);
        return -1;
    }

    memory_free(skid);

    uint8_t* akid = sha256_hash(ca_public_key, 32);
    if(akid == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server AKID");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_authority_key_identifier(cert, akid, 32) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add server authority key identifier");
        memory_free(akid);
        x509_certificate_free(cert);
        return -1;
    }

    memory_free(akid);

    if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ED25519, server_public_key, 32) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to server certificate");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_sign(cert, X509_ALGORITHM_ED25519,
                              ca_private_key, sizeof(ca_private_key)) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign server certificate");
        x509_certificate_free(cert);
        return -1;
    }

    memory_memclean(ca_private_key, sizeof(ca_private_key));

    tls13_ctx->server_certificate = cert;
    tls13_ctx->server_private_key.data = memory_malloc(ED25519_PRIVATE_KEY_RAW_LEN);
    if(!tls13_ctx->server_private_key.data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for server private key PEM");
        x509_certificate_free(cert);
        return -1;
    }

    memory_memcopy(server_private_key, tls13_ctx->server_private_key.data, ED25519_PRIVATE_KEY_RAW_LEN);
    tls13_ctx->server_private_key.length = ED25519_PRIVATE_KEY_RAW_LEN;

    memory_memclean(server_private_key, sizeof(server_private_key));

    return 0;
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

static int8_t tls13_client_hello(tls13_context_t* tls13_ctx, int32_t client_fd) {
    if(!tls13_ctx) {
        return -1;
    }

    uint8_t header[5];

    int32_t received = recv(client_fd, header, 5, 0);

    if(received != 5) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to receive TLS record header");
        return -1;
    }

    if(header[0] != 0x16) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Not a handshake record");

        // check for GET request (HTTP)
        if(header[0] == 'G' && header[1] == 'E' && header[2] == 'T') {
            PRINTLOG(CRYPTOLIB, LOG_INFO, "Received HTTP GET request, sending 301 redirect to HTTPS");
            uint8_t buffer[512];
            memory_memclean(buffer, sizeof(buffer));
            memory_memcopy(header, &buffer[0], 5);
            received = recv(client_fd, &buffer[5], 506, 0);
            if(received > 0) {
                buffer[5 + received] = '\0';
                // find Host header
                char_t default_host[256] = MAKE_HOST_WITH_PORT("localhost");
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
                    "HTTP/1.1 301 Moved Permanently\r\n"
                    "Location: https://%s/\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n",
                    default_host
                    );
                send(client_fd, (uint8_t*)response, strlen(response), 0);
                memory_free(response);
                PRINTLOG(CRYPTOLIB, LOG_INFO, "Sent 301 redirect to https://%s/", default_host);

                return -2;
            }

            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to receive complete HTTP GET request");
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

    received = recv(client_fd, buffer, record_len, 0);

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
    tls13_ctx->version = client_version;

    // 4. Client Random (32 bytes)
    uint8_t* client_random = &handshake[6];
    memory_memcopy(client_random, tls13_ctx->client_random, 32);

    // 5. Session ID (Variable length)
    uint8_t session_id_len = handshake[38];
    uint8_t* session_id = &handshake[39];
    tls13_ctx->session_id_len = session_id_len;
    if (session_id_len > 0) {
        tls13_ctx->session_id = (uint8_t*)memory_malloc(session_id_len);
        if (!tls13_ctx->session_id) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for session_id failed");
            memory_free(buffer);
            return -1;
        }
        memory_memcopy(session_id, tls13_ctx->session_id, session_id_len);
    } else {
        tls13_ctx->session_id = NULL;
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
            tls13_ctx->cipher_suite = TLS_AES_128_GCM_SHA256;
            tls13_ctx->selected_hash_algorithm = TLS_HASH_SHA256;
            tls13_ctx->handshake_hash_len = SHA256_OUTPUT_SIZE;
            tls13_ctx->handshake_key_len = AES128_KEY_SIZE; // AES-128 key length
            tls13_ctx->handshake_iv_len = 12; // AES-GCM standard IV length
            cipher_suit_found = true;
            // You can break here or continue to see what else the client offers
        } else if (suite == TLS_AES_256_GCM_SHA384) {
            tls13_ctx->cipher_suite = TLS_AES_256_GCM_SHA384;
            tls13_ctx->selected_hash_algorithm = TLS_HASH_SHA384;
            tls13_ctx->handshake_hash_len = SHA384_OUTPUT_SIZE;
            tls13_ctx->handshake_key_len = AES256_KEY_SIZE; // AES-256 key length
            tls13_ctx->handshake_iv_len = 12; // AES-GCM standard IV length
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
                    if (name_len < sizeof(tls13_ctx->sni_hostname)) {
                        memory_memcopy(sni_list_ptr + 3, tls13_ctx->sni_hostname, name_len);
                        tls13_ctx->sni_hostname[name_len] = '\0';
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
            tls13_ctx->has_alpn = true;
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
                        tls13_ctx->alpn_h2 = true;
                    } else if (strcmp(protocol, "http/1.1") == 0) {
                        tls13_ctx->alpn_http11 = true;
                    }
                }

                ptr += (1 + str_len);
                processed += (1 + str_len);
            }
        } else if (ext_type == TLS_EXTENSION_SUPPORTED_VERSIONS) { // Supported Versions
            tls13_ctx->tls13_supported = true;
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
                    tls13_ctx->x25519_supported = true;
                    if (key_len == X25519_PUBLIC_KEY_RAW_LEN) {
                        memory_memcopy(key_data, tls13_ctx->client_pubkey, X25519_PUBLIC_KEY_RAW_LEN);
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

    if (!tls13_ctx->tls13_supported) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client does not support TLS 1.3");
        memory_free(buffer);
        return -1;
    }

    if (!tls13_ctx->x25519_supported) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client does not support X25519 key exchange");
        memory_free(buffer);
        return -1;
    }

    if(tls13_ctx->selected_hash_algorithm == TLS_HASH_NONE) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No supported hash algorithm selected");
        memory_free(buffer);
        return -1;
    }

    if(tls13_hash_update(tls13_ctx, handshake, record_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update handshake hash");
        memory_free(buffer);
        return -1;
    }

    memory_free(buffer);

    return 0;
}

static int32_t tls13_send_server_hello(tls13_context_t* ctx, int32_t client_fd) {
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
    memory_memcopy(ctx->server_pubkey, &msg[p], 32);
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

    return send(client_fd, msg, p, 0);
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

static int8_t tls13_send_encrypted_extensions(tls13_context_t* ctx, int32_t client_fd) {
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
    if (send(client_fd, aad, 5, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send TLS record header");
        return -1;
    }

    if (send(client_fd, ciphertext, encrypted_record_len, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send encrypted extensions");
        return -1;
    }

    ctx->write_seq_num++;
    return 0;
}

static int8_t tls13_send_certificate(tls13_context_t* ctx, int32_t client_fd) {
    // We need the raw DER for both
    size_t server_der_len = 0;
    uint8_t* server_der = x509_certificate_get_der(ctx->server_certificate, &server_der_len);

    size_t ca_der_len = 0;
    uint8_t* ca_der = x509_certificate_get_der(ctx->ca_certificate, &ca_der_len);

    if (!server_der || !ca_der) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get DER encoding of certificates");
        memory_free(server_der);
        memory_free(ca_der);
        return -1;
    }

    size_t total_cert_len = 3 + server_der_len + 2 + 3 + ca_der_len + 2;
    size_t estimated_hs_len = 1 + 3 + 1 + 3 + total_cert_len;
    estimated_hs_len += 4096 - (estimated_hs_len % 4096); // Padding for safety

    uint8_t* plaintext = (uint8_t*)memory_malloc(estimated_hs_len);
    uint8_t* ciphertext = (uint8_t*)memory_malloc(estimated_hs_len + 16);
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

    send(client_fd, aad, 5, 0);
    send(client_fd, ciphertext, encrypted_len, 0);

    ctx->write_seq_num++;

    memory_free(plaintext);
    memory_free(ciphertext);
    return 0;
}

static int8_t tls13_send_certificate_verify(tls13_context_t* ctx, int32_t client_fd) {
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

    send(client_fd, aad, 5, 0);
    send(client_fd, ciphertext, encrypted_len, 0);

    ctx->write_seq_num++;
    return 0;
}

static int8_t tls13_send_finished(tls13_context_t* ctx, int32_t client_fd) {
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

    send(client_fd, aad, 5, 0);
    send(client_fd, ciphertext, encrypted_len, 0);

    ctx->write_seq_num++;

    return 0;
}

static int8_t tls13_parse_and_verify_client_finished(tls13_context_t* ctx,
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

static int8_t tls13_handle_client_finished_record(tls13_context_t* ctx, int32_t client_fd) {
    uint8_t header[5];
    int32_t ret = recv(client_fd, header, 5, 0);
    if (ret != 5) {
        return -1;
    }

    // Handle Dummy ChangeCipherSpec (CCS is Type 0x14)
    if (header[0] == 0x14) {
        uint16_t ccs_len = (header[3] << 8) | header[4];
        uint8_t dummy[16];
        recv(client_fd, dummy, ccs_len, 0);
        // Recursively call to get the actual 0x17 record following the CCS
        return tls13_handle_client_finished_record(ctx, client_fd);
    }

    if (header[0] != 0x17) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected encrypted record (0x17), got 0x%02x", header[0]);
        return -1;
    }

    int32_t record_len = (header[3] << 8) | header[4];
    uint8_t* buffer = (uint8_t*)memory_malloc(record_len);

    ret = recv(client_fd, buffer, record_len, 0);
    if (ret != record_len) {
        memory_free(buffer);
        return -1;
    }

    uint32_t ciphertext_len = record_len - 16;
    uint8_t* tag = buffer + ciphertext_len;

    uint8_t nonce[12];
    tls13_make_nonce(ctx->client_handshake_iv, ctx->read_seq_num, nonce);

    uint8_t plaintext[256];
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
        return -1;
    }

    // Identify real content type (ignores potential padding)
    int32_t type_pos = ciphertext_len - 1;
    while (type_pos > 0 && plaintext[type_pos] == 0x00) {type_pos--;}
    uint8_t inner_type = plaintext[type_pos];

    if (inner_type == 0x15) { // ALERT
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Client sent Alert: %d %d", plaintext[0], plaintext[1]);
        return -1;
    }

    if (inner_type != 0x16 || plaintext[0] != 0x14) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected Finished message, got Type 0x%02x/Inner 0x%02x", plaintext[0], inner_type);
        return -1;
    }

    uint32_t verify_data_len = (plaintext[1] << 16) | (plaintext[2] << 8) | plaintext[3];
    if (tls13_parse_and_verify_client_finished(ctx, &plaintext[4], verify_data_len) != 0) {
        return -1;
    }

    // Update transcript with decrypted Handshake message
    tls13_hash_update(ctx, plaintext, 4 + verify_data_len);

    // Sequence numbers are reset in the generate_application_keys step
    ctx->read_seq_num = 0;
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

static int32_t tls13_write(tls13_context_t* ctx, int32_t client_fd, const uint8_t* data, uint32_t len) {
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
        return -1;
    }

    // Send Header + Ciphertext
    if (send(client_fd, aad, 5, 0) < 0) {
        return -1;
    }
    if (send(client_fd, ciphertext, encrypted_record_len, 0) < 0) {
        return -1;
    }

    ctx->write_seq_num++;
    return len;
}

static int32_t tls13_read(tls13_context_t* ctx, int32_t client_fd, uint8_t* out_data, uint32_t max_len) {
    uint8_t header[5];
    if (recv(client_fd, header, 5, 0) <= 0) {
        return -1;
    }

    // Handle legacy ChangeCipherSpec if it pops up mid-stream (unlikely but possible)
    if (header[0] == 0x14) {
        uint16_t ccs_len = (header[3] << 8) | header[4];
        uint8_t dummy[16];
        recv(client_fd, dummy, ccs_len, 0);
        return tls13_read(ctx, client_fd, out_data, max_len);
    }

    if (header[0] != 0x17) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected encrypted record (0x17), got 0x%02x", header[0]);
        return -1;
    }

    uint16_t record_len = (header[3] << 8) | header[4];
    uint8_t* buffer = memory_malloc(record_len);
    if (recv(client_fd, buffer, record_len, 0) <= 0) {
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
    uint32_t real_data_len = type_pos; // Data ends before the type byte

    // --- STEP 2: Handle Inner Types ---
    if (inner_type == 0x15) { // ALERT
        if (plaintext[0] == 0x01 && plaintext[1] == 0x00) {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Received Close Notify.");
        }
        memory_free(plaintext);
        return 0;
    }

    if (inner_type == 0x16) { // POST-HANDSHAKE (e.g. KeyUpdate or NewSessionTicket)
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Received post-handshake message type 0x%02x", plaintext[0]);
        // Note: KeyUpdate is 0x18. If you don't handle it,
        // the next record will fail decryption because keys didn't rotate!
        memory_free(plaintext);
        return tls13_read(ctx, client_fd, out_data, max_len);
    }

    if (inner_type != 0x17) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unexpected inner type 0x%02x", inner_type);
        memory_free(plaintext);
        return 0;
    }

    // Success: Copy application data
    uint32_t to_copy = (real_data_len < max_len) ? real_data_len : max_len;
    memory_memcopy(plaintext, out_data, to_copy);

    ctx->read_seq_num++;
    memory_free(plaintext);
    return to_copy;
}

static int8_t tls13_send_close_notify(tls13_context_t* ctx, int32_t client_fd) {
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
        send(client_fd, aad, 5, 0);
        send(client_fd, ciphertext, encrypted_len, 0);
        ctx->write_seq_num++;
    }

    return status;
}

static int8_t tls13_handle_handshake(tls13_context_t* ctx, int32_t client_fd) {
    int32_t res_client_hello = tls13_client_hello(ctx, client_fd);

    if(res_client_hello == -2) {
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Redirecting HTTP/1.1 client to HTTPS URL");
        return -1;
    }

    if(res_client_hello == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to parse Client Hello");
        return -1;
    }

    get_random_bytes(ctx->server_random, sizeof(ctx->server_random));
    if(x25519_generate_keypair(ctx->server_privkey, ctx->server_pubkey) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate X25519 keypair");
        return -1;
    }

    if(x25519_shared_secret(ctx->shared_secret,
                            ctx->server_privkey,
                            ctx->client_pubkey) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute shared secret");
        return -1;
    }

    ctx->shared_secret_len = X25519_PUBLIC_KEY_RAW_LEN;

    if(tls13_send_server_hello(ctx, client_fd) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Server Hello");
        return -1;
    }

    if(tls13_generate_handshake_key_and_iv(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate handshake key and IV");
        return -1;
    }

    if(tls13_send_encrypted_extensions(ctx, client_fd) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Encrypted Extensions");
        return -1;
    }

    if(tls13_send_certificate(ctx, client_fd) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Certificate");
        return -1;
    }

    if(tls13_send_certificate_verify(ctx, client_fd) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Certificate Verify");
        return -1;
    }

    if(tls13_send_finished(ctx, client_fd) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Finished");
        return -1;
    }

    if(tls13_generate_application_keys(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate application keys");
        return -1;
    }

    if(tls13_handle_client_finished_record(ctx, client_fd) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to handle Client Finished record");
        return -1;
    }

    return 0;
}

typedef enum http_version_t {
    HTTP_VERSION_1_1,
    HTTP_VERSION_2,
} http_version_t;

typedef enum http_method_t {
    HTTP_METHOD_UNKNOWN,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_PATCH,
} http_method_t;

typedef enum content_type_t {
    CONTENT_TYPE_TEXT_HTML,
    CONTENT_TYPE_APPLICATION_JSON,
    CONTENT_TYPE_TEXT_PLAIN,
    CONTENT_TYPE_APPLICATION_OCTET_STREAM,
} content_type_t;

typedef struct http_request_t {
    http_version_t version;
    http_method_t  method;
    char_t         path[1024]; // request path without query string
    list_t*        headers; // list of http_header_t
    list_t*        query_params; // list of http_query_param_t
    buffer_t*      body; // for storing request body
} http_request_t;

typedef struct http_header_t {
    char_t* name;
    char_t* value;
} http_header_t;

typedef struct http_query_param_t {
    char_t* name;
    char_t* value;
} http_query_param_t;

typedef struct http_response_t {
    http_version_t version;
    int32_t        status_code;
    list_t*        headers; // list of http_header_t
    buffer_t*      body; // for storing response body
} http_response_t;

static int8_t http_handle(http_request_t* request, http_response_t* response) {
    // Simple handler: respond with 200 OK and a hello message
    response->status_code = 200;
    response->version = request->version;
    response->body = buffer_new();
    if(!response->body) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for response body");
        return -1;
    }

    PRINTLOG(CRYPTOLIB, LOG_INFO, "Handling HTTP request for path: %s", request->path);
    for (size_t i = 0; i < list_size(request->headers); i++) {
        http_header_t* header = (http_header_t*)list_get_data_at_position(request->headers, i);
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Request Header: %s: %s", header->name, header->value);
    }
    for (size_t i = 0; i < list_size(request->query_params); i++) {
        http_query_param_t* param = (http_query_param_t*)list_get_data_at_position(request->query_params, i);
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Query Param: %s=%s", param->name, param->value);
    }

    const char_t* message = "<html><body><h1>Hello, World!</h1></body></html>\n";
    buffer_append_bytes(response->body, (uint8_t*)message, strlen(message));

    // Add Content-Type header
    http_header_t* content_type_header = (http_header_t*)memory_malloc(sizeof(http_header_t));
    if(!content_type_header) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Content-Type header");
        return -1;
    }
    content_type_header->name = strdup("Content-Type");
    content_type_header->value = strdup("text/html; charset=UTF-8");
    if(!content_type_header->name || !content_type_header->value) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Content-Type header strings");
        memory_free(content_type_header);
        return -1;
    }

    response->headers = list_create_list();
    if(!response->headers) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for response headers list");
        memory_free(content_type_header->name);
        memory_free(content_type_header->value);
        memory_free(content_type_header);
        return -1;
    }
    list_list_insert(response->headers, content_type_header);

    // Add Content-Length header
    http_header_t* content_length_header = (http_header_t*)memory_malloc(sizeof(http_header_t));
    if(!content_length_header) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Content-Length header");
        return -1;
    }
    content_length_header->name = strdup("Content-Length");
    content_length_header->value = itoa(buffer_get_length(response->body));
    if(!content_length_header->name || !content_length_header->value) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Content-Length header name");
        memory_free(content_length_header->name);
        memory_free(content_length_header->value);
        memory_free(content_length_header);
        return -1;
    }
    list_list_insert(response->headers, content_length_header);

    return 0;
}

static void http_free_request(http_request_t* request) {
    if(!request) {
        return;
    }

    if(request->headers) {
        size_t header_count = list_size(request->headers);
        for(size_t i = 0; i < header_count; i++) {
            http_header_t* header = (http_header_t*)list_get_data_at_position(request->headers, i);
            if(header) {
                memory_free(header->name);
                memory_free(header->value);
                memory_free(header);
            }
        }
        list_destroy(request->headers);
    }

    if(request->query_params) {
        size_t param_count = list_size(request->query_params);
        for(size_t i = 0; i < param_count; i++) {
            http_query_param_t* param = (http_query_param_t*)list_get_data_at_position(request->query_params, i);
            if(param) {
                memory_free(param->name);
                memory_free(param->value);
                memory_free(param);
            }
        }
        list_destroy(request->query_params);
    }

    if(request->body) {
        buffer_destroy(request->body);
    }

    memory_free(request);
}

static void http_free_response(http_response_t* response) {
    if(!response) {
        return;
    }

    if(response->headers) {
        size_t header_count = list_size(response->headers);
        for(size_t i = 0; i < header_count; i++) {
            http_header_t* header = (http_header_t*)list_get_data_at_position(response->headers, i);
            if(header) {
                memory_free(header->name);
                memory_free(header->value);
                memory_free(header);
            }
        }
        list_destroy(response->headers);
    }

    if(response->body) {
        buffer_destroy(response->body);
    }

    memory_free(response);
}

static int8_t http11_handle_connection(tls13_context_t* ctx, int32_t client_fd) {
    int8_t ret = -1;
    http_request_t* request = NULL;
    http_response_t* response = NULL;


    uint8_t buffer[16384];
    int32_t bytes_received = 0;
    bytes_received = tls13_read(ctx, client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_received < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS application data read failed");
        return -1;
    }

    buffer[bytes_received] = '\0'; // Null-terminate for string operations

    request = (http_request_t*)memory_malloc(sizeof(http_request_t));
    if(!request) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP request");
        return -1;
    }

    char_t method[16], path[1024], version[16];
    size_t offset = 0;

    while(buffer[offset] == ' ') {
        offset++; // Skip leading spaces
    }

    while(buffer[offset] != ' ' && buffer[offset] != '\0' && offset < sizeof(buffer) - 1) {
        method[offset] = buffer[offset];
        offset++;
    }
    method[offset] = '\0';

    if(strcmp(method, "GET") == 0) {
        request->method = HTTP_METHOD_GET;
    } else if(strcmp(method, "POST") == 0) {
        request->method = HTTP_METHOD_POST;
    } else if(strcmp(method, "PUT") == 0) {
        request->method = HTTP_METHOD_PUT;
    } else if(strcmp(method, "DELETE") == 0) {
        request->method = HTTP_METHOD_DELETE;
    } else if(strcmp(method, "HEAD") == 0) {
        request->method = HTTP_METHOD_HEAD;
    } else if(strcmp(method, "OPTIONS") == 0) {
        request->method = HTTP_METHOD_OPTIONS;
    } else if(strcmp(method, "PATCH") == 0) {
        request->method = HTTP_METHOD_PATCH;
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unknown HTTP method: %s", method);
        goto error_cleanup;
    }

    while(buffer[offset] == ' ') {
        offset++; // Skip spaces
    }

    size_t path_start = offset;
    while(buffer[offset] != ' ' && buffer[offset] != '\0' && offset < sizeof(buffer) - 1) {
        path[offset - path_start] = buffer[offset];
        offset++;
    }
    path[offset - path_start] = '\0';

    while(buffer[offset] == ' ') {
        offset++; // Skip spaces
    }

    size_t version_start = offset;
    while(buffer[offset] != '\r' && buffer[offset] != '\n' && buffer[offset] != '\0' && offset < sizeof(buffer) - 1) {
        version[offset - version_start] = buffer[offset];
        offset++;
    }
    version[offset - version_start] = '\0';

    if(strcmp(version, "HTTP/1.1") == 0) {
        request->version = HTTP_VERSION_1_1;
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported HTTP version: %s", version);
        goto error_cleanup;
    }

    // path can have query string, parse and separate it
    size_t query_pos = 0;
    while(path[query_pos] != '\0' && path[query_pos] != '?') {
        query_pos++;
    }

    if(path[query_pos] == '?') {
        request->query_params = list_create_list();
        if(!request->query_params) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP query params list");
            goto error_cleanup;
        }

        path[query_pos] = '\0'; // terminate path
        memory_memcopy(path, request->path, query_pos + 1);

        // now parse query string
        size_t qp_start = query_pos + 1;;
        while(path[qp_start] != '\0') {
            size_t name_start = qp_start;

            while(path[qp_start] != '=' && path[qp_start] != '&' && path[qp_start] != '\0') {
                qp_start++;
            }

            char_t* name = (char_t*)memory_malloc(qp_start - name_start + 1);
            memory_memcopy(&path[name_start], name, qp_start - name_start);

            name[qp_start - name_start] = '\0';

            char_t* value = NULL;

            if(path[qp_start] == '=') {
                qp_start++;
                size_t value_start = qp_start;

                while(path[qp_start] != '&' && path[qp_start] != '\0') {
                    qp_start++;
                }

                value = (char_t*)memory_malloc(qp_start - value_start + 1);
                memory_memcopy(&path[value_start], value, qp_start - value_start);
                value[qp_start - value_start] = '\0';
            }

            http_query_param_t* param = (http_query_param_t*)memory_malloc(sizeof(http_query_param_t));
            if(!param) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP query param");
                goto error_cleanup;
            }

            param->name = name;
            param->value = value;

            if(list_list_insert(request->query_params, param) == -1ULL) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to insert HTTP query param into list");
                goto error_cleanup;
            }

            if(path[qp_start] == '&') {
                qp_start++;
            }
        }
    } else {
        memory_memcopy(path, request->path, strlen(path) + 1);
    }

    request->headers = list_create_list();
    if(!request->headers) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP headers list");
        goto error_cleanup;
    }

    size_t content_length = 0;

    // remove CRLF from the end of the request line
    if(buffer[offset] == '\r' && buffer[offset + 1] == '\n') {
        offset += 2;
    }

    while(buffer[offset] != '\0' && !(buffer[offset] == '\r' && buffer[offset + 1] == '\n') && offset < (size_t)bytes_received) {
        // parse header line
        size_t name_start = offset;
        while(buffer[offset] != ':' && buffer[offset] != '\0') {
            offset++;
        }

        if(buffer[offset] == '\0') {
            break;
        }

        char_t* name = (char_t*)memory_malloc(offset - name_start + 1);
        memory_memcopy(&buffer[name_start], name, offset - name_start);
        name[offset - name_start] = '\0';

        offset++; // skip ':'
        while(buffer[offset] == ' ') {
            offset++; // skip spaces
        }

        size_t value_start = offset;
        while(!(buffer[offset] == '\r' && buffer[offset + 1] == '\n') && buffer[offset] != '\0') {
            offset++;
        }

        char_t* value = (char_t*)memory_malloc(offset - value_start + 1);
        memory_memcopy(&buffer[value_start], value, offset - value_start);
        value[offset - value_start] = '\0';

        if(buffer[offset] == '\r' && buffer[offset + 1] == '\n') {
            offset += 2; // skip CRLF
        }

        http_header_t* header = (http_header_t*)memory_malloc(sizeof(http_header_t));
        if(!header) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP header");
            goto error_cleanup;
        }

        header->name = name;
        header->value = value;

        if(list_list_insert(request->headers, header) == -1ULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to insert HTTP header into list");
            goto error_cleanup;
        }

        if(strcmp(name, "Content-Length") == 0  || strcmp(name, "content-length") == 0) {
            content_length = atou(value);
        }
    }

    if(buffer[offset] == '\r' && buffer[offset + 1] == '\n') {
        offset += 2; // skip final CRLF
    }

    if(content_length > 0) {
        request->body = buffer_new_with_capacity(NULL, content_length);
        if(!request->body) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP request body");
            goto error_cleanup;
        }

        size_t body_bytes_read = bytes_received - offset;
        if(body_bytes_read > content_length) {
            body_bytes_read = content_length;
        }

        buffer_append_bytes(request->body, &buffer[offset], body_bytes_read);

        while(body_bytes_read < content_length) {
            uint8_t temp_buffer[4096];
            int32_t to_read = (content_length - body_bytes_read > sizeof(temp_buffer)) ? sizeof(temp_buffer) : (content_length - body_bytes_read);
            int32_t br = tls13_read(ctx, client_fd, temp_buffer, to_read);
            if(br <= 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read HTTP request body");
                goto error_cleanup;
            }
            buffer_append_bytes(request->body, temp_buffer, br);
            body_bytes_read += br;
        }
    }

    response = (http_response_t*)memory_malloc(sizeof(http_response_t));
    if(!response) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP response");
        goto error_cleanup;
    }

    if(http_handle(request, response) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP handler failed");
        goto error_cleanup;
    }

    // Send response
    char_t* status_line = strprintf("HTTP/1.1 %d OK\r\n", response->status_code);
    tls13_write(ctx, client_fd, (uint8_t*)status_line, strlen(status_line));
    memory_free(status_line);
    // Send headers
    for(size_t i = 0; i < list_size(response->headers); i++) {
        http_header_t* header = (http_header_t*)list_get_data_at_position(response->headers, i);
        char_t* header_line = strprintf("%s: %s\r\n", header->name, header->value);
        tls13_write(ctx, client_fd, (uint8_t*)header_line, strlen(header_line));
        memory_free(header_line);
    }
    // End of headers
    tls13_write(ctx, client_fd, (uint8_t*)"\r\n", 2);
    // Send body
    if(response->body && buffer_get_length(response->body) > 0) {
        size_t body_data_len = 0;
        uint8_t* body_data = buffer_get_all_bytes_and_destroy(response->body, &body_data_len);
        uint8_t* original_body_data = body_data;
        response->body = NULL; // prevent double free

        while(body_data_len > 0) {
            int32_t to_write = (body_data_len > 4096) ? 4096 : body_data_len;
            int32_t written = tls13_write(ctx, client_fd, body_data, to_write);
            if(written <= 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send HTTP response body");
                memory_free(body_data);
                goto error_cleanup;
            }
            body_data += to_write;
            body_data_len -= to_write;
        }

        memory_free(original_body_data);
    }

    ret = 0;
error_cleanup:
    http_free_request(request);
    http_free_response(response);
    return ret;
}

static int8_t http2_handle_connection(tls13_context_t* ctx, int32_t client_fd) {
    static const uint8_t http2_response[] = {
        // SETTINGS (empty)
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,

        // SETTINGS ACK
        0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00,

        // HEADERS frame
        0x00, 0x00, 0x0d, // 20 bytes HPACK
        0x01,
        0x04, // END_HEADERS only
        0x00, 0x00, 0x00, 0x01,

        // HPACK
        0x88, // :status: 200
        0x5f, 0x0a, // Literal never indexed: content-type
        't', 'e', 'x', 't', '/', 'p', 'l', 'a', 'i', 'n',

        // DATA frame
        0x00, 0x00, 0x0d,
        0x00,
        0x01, // END_STREAM
        0x00, 0x00, 0x00, 0x01,
        'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\n'
    };

    if(tls13_write(ctx, client_fd, http2_response, sizeof(http2_response)) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send HTTP/2 application data");
        return -1;
    }

    return 0;
}

int32_t main(int32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    gcm_initialize();

    PRINTLOG(CRYPTOLIB, LOG_INFO, "TLS server test application");

    int32_t server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int32_t opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "socket creation failed");
        return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "setsockopt SO_REUSEADDR failed");
        close(server_fd);
        return 1;
    }

    memory_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "bind failed. error code: %lli", errno);
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "listen failed");
        close(server_fd);
        return 1;
    }

    PRINTLOG(CRYPTOLIB, LOG_INFO, "Server listening on port %d (SO_REUSEADDR enabled)", PORT);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Waiting for connections...");

    int32_t request_count = 0;

    while (true && request_count < 2) {
        request_count++;
        // Accept incoming connection
        client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_len);
        if (client_fd == -1) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        PRINTLOG(CRYPTOLIB, LOG_INFO, "New connection from %s:%d", client_ip, ntohs(client_addr.sin_port));

        tls13_context_t* tls13_ctx = (tls13_context_t*)memory_malloc(sizeof(tls13_context_t));

        if(!tls13_ctx) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed");
            close(client_fd);
            continue;
        }

        if(tls13_load_certificate_and_key(tls13_ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load/generate certificate and key");
            tls13_destroy_context(tls13_ctx);
            close(client_fd);
            continue;
        }

        if(tls13_handle_handshake(tls13_ctx, client_fd) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS handshake failed");
            tls13_destroy_context(tls13_ctx);
            close(client_fd);
            continue;
        }

        if(tls13_ctx->alpn_h2) {
            if(http2_handle_connection(tls13_ctx, client_fd) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/2 connection handling failed");
            }
        } else {
            if(http11_handle_connection(tls13_ctx, client_fd) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/1.1 connection handling failed");
            }
        }

        if(tls13_send_close_notify(tls13_ctx, client_fd) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Close Notify");
            tls13_destroy_context(tls13_ctx);
            close(client_fd);
            continue;
        }

        uint8_t buffer[16384];
        int32_t bytes_received = tls13_read(tls13_ctx, client_fd, buffer, sizeof(buffer) - 1);

        if (bytes_received < 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS application data read failed");
            tls13_destroy_context(tls13_ctx);
            close(client_fd);
            continue;
        }

        tls13_destroy_context(tls13_ctx);

        close(client_fd);
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Connection closed");
    }

    close(server_fd);

    return 0;
}
