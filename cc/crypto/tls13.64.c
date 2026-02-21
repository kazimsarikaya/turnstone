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
#include <crypto/mlkem768.h>
#include <crypto/ellipticcurve.h>
#include <crypto/aes-gcm.h>
#include <strings.h>
#include <pipeline.h>
#include <logging.h>
#include <random.h>
#include <time.h>

MODULE("turnstone.lib.crypto.tls13");

typedef enum tls_version_t {
    TLS_VERSION_1_0 = 0x0301,
    TLS_VERSION_1_1 = 0x0302,
    TLS_VERSION_1_2 = 0x0303,
    TLS_VERSION_1_3 = 0x0304,
} tls_version_t;

typedef enum tls13_extension_type_t : uint16_t {
    TLS_EXTENSION_SNI = 0x0000,
    TLS_EXTENSION_STATUS_REQUEST = 0x0005,
    TLS_EXTENSION_SUPPORTED_GROUPS = 0x000a,
    TLS_EXTENSION_SIGNATURE_ALGORITHMS = 0x000d,
    TLS_EXTENSION_ALPN = 0x0010,
    TLS_EXTENSION_SIGNED_CERTIFICATE_TIMESTAMP = 0x0012,
    TLS_EXTENSION_CLIENT_CERTIFICATE_TYPE = 0x0013,
    TLS_EXTENSION_SERVER_CERTIFICATE_TYPE = 0x0014,
    TLS_EXTENSION_PRE_SHARED_KEY = 0x0029,
    TLS_EXTENSION_SUPPORTED_VERSIONS = 0x002b,
    TLS_EXTENSION_PSK_KEY_EXCHANGE_MODES  = 0x002d,
    TLS_EXTENSION_CERTIFICATE_AUTHORITIES = 0x002f,
    TLS_EXTENSION_POST_HANDSHAKE_AUTH = 0x0031,
    TLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT = 0x0032,
    TLS_EXTENSION_KEY_SHARE = 0x0033,
} tls13_extension_type_t;

typedef enum tls13_key_exchange_group_t : uint16_t {
    TLS_GROUP_NONE = 0x0000,
    TLS_GROUP_SECP256R1 = 0x0017,
    TLS_GROUP_SECP384R1 = 0x0018,
    TLS_GROUP_SECP521R1 = 0x0019,
    TLS_GROUP_X25519 = 0x001d,
    TLS_GROUP_X448 = 0x001e,
    TLS_GROUP_X25519_ML_KEM768 = 0x11ec, // not implemented PQC group, reserved for future use
} tls13_key_exchange_group_t;

typedef enum tls13_hash_algorithm_t {
    TLS_HASH_NONE   = 0x00,
    TLS_HASH_SHA256 = 0x04,
    TLS_HASH_SHA384 = 0x05,
} tls13_hash_algorithm_t;

typedef enum tls13_cipher_suite_t : uint16_t {
    TLS_AES_128_GCM_SHA256 = 0x1301,
    TLS_AES_256_GCM_SHA384 = 0x1302,
    TLS_CHACHA20_POLY1305_SHA256 = 0x1303, // not implemented, reserved for future use
} tls13_cipher_suite_t;

typedef enum tls13_signature_algorithm_t : uint16_t {
    TLS_SIG_ALG_NONE = 0x0000,

    /* ECDSA with SHA-2 */
    TLS_SIG_ALG_ECDSA_SECP256R1_SHA256 = 0x0403,
    TLS_SIG_ALG_ECDSA_SECP384R1_SHA384 = 0x0503,
    TLS_SIG_ALG_ECDSA_SECP521R1_SHA512 = 0x0603,

    /* EdDSA */
    TLS_SIG_ALG_ED25519 = 0x0807,
    TLS_SIG_ALG_ED448 = 0x0808,

    /* Post-Quantum ML-DSA (NIST FIPS 204) */
    TLS_SIG_ALG_MLDSA_44 = 0x0904, // Smallest/Fastest
    TLS_SIG_ALG_MLDSA_65 = 0x0905, // Balanced (Recommended)
    TLS_SIG_ALG_MLDSA_87 = 0x0906, // Highest Security

} tls13_signature_algorithm_t;

typedef enum tls13_psk_key_exchange_mode_t : uint8_t {
    TLS13_PSK_KEY_EXHCANGE_MODE_PSK_ONLY = 0,
    TLS13_PSK_KEY_EXCHANGE_MODE_DHE_PSK  = 1,
    TLS13_PSK_KEY_EXCHANGE_MODE_REJECTED = 255, // Not a real mode, used internally to indicate that PSK key exchange is not used
} tls13_psk_key_exchange_mode_t;

typedef enum tls13_content_type_t : uint8_t {
    TLS13_CONTENT_TYPE_CHANGE_CIPHER_SPEC = 0x14,
    TLS13_CONTENT_TYPE_ALERT = 0x15,
    TLS13_CONTENT_TYPE_HANDSHAKE = 0x16,
    TLS13_CONTENT_TYPE_APPLICATION_DATA = 0x17,
    TLS13_CONTENT_TYPE_HEARTBEAT = 0x18,
} tls13_content_type_t;

typedef enum tls13_alert_level_t : uint8_t {
    TLS13_ALERT_LEVEL_WARNING = 1,
    TLS13_ALERT_LEVEL_FATAL = 2,
} tls13_alert_level_t;

typedef enum tls13_alert_description_t : uint8_t {
    TLS13_ALERT_DESCRIPTION_CLOSE_NOTIFY = 0,
    TLS13_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE = 10,
    TLS13_ALERT_DESCRIPTION_BAD_RECORD_MAC = 20,
    TLS13_ALERT_DESCRIPTION_DECRYPTION_FAILED = 21,
    TLS13_ALERT_DESCRIPTION_RECORD_OVERFLOW = 22,
    TLS13_ALERT_DESCRIPTION_DECOMPRESSION_FAILURE = 30,
    TLS13_ALERT_DESCRIPTION_HANDSHAKE_FAILURE = 40,
    TLS13_ALERT_DESCRIPTION_NO_CERTIFICATE  = 41, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE = 42,
    TLS13_ALERT_DESCRIPTION_UNSUPPORTED_CERTIFICATE = 43,
    TLS13_ALERT_DESCRIPTION_CERTIFICATE_REVOKED = 44,
    TLS13_ALERT_DESCRIPTION_CERTIFICATE_EXPIRED = 45,
    TLS13_ALERT_DESCRIPTION_CERTIFICATE_UNKNOWN = 46,
    TLS13_ALERT_DESCRIPTION_ILLEGAL_PARAMETER = 47,
    TLS13_ALERT_DESCRIPTION_UNKNOWN_CA = 48,
    TLS13_ALERT_DESCRIPTION_ACCESS_DENIED = 49,
    TLS13_ALERT_DESCRIPTION_DECODE_ERROR  = 50,
    TLS13_ALERT_DESCRIPTION_DECRYPT_ERROR = 51,
    TLS13_ALERT_DESCRIPTION_EXPORT_RESTRICTION = 60, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_ALERT_DESCRIPTION_PROTOCOL_VERSION = 70,
    TLS13_ALERT_DESCRIPTION_INSUFFICIENT_SECURITY = 71,
    TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR = 80,
    TLS13_ALERT_DESCRIPTION_USER_CANCELED  = 90,
    TLS13_ALERT_DESCRIPTION_NO_RENEGOTIATION = 100, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_ALERT_DESCRIPTION_UNSUPPORTED_EXTENSION = 110,
    TLS13_ALERT_DESCRIPTION_CERTIFICATE_UNOBTAINABLE = 111, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_ALERT_DESCRIPTION_UNRECOGNIZED_NAME = 112,
    TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE_STATUS_RESPONSE = 113,
    TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE_HASH_VALUE = 114, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_ALERT_DESCRIPTION_UNKNOWN_PSK_IDENTITY = 115,
    TLS13_ALERT_DESCRIPTION_CERTIFICATE_REQUIRED = 116,
    TLS13_ALERT_DESCRIPTION_NO_APPLICATION_PROTOCOL = 120,
} tls13_alert_description_t;

typedef enum tls13_handshake_type_t : uint8_t {
    TLS13_HANDSHAKE_TYPE_HELLO_REQUEST = 0, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_CLIENT_HELLO  = 1,
    TLS13_HANDSHAKE_TYPE_SERVER_HELLO  = 2,
    TLS13_HANDSHAKE_TYPE_HELLO_VERIFY_REQUEST = 3, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_NEW_SESSION_TICKET   = 4,
    TLS13_HANDSHAKE_TYPE_END_OF_EARLY_DATA    = 5,
    TLS13_HANDSHAKE_TYPE_HELLO_RETRY_REQUEST  = 6, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_ENCRYPTED_EXTENSIONS = 8,
    TLS13_HANDSHAKE_TYPE_CERTIFICATE = 11,
    TLS13_HANDSHAKE_TYPE_SERVER_KEY_EXCHANGE = 12, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_CERTIFICATE_REQUEST = 13,
    TLS13_HANDSHAKE_TYPE_SERVER_HELLO_DONE   = 14, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_CERTIFICATE_VERIFY  = 15,
    TLS13_HANDSHAKE_TYPE_CLIENT_KEY_EXCHANGE = 16, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_FINISHED = 20,
    TLS13_HANDSHAKE_TYPE_CERTIFICATE_URL = 21, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_CERTIFICATE_STATUS = 22, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_SUPPLEMENTAL_DATA  = 23, // Not used in TLS 1.3, reserved for backward compatibility
    TLS13_HANDSHAKE_TYPE_KEY_UPDATE = 24,
    TLS13_HANDSHAKE_TYPE_MESSAGE_HASH = 254,
} tls13_handshake_type_t;

struct tls13_config_t {
    const char_t*                                   default_host_port;
    tls13_load_server_certificate_and_key_f         load_server_certificate_and_key;
    tls13_client_certificate_verify_callback_f      client_certificate_verify_callback;
    tls13_client_certificates_ca_dn_list_callback_f client_certificates_ca_dn_list_callback;
    tls13_get_psk_encryption_keys_callback_f        get_psk_encryption_keys_callback;
    tls13_network_send_f                            network_send;
    tls13_network_recv_f                            network_recv;
    boolean_t                                       require_client_certificate;
};

typedef struct tls13_connection_state_t {
    tls_version_t                 version;
    boolean_t                     tls13_supported;
    uint8_t                       session_id_len;
    uint8_t*                      session_id;
    char_t                        sni_hostname[256];
    boolean_t                     has_alpn;
    boolean_t                     alpn_h2;
    boolean_t                     alpn_http11;
    tls13_psk_key_exchange_mode_t psk_key_exchange_mode;
    tls13_cipher_suite_t          selected_cipher_suite;
    tls13_hash_algorithm_t        selected_hash_algorithm;
    tls13_key_exchange_group_t    selected_group;
    union {
        sha256_ctx_t* sha256;
        sha384_ctx_t* sha384;
    }                         handshake_hash_ctx;
    uint8_t*                  handshake_hash_value;
    size_t                    shared_secret_len;
    uint8_t*                  shared_secret; // X25519 shared secret
    int32_t                   handshake_hash_len;
    int32_t                   handshake_key_len;
    int32_t                   handshake_iv_len;
    uint8_t                   master_secret[SHA384_OUTPUT_SIZE];
    uint8_t                   resumption_master_secret[SHA384_OUTPUT_SIZE];
    boolean_t                 session_resumed;
    uint8_t                   selected_identity_index;
    uint8_t                   selected_psk_value[SHA384_OUTPUT_SIZE]; // max size for PSK is hash output size
    boolean_t                 has_alert;
    tls13_alert_level_t       alert_level;
    tls13_alert_description_t alert_description;
    int64_t                   network_client_identifier;
    boolean_t                 connection_closed;
} tls13_connection_state_t;

typedef struct tls13_server_state_t {
    uint8_t  server_random[32];
    uint8_t* server_key_exchange_public_key;
    size_t   server_key_exchange_public_key_len;
    uint8_t  server_handshake_key[AES256_KEY_SIZE]; // max size
    uint8_t  server_handshake_iv[12];
    uint8_t  server_handshake_traffic_secret[SHA384_OUTPUT_SIZE];
    uint8_t  server_finished_key[SHA384_OUTPUT_SIZE];
    uint8_t  server_application_key[AES256_KEY_SIZE];
    uint8_t  server_application_iv[12];
    int32_t  write_seq_num;
} tls13_server_state_t;

typedef struct tls13_client_state_t {
    uint8_t                      client_random[32];
    tls13_key_exchange_group_t*  client_supported_groups;
    size_t                       client_supported_groups_len;
    tls13_signature_algorithm_t* client_supported_signature_algorithms;
    x509_algorithm_t*            client_supported_signature_algorithms_x509;
    uint8_t                      client_handshake_key[AES256_KEY_SIZE]; // max size
    uint8_t                      client_handshake_iv[12];
    uint8_t                      client_handshake_traffic_secret[SHA384_OUTPUT_SIZE];
    uint8_t                      client_finished_key[SHA384_OUTPUT_SIZE];
    uint8_t                      client_application_key[AES256_KEY_SIZE];
    uint8_t                      client_application_iv[12];
    int32_t                      read_seq_num;
    pipeline_t*                  read_buffer;
} tls13_client_state_t;

struct tls13_session_t {
    tls13_config_t*          config;
    tls13_connection_state_t connection_state;
    tls13_server_state_t     server_state;
    tls13_client_state_t     client_state;
};

static void tls13_print_alert(tls13_alert_level_t level, tls13_alert_description_t description) {
    const char_t* level_str = (level == TLS13_ALERT_LEVEL_WARNING) ? "Warning" : "Fatal";
    const char_t* description_str = "Unknown Alert";

    switch (description) {
    case TLS13_ALERT_DESCRIPTION_CLOSE_NOTIFY: description_str = "Close Notify"; break;
    case TLS13_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE: description_str = "Unexpected Message"; break;
    case TLS13_ALERT_DESCRIPTION_BAD_RECORD_MAC: description_str = "Bad Record MAC"; break;
    case TLS13_ALERT_DESCRIPTION_DECRYPTION_FAILED: description_str = "Decryption Failed"; break;
    case TLS13_ALERT_DESCRIPTION_RECORD_OVERFLOW: description_str = "Record Overflow"; break;
    case TLS13_ALERT_DESCRIPTION_DECOMPRESSION_FAILURE: description_str = "Decompression Failure"; break;
    case TLS13_ALERT_DESCRIPTION_HANDSHAKE_FAILURE: description_str = "Handshake Failure"; break;
    case TLS13_ALERT_DESCRIPTION_NO_CERTIFICATE: description_str  = "No Certificate"; break;
    case TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE: description_str = "Bad Certificate"; break;
    case TLS13_ALERT_DESCRIPTION_UNSUPPORTED_CERTIFICATE: description_str = "Unsupported Certificate"; break;
    case TLS13_ALERT_DESCRIPTION_CERTIFICATE_REVOKED: description_str = "Certificate Revoked"; break;
    case TLS13_ALERT_DESCRIPTION_CERTIFICATE_EXPIRED: description_str = "Certificate Expired"; break;
    case TLS13_ALERT_DESCRIPTION_CERTIFICATE_UNKNOWN: description_str = "Certificate Unknown"; break;
    case TLS13_ALERT_DESCRIPTION_ILLEGAL_PARAMETER: description_str = "Illegal Parameter"; break;
    case TLS13_ALERT_DESCRIPTION_UNKNOWN_CA: description_str = "Unknown CA"; break;
    case TLS13_ALERT_DESCRIPTION_ACCESS_DENIED: description_str = "Access Denied"; break;
    case TLS13_ALERT_DESCRIPTION_DECODE_ERROR: description_str  = "Decode Error"; break;
    case TLS13_ALERT_DESCRIPTION_DECRYPT_ERROR: description_str = "Decrypt Error"; break;
    case TLS13_ALERT_DESCRIPTION_EXPORT_RESTRICTION: description_str = "Export Restriction"; break;
    case TLS13_ALERT_DESCRIPTION_PROTOCOL_VERSION: description_str = "Protocol Version"; break;
    case TLS13_ALERT_DESCRIPTION_INSUFFICIENT_SECURITY: description_str = "Insufficient Security"; break;
    case TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR: description_str = "Internal Error"; break;
    case TLS13_ALERT_DESCRIPTION_USER_CANCELED: description_str  = "User Canceled"; break;
    case TLS13_ALERT_DESCRIPTION_NO_RENEGOTIATION: description_str = "No Renegotiation"; break;
    case TLS13_ALERT_DESCRIPTION_UNSUPPORTED_EXTENSION: description_str = "Unsupported Extension"; break;
    case TLS13_ALERT_DESCRIPTION_CERTIFICATE_UNOBTAINABLE: description_str = "Certificate Unobtainable"; break;
    case TLS13_ALERT_DESCRIPTION_UNRECOGNIZED_NAME: description_str = "Unrecognized Name"; break;
    case TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE_HASH_VALUE: description_str = "Bad Certificate Hash Value"; break;
    case TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE_STATUS_RESPONSE: description_str = "Bad Certificate Status Response"; break;
    case TLS13_ALERT_DESCRIPTION_UNKNOWN_PSK_IDENTITY: description_str = "Unknown PSK Identity"; break;
    case TLS13_ALERT_DESCRIPTION_CERTIFICATE_REQUIRED: description_str = "Certificate Required"; break;
    case TLS13_ALERT_DESCRIPTION_NO_APPLICATION_PROTOCOL: description_str = "No Application Protocol"; break;
    }

    PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS Alert: Level=%s, Description=%s", level_str, description_str);
}

static int8_t tls13_hash_final(tls13_session_t* ctx) {
    if(!ctx) {
        return -1;
    }

    if(ctx->connection_state.handshake_hash_value) {
        // Already finalized
        return 0;
    }

    if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA256) {
        if (ctx->connection_state.handshake_hash_ctx.sha256) {
            ctx->connection_state.handshake_hash_value = sha256_final(ctx->connection_state.handshake_hash_ctx.sha256);
            ctx->connection_state.handshake_hash_ctx.sha256 = NULL;
            if (ctx->connection_state.handshake_hash_value == NULL) {
                return -1; // Finalization Failed
            }

            return 0;
        }
    } else if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA384) {
        if (ctx->connection_state.handshake_hash_ctx.sha384) {
            ctx->connection_state.handshake_hash_value = sha384_final(ctx->connection_state.handshake_hash_ctx.sha384);
            ctx->connection_state.handshake_hash_ctx.sha384 = NULL;
            if (ctx->connection_state.handshake_hash_value == NULL) {
                return -1; // Finalization Failed
            }

            return 0;
        }
    }

    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for handshake hash finalization");
    return -1; // Unsupported hash algorithm
}

void tls13_destroy_session(tls13_session_t* ctx) {
    if (!ctx) {
        return;
    }
    // Don't forget to free session_id and ctx when done
    if (ctx->connection_state.session_id) {
        memory_free(ctx->connection_state.session_id);
    }
    tls13_hash_final(ctx);
    memory_free(ctx->connection_state.handshake_hash_value);

    if(ctx->client_state.read_buffer) {
        pipeline_destroy(ctx->client_state.read_buffer);
    }

    if(ctx->server_state.server_key_exchange_public_key) {
        memory_free(ctx->server_state.server_key_exchange_public_key);
    }

    if(ctx->client_state.client_supported_groups) {
        memory_free(ctx->client_state.client_supported_groups);
    }

    if(ctx->client_state.client_supported_signature_algorithms) {
        memory_free(ctx->client_state.client_supported_signature_algorithms);
    }

    if(ctx->client_state.client_supported_signature_algorithms_x509) {
        memory_free(ctx->client_state.client_supported_signature_algorithms_x509);
    }

    if(ctx->connection_state.shared_secret) {
        memory_free(ctx->connection_state.shared_secret);
    }

    memory_free(ctx);
}

tls13_config_t* tls13_create_config(const char_t*                                   host_port,
                                    tls13_load_server_certificate_and_key_f         load_server_certificate_and_key,
                                    tls13_client_certificate_verify_callback_f      client_certificate_verify_callback,
                                    tls13_client_certificates_ca_dn_list_callback_f client_certificates_ca_dn_list_callback,
                                    tls13_get_psk_encryption_keys_callback_f        get_psk_encryption_keys_callback,
                                    tls13_network_send_f                            network_send,
                                    tls13_network_recv_f                            network_recv,
                                    boolean_t                                       require_client_certificate) {
    if (!host_port || !network_send || !network_recv) {
        return NULL;
    }

    tls13_config_t* cfg = (tls13_config_t*)memory_malloc(sizeof(tls13_config_t));
    if (!cfg) {
        return NULL;
    }

    cfg->default_host_port = host_port;
    cfg->load_server_certificate_and_key = load_server_certificate_and_key;
    cfg->client_certificate_verify_callback = client_certificate_verify_callback;
    cfg->client_certificates_ca_dn_list_callback = client_certificates_ca_dn_list_callback;
    cfg->get_psk_encryption_keys_callback = get_psk_encryption_keys_callback;
    cfg->network_send = network_send;
    cfg->network_recv = network_recv;
    cfg->require_client_certificate = require_client_certificate;

    return cfg;
}

void tls13_destroy_config(tls13_config_t* tls13_config) {
    if (tls13_config) {
        memory_free(tls13_config);
    }
}

tls13_session_t* tls13_create_session(tls13_config_t* config, int64_t network_client_identifier) {
    if (!config) {
        return NULL;
    }

    tls13_session_t* session = (tls13_session_t*)memory_malloc(sizeof(tls13_session_t));
    if (!session) {
        return NULL;
    }

    session->config = config;
    session->connection_state.psk_key_exchange_mode = TLS13_PSK_KEY_EXCHANGE_MODE_REJECTED; // default to no PSK
    session->connection_state.network_client_identifier = network_client_identifier;

    return session;
}

boolean_t tls13_has_alpn_h2(tls13_session_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->connection_state.alpn_h2;
}

static int8_t tls13_hash_compute(tls13_session_t* ctx, const uint8_t* data, uint32_t len, uint8_t* out_hash) {
    if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA256) {
        uint8_t* hash = sha256_hash(data, len);
        if (!hash) {
            return -1; // Hash computation failed
        }
        memory_memcopy(hash, out_hash, SHA256_OUTPUT_SIZE);
        memory_free(hash);
        return 0;
    } else if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA384) {
        uint8_t* hash = sha384_hash(data, len);
        if (!hash) {
            return -1; // Hash computation failed
        }
        memory_memcopy(hash, out_hash, SHA384_OUTPUT_SIZE);
        memory_free(hash);
        return 0;
    }
    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for handshake hash computation");
    return -1; // Unsupported hash algorithm

}

static int8_t tls13_hash_update(tls13_session_t* ctx, const uint8_t* data, uint32_t len) {
    if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA256) {
        if (!ctx->connection_state.handshake_hash_ctx.sha256) {
            ctx->connection_state.handshake_hash_ctx.sha256 = sha256_init();
            if (!ctx->connection_state.handshake_hash_ctx.sha256) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to initialize SHA256 context");
                return -1; // Initialization Failed
            }
        }
        sha256_update(ctx->connection_state.handshake_hash_ctx.sha256, data, len);
        return 0;
    } else if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA384) {
        if (!ctx->connection_state.handshake_hash_ctx.sha384) {
            ctx->connection_state.handshake_hash_ctx.sha384 = sha384_init();
            if (!ctx->connection_state.handshake_hash_ctx.sha384) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to initialize SHA384 context");
                return -1; // Initialization Failed
            }
        }
        sha384_update(ctx->connection_state.handshake_hash_ctx.sha384, data, len);
        return 0;
    }
    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for handshake hash update");
    return -1; // Unsupported hash algorithm
}

static int8_t tls13_hash_hmac(tls13_hash_algorithm_t hash_alg,
                              const uint8_t* key, uint32_t key_len,
                              const uint8_t* data, uint32_t data_len,
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

static int8_t tls13_hash_get_current(tls13_session_t* ctx, uint8_t* out_hash) {
    uint32_t hlen = ctx->connection_state.handshake_hash_len;
    if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA256) {
        sha256_ctx_t* tmp = sha256_clone(ctx->connection_state.handshake_hash_ctx.sha256);
        uint8_t* h = sha256_final(tmp);
        memory_memcopy(h, out_hash, hlen);
        memory_free(h);
    } else if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA384) {
        sha384_ctx_t* tmp = sha384_clone(ctx->connection_state.handshake_hash_ctx.sha384);
        uint8_t* h = sha384_final(tmp);
        memory_memcopy(h, out_hash, hlen);
        memory_free(h);
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported hash algorithm for getting current hash");
        return -1;
    }

    return 0;
}

static int8_t tls13_hash_get_empty(tls13_session_t* ctx, uint8_t* out_hash) {
    uint32_t hlen = ctx->connection_state.handshake_hash_len;

    if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA256) {
        memory_memcopy(sha256_empty_hash, out_hash, hlen);
    } else if (ctx->connection_state.selected_hash_algorithm == TLS_HASH_SHA384) {
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

static int32_t hkdf_expand(tls13_session_t* ctx,
                           uint8_t* prk, uint8_t* info, uint16_t info_len,
                           uint8_t* out, uint16_t out_len) {

    uint16_t hash_len = ctx->connection_state.handshake_hash_len;
    tls13_hash_algorithm_t hash_alg = ctx->connection_state.selected_hash_algorithm;
    uint16_t n = (out_len + hash_len - 1) / hash_len; // Number of iterations

    if (n > 255) {
        return -1; // RFC limit

    }
    uint8_t T[64]; // Buffer for T(n)
    uint8_t* hash_result = NULL;
    uint16_t out_offset  = 0;

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

static int32_t hkdf_expand_label_ext(tls13_session_t* ctx, uint8_t* secret,
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

static int32_t hkdf_extract(tls13_session_t* ctx,
                            uint8_t* salt, uint32_t salt_len,
                            uint8_t* ikm, uint32_t ikm_len,
                            uint8_t* out_prk) {
    uint8_t* hash_result = NULL;
    uint32_t hash_len = ctx->connection_state.handshake_hash_len;

    // If salt is NULL, use a string of zeros of hash_len
    uint8_t zero_salt[64] = {0};
    if (salt == NULL) {
        salt = zero_salt;
        salt_len = hash_len;
    }

    if (tls13_hash_hmac(ctx->connection_state.selected_hash_algorithm,
                        salt, salt_len,
                        ikm, ikm_len,
                        &hash_result) != 0) {
        return -1;
    }

    memory_memcopy(hash_result, out_prk, hash_len);
    memory_free(hash_result);
    return 0;
}

static int8_t tls13_check_plain_text_protcol(tls13_session_t * ctx, uint8_t* header) {
    if(header[0] != TLS13_CONTENT_TYPE_HANDSHAKE || header[1] != 0x03 || (header[2] < 0x01 || header[2] > 0x04)) {
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
            uint32_t received = ctx->config->network_recv(ctx->connection_state.network_client_identifier, &buffer[5], 506, 0 | 0x80000000); // try once.
            if(received > 0) {
                buffer[5 + received] = '\0';
                // find Host header
                char_t default_host[256];
                memory_memclean(default_host, sizeof(default_host));
                memory_memcopy(ctx->config->default_host_port, default_host, strlen(ctx->config->default_host_port));
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
                ctx->config->network_send(ctx->connection_state.network_client_identifier, (uint8_t*)response, strlen(response), 0);
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

    return 0; // It's a handshake record, continue processing
}

static int8_t tls13_parse_client_hello_cipher_suites(tls13_session_t* ctx, uint8_t* cipher_suites, uint16_t cipher_suites_len) {
    boolean_t cipher_suit_found = false;

    for (int i = 0; i < cipher_suites_len; i += 2) {
        uint16_t suite = (cipher_suites[i] << 8) | cipher_suites[i + 1];

        if (suite == TLS_AES_128_GCM_SHA256) { // TLS_AES_128_GCM_SHA256
            if(ctx->connection_state.selected_cipher_suite == TLS_AES_256_GCM_SHA384) { // Prefer stronger suite if both are offered
                continue; // Already selected, skip
            }
            ctx->connection_state.selected_cipher_suite = TLS_AES_128_GCM_SHA256;
            ctx->connection_state.selected_hash_algorithm = TLS_HASH_SHA256;
            ctx->connection_state.handshake_hash_len = SHA256_OUTPUT_SIZE;
            ctx->connection_state.handshake_key_len  = AES128_KEY_SIZE; // AES-128 key length
            ctx->connection_state.handshake_iv_len = 12; // AES-GCM standard IV length
            cipher_suit_found = true;
            // You can break here or continue to see what else the client offers
        } else if (suite == TLS_AES_256_GCM_SHA384) {
            ctx->connection_state.selected_cipher_suite = TLS_AES_256_GCM_SHA384;
            ctx->connection_state.selected_hash_algorithm = TLS_HASH_SHA384;
            ctx->connection_state.handshake_hash_len = SHA384_OUTPUT_SIZE;
            ctx->connection_state.handshake_key_len  = AES256_KEY_SIZE; // AES-256 key length
            ctx->connection_state.handshake_iv_len = 12; // AES-GCM standard IV length
            cipher_suit_found = true;
        } else if (suite == TLS_CHACHA20_POLY1305_SHA256) {
            // Not implemented, reserved for future use
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Client offered not implemented cipher suite: TLS_CHACHA20_POLY1305_SHA256");
        } else {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client offered unsupported cipher suite: 0x%04x", suite);
        }
    }

    if(!cipher_suit_found) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No supported cipher suites found");
        return -1;
    }

    return 0; // Cipher suite successfully parsed and selected
}

static int8_t tls13_parse_client_hello_extension_sni(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    // Parse SNI to extract hostname
    uint8_t * sni_data = ext_ptr + 4;
    uint16_t sni_list_len  = (sni_data[0] << 8) | sni_data[1];
    uint8_t * sni_list_ptr = sni_data + 2;
    int32_t sni_parsed = 0;

    if(sni_list_len + 2 > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid SNI extension length");
        return -1;
    }

    while (sni_parsed < sni_list_len) {
        uint8_t name_type = sni_list_ptr[0];
        uint16_t name_len = (sni_list_ptr[1] << 8) | sni_list_ptr[2];
        if (name_type == 0) { // hostname
            if (name_len < sizeof(ctx->connection_state.sni_hostname)) {
                memory_memcopy(sni_list_ptr + 3, ctx->connection_state.sni_hostname, name_len);
                ctx->connection_state.sni_hostname[name_len] = '\0';
            } else {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "SNI hostname too long: %d", name_len);
                return -1;
            }
        }
        sni_parsed += 3 + name_len;
        sni_list_ptr += 3 + name_len;
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_alpn(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    ctx->connection_state.has_alpn = true;
    uint8_t * alpn_data = ext_ptr + 4;
    uint16_t alpn_list_len = (alpn_data[0] << 8) | alpn_data[1];
    uint8_t * ptr = alpn_data + 2;
    uint16_t processed = 0;

    if(alpn_list_len + 2 > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid ALPN extension length");
        return -1;
    }

    while (processed < alpn_list_len) {
        uint8_t str_len = ptr[0];
        char protocol[32]; // Protocol names are usually short

        if (str_len < sizeof(protocol)) {
            memory_memcopy(ptr + 1, protocol, str_len);
            protocol[str_len] = '\0';

            if (strcmp(protocol, "h2") == 0) {
                ctx->connection_state.alpn_h2 = true;
            } else if (strcmp(protocol, "http/1.1") == 0) {
                ctx->connection_state.alpn_http11 = true;
            }
        }

        ptr += (1 + str_len);
        processed += (1 + str_len);
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_supported_versions(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    uint8_t version_count = ext_ptr[4] / 2; // Each version is 2 bytes
    uint8_t* version_list_ptr = ext_ptr + 5;

    if (version_count * 2 + 1 > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Supported Versions extension length. Expected at least %d bytes, got %d", version_count * 2 + 1, ext_len);
        return -1;
    }

    for (int i = 0; i < version_count; i++) {
        uint16_t version = (version_list_ptr[i * 2] << 8) | version_list_ptr[i * 2 + 1];
        if (version == TLS_VERSION_1_3) {
            ctx->connection_state.tls13_supported = true;
            return 0; // TLS 1.3 supported, no need to check further
        }
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_key_share_group_x25519(tls13_session_t* ctx, uint8_t* key_data, uint16_t key_len) {
    if(ctx->connection_state.selected_group == TLS_GROUP_X25519_ML_KEM768) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client offered multiple key share groups, prioritizing x25519_mlkem768 over x25519");
        return 0;
    }

    if (key_len != X25519_PUBLIC_KEY_RAW_LEN) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid X25519 public key length: %d", key_len);
        return -1;
    }

    ctx->server_state.server_key_exchange_public_key_len = X25519_PUBLIC_KEY_RAW_LEN;
    memory_free(ctx->server_state.server_key_exchange_public_key); // Free previous if any
    ctx->server_state.server_key_exchange_public_key = (uint8_t*)memory_malloc(ctx->server_state.server_key_exchange_public_key_len);

    if (!ctx->server_state.server_key_exchange_public_key) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for server key exchange public key failed");
        return -1;
    }

    uint8_t server_private_key[X25519_PRIVATE_KEY_RAW_LEN];

    if(x25519_generate_keypair(server_private_key, ctx->server_state.server_key_exchange_public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate X25519 keypair");
        return -1;
    }

    ctx->connection_state.shared_secret = (uint8_t*)memory_malloc(X25519_SHARED_SECRET_LEN);
    if (!ctx->connection_state.shared_secret) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for shared secret failed");
        return -1;
    }

    if(x25519_shared_secret(ctx->connection_state.shared_secret,
                            server_private_key,
                            key_data) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute shared secret");
        return -1;
    }

    memory_memclean(server_private_key, sizeof(server_private_key)); // Clear private key from memory

    ctx->connection_state.shared_secret_len = X25519_SHARED_SECRET_LEN; // X25519 shared secret is 32 bytes
    ctx->connection_state.selected_group = TLS_GROUP_X25519;
    PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client Key Share Group: x25519");

    return 0;
}

static int8_t tls13_parse_client_hello_extension_key_share_group_secp256r1(tls13_session_t* ctx, uint8_t* key_data, uint16_t key_len) {
    if(ctx->connection_state.selected_group == TLS_GROUP_X25519 || ctx->connection_state.selected_group == TLS_GROUP_X25519_ML_KEM768) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client offered multiple key share groups, prioritizing x25519 over secp256r1");
        return 0;
    }

    if (key_len != (ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1) || key_data[0] != 0x04) { // Uncompressed point should be 65 bytes and start with 0x04
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Secp256r1 public key format or length: %d", key_len);
        return -1;
    }

    key_data++; // Skip the 0x04 prefix

    ctx->server_state.server_key_exchange_public_key_len = ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1;
    memory_free(ctx->server_state.server_key_exchange_public_key); // Free previous if any
    ctx->server_state.server_key_exchange_public_key = (uint8_t*)memory_malloc(ctx->server_state.server_key_exchange_public_key_len);
    if (!ctx->server_state.server_key_exchange_public_key) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for server key exchange public key failed");
        return -1;
    }

    ctx->server_state.server_key_exchange_public_key[0] = 0x04; // Uncompressed point prefix

    uint8_t server_private_key[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN];

    if(ellipticcurve_secp256r1_generate_keypair(server_private_key, ctx->server_state.server_key_exchange_public_key + 1) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate Secp256r1 keypair");
        return -1;
    }

    ctx->connection_state.shared_secret = (uint8_t*)memory_malloc(ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN);
    if (!ctx->connection_state.shared_secret) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for shared secret failed");
        return -1;
    }

    if(ellipticcurve_secp256r1_shared_secret(ctx->connection_state.shared_secret,
                                             server_private_key,
                                             key_data) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute shared secret");
        return -1;
    }

    ctx->connection_state.shared_secret_len = ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN; // P-256 shared secret is 32 bytes
    ctx->connection_state.selected_group = TLS_GROUP_SECP256R1;
    PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client Key Share Group: secp256r1");

    return 0;
}

static int8_t tls13_parse_client_hello_extension_key_share_group_x25519_mlkem768(tls13_session_t* ctx, uint8_t* key_data, uint16_t key_len) {
    if(key_len != X25519_PUBLIC_KEY_RAW_LEN + MLKEM768_PUBLICKEYBYTES) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid key length for x25519_mlkem768: %d", key_len);
        return -1;
    }

    ctx->server_state.server_key_exchange_public_key_len = MLKEM768_CIPHERTEXTBYTES + X25519_PUBLIC_KEY_RAW_LEN; // Combined length of ML-KEM ciphertext and X25519 public key
    memory_free(ctx->server_state.server_key_exchange_public_key); // Free previous if any
    ctx->server_state.server_key_exchange_public_key = (uint8_t*)memory_malloc(ctx->server_state.server_key_exchange_public_key_len);

    if (!ctx->server_state.server_key_exchange_public_key) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for server key exchange public key failed");
        return -1;
    }

    ctx->connection_state.shared_secret_len = MLKEM768_SHARED_SECRET_BYTES + X25519_SHARED_SECRET_LEN; // Combined shared secret length
    ctx->connection_state.shared_secret = (uint8_t*)memory_malloc(ctx->connection_state.shared_secret_len);
    if (!ctx->connection_state.shared_secret) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for shared secret failed");
        return -1;
    }

    uint8_t* mlkem768_public_key = key_data;
    uint8_t* x25519_client_public_key = key_data + MLKEM768_PUBLICKEYBYTES;

    uint8_t* mlkem768_ciphertext = ctx->server_state.server_key_exchange_public_key;
    uint8_t* server_x25519_public_key = ctx->server_state.server_key_exchange_public_key + MLKEM768_CIPHERTEXTBYTES;

    uint8_t* mlkem768_shared_secret_part = ctx->connection_state.shared_secret;
    uint8_t* x25519_shared_secret_part = ctx->connection_state.shared_secret + MLKEM768_SHARED_SECRET_BYTES;

    // Encapsulate ML-KEM768 using client's ML-KEM public key
    mlkem768_encaps(mlkem768_ciphertext,
                    mlkem768_shared_secret_part,
                    mlkem768_public_key);


    // Generate server's X25519 key pair and compute shared secret with client's X25519 public key
    uint8_t server_private_key[X25519_PRIVATE_KEY_RAW_LEN];

    if(x25519_generate_keypair(server_private_key, server_x25519_public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate X25519 keypair");
        return -1;
    }

    if(x25519_shared_secret(x25519_shared_secret_part,
                            server_private_key,
                            x25519_client_public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute shared secret");
        return -1;
    }

    memory_memclean(server_private_key, sizeof(server_private_key)); // Clear private key from memory

    ctx->connection_state.selected_group = TLS_GROUP_X25519_ML_KEM768;

    return 0;
}

static int8_t tls13_parse_client_hello_extension_key_share(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    uint8_t * share_ptr = ext_ptr + 4;
    uint16_t total_shares_len = (share_ptr[0] << 8) | share_ptr[1];
    uint8_t * current_share = share_ptr + 2;
    uint16_t processed = 0;

    if (total_shares_len + 2 > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Key Share extension length");
        return -1;
    }

    while (processed < total_shares_len) {
        uint16_t group = (current_share[0] << 8) | current_share[1];
        uint16_t key_len = (current_share[2] << 8) | current_share[3];
        uint8_t * key_data = current_share + 4;

        if (group == TLS_GROUP_X25519) { // X25519
            if(tls13_parse_client_hello_extension_key_share_group_x25519(ctx, key_data, key_len) != 0) {
                return -1; // Error already logged in the function
            }
        } else if (group == TLS_GROUP_SECP256R1) { // Secp256r1 (P-256)
            if(tls13_parse_client_hello_extension_key_share_group_secp256r1(ctx, key_data, key_len) != 0) {
                return -1; // Error already logged in the function
            }
        } else if(group == TLS_GROUP_X25519_ML_KEM768) {
            if(tls13_parse_client_hello_extension_key_share_group_x25519_mlkem768(ctx, key_data, key_len) != 0) {
                return -1; // Error already logged in the function
            }
        } else {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Unsupported Key Share Group: 0x%04x", group);
        }

        int32_t jump = 4 + key_len;
        current_share += jump;
        processed += jump;
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_signature_algorithms(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    uint16_t sigalgs_len  = (ext_ptr[2] << 8) | ext_ptr[3];
    uint8_t* sigalgs_data = ext_ptr + 4;

    if (sigalgs_len > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Signature Algorithms extension length. Expected at least %d bytes, got %d", sigalgs_len, ext_len);
        return -1;
    }

    int32_t algorithms_len = sigalgs_len / 2 + 1; // NONE ended list

    ctx->client_state.client_supported_signature_algorithms = (tls13_signature_algorithm_t*)memory_malloc(algorithms_len * sizeof(tls13_signature_algorithm_t));
    ctx->client_state.client_supported_signature_algorithms_x509 = (x509_algorithm_t*)memory_malloc(algorithms_len * sizeof(x509_algorithm_t));

    if (!ctx->client_state.client_supported_signature_algorithms || !ctx->client_state.client_supported_signature_algorithms_x509) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for client supported signature algorithms failed");
        memory_free(ctx->client_state.client_supported_signature_algorithms);
        memory_free(ctx->client_state.client_supported_signature_algorithms_x509);
        return -1;
    }

    int32_t x509_alg_index = 0;
    for (int32_t i = 0; i < sigalgs_len; i += 2) {
        uint16_t alg = (sigalgs_data[i] << 8) | sigalgs_data[i + 1];
        ctx->client_state.client_supported_signature_algorithms[i / 2] = alg;
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client supported signature algorithm: 0x%04x", alg);

        if(alg == TLS_SIG_ALG_ED25519) {
            ctx->client_state.client_supported_signature_algorithms_x509[x509_alg_index++] = X509_ALGORITHM_ED25519;
        } else if(alg == TLS_SIG_ALG_ECDSA_SECP256R1_SHA256) {
            ctx->client_state.client_supported_signature_algorithms_x509[x509_alg_index++] = X509_ALGORITHM_ECDSA_SECP256R1_SHA256;
        } else {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Unsupported signature algorithm: 0x%04x, skipping", alg);
        }
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_supported_groups(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    uint16_t groups_len  = (ext_ptr[2] << 8) | ext_ptr[3];
    uint8_t* groups_data = ext_ptr + 4;

    if (groups_len > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Supported Groups extension length. Expected %d, got %d", groups_len, ext_len);
        return -1;
    }

    // if list contains TLS_GROUP_SECP160R1, bad it.
    // ignore 0x01XX series, they are slow we don't like them.
    for (int i = 0; i < groups_len; i += 2) {
        uint16_t group = (groups_data[i] << 8) | groups_data[i + 1];
        if(group == 0x0010) { // TLS_GROUP_SECP160R1
            groups_len -= 2;
        }
        if((group & 0xFF00) == 0x0100) {
            groups_len -= 2;
        }
    }

    ctx->client_state.client_supported_groups_len = groups_len / 2;
    ctx->client_state.client_supported_groups = (tls13_key_exchange_group_t*)memory_malloc(ctx->client_state.client_supported_groups_len * sizeof(tls13_key_exchange_group_t));
    if (!ctx->client_state.client_supported_groups) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for client supported groups failed");
        return -1;
    }
    for (int i = 0; i < groups_len; i += 2) {
        uint16_t group = (groups_data[i] << 8) | groups_data[i + 1];
        if(group == 0x0010) { // TLS_GROUP_SECP160R1
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client offered unsupported group: secp160r1, skipping");
            continue;
        }
        if((group & 0xFF00) == 0x0100) {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client offered unsupported group: 0x%04x, skipping", group);
            continue;
        }
        ctx->client_state.client_supported_groups[i / 2] = group;
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client supported group: 0x%04x", group);
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_psk_key_exchange_modes(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    uint8_t mode_count  = ext_ptr[4];
    uint8_t* modes_data = ext_ptr + 4;

    if (mode_count + 1 > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid PSK Key Exchange Modes extension length. Expected at least %d bytes, got %d", mode_count + 1, ext_len);
        return -1;
    }

    for (int i = 0; i < mode_count; i++) {
        uint8_t mode = modes_data[i];
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client offered PSK Key Exchange Mode: 0x%02x", mode);

        if(mode != TLS13_PSK_KEY_EXCHANGE_MODE_DHE_PSK) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Unsupported PSK Key Exchange Mode: 0x%02x, skipping", mode);
            continue;
        }

        ctx->connection_state.psk_key_exchange_mode = mode;
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_pre_shared_key(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len, const uint8_t* handshake) {
    uint16_t identity_list_len = (ext_ptr[4] << 8) | ext_ptr[5];
    uint8_t* identity_list_ptr = ext_ptr + 6;
    uint8_t* identity_list_end = identity_list_ptr + identity_list_len;

    uint8_t* binders_data_start = identity_list_end;
    uint16_t binders_list_len = (binders_data_start[0] << 8) | binders_data_start[1];
    uint8_t* binders_data_ptr = binders_data_start + 2;
    uint8_t* binders_data_end = binders_data_ptr + binders_list_len;

    if(!ctx->config->get_psk_encryption_keys_callback) {
        ctx->connection_state.session_resumed = false; // Can't resume without keys, but allow full handshake to proceed
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "No callback registered to retrieve PSK encryption keys");
        return 0;
    }

    if (identity_list_len + 2 + binders_list_len + 2 > ext_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Pre-Shared Key extension length. Expected at least %d bytes, got %d", identity_list_len + 2 + binders_list_len + 2, ext_len);
        return -1;
    }

    int32_t identity_count = 0;
    int32_t binder_count = 0;

    uint8_t* tmp_ptr = identity_list_ptr;
    while (tmp_ptr < identity_list_end) {
        if (tmp_ptr + 2 > identity_list_end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Pre-Shared Key identity list format");
            return -1;
        }
        uint16_t identity_len = (tmp_ptr[0] << 8) | tmp_ptr[1];
        tmp_ptr += 2 + identity_len + 4; // Move past identity and obfuscated ticket age
        identity_count++;
    }

    tmp_ptr = binders_data_ptr;
    while (tmp_ptr < binders_data_end) {
        if (tmp_ptr + 1 > binders_data_end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid Pre-Shared Key binders list format");
            return -1;
        }
        uint8_t binder_len = tmp_ptr[0];
        tmp_ptr += 1 + binder_len; // Move past binder length and binder data
        binder_count++;
    }

    if (identity_count != binder_count) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Mismatch between number of PSK identities (%d) and binders (%d)", identity_count, binder_count);
        return -1;
    }

    PRINTLOG(CRYPTOLIB, LOG_INFO, "PSK Identity Count: %d, Binder Count: %d", identity_count, binder_count);

    int32_t hash_len = ctx->connection_state.handshake_hash_len;

    uint8_t* binder_locations[binder_count];
    memory_memclean(binder_locations, sizeof(binder_locations));
    int32_t binder_lens[binder_count];
    memory_memclean(binder_lens, sizeof(binder_lens));
    tmp_ptr = binders_data_ptr;
    for (int i = 0; i < binder_count; i++) {
        uint8_t binder_len = tmp_ptr[0];
        binder_lens[i] = binder_len;
        binder_locations[i] = tmp_ptr + 1; // Point to the start of binder data
        tmp_ptr += 1 + binder_len; // Move to the next binder
    }

    int32_t psk_index = 0;

#define NEXT_IDENTITY() { \
            if (psk_index >= identity_count) { \
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "PSK index out of bounds: %d", psk_index); \
                return -1; \
            } \
            psk_index++; \
            identity_list_ptr += 2 + ((identity_list_ptr[0] << 8) | identity_list_ptr[1]) + 4; /* Move past identity and obfuscated ticket age */ \
            continue; \
}

    while (identity_list_ptr < identity_list_end) {
        uint16_t identity_len  = (identity_list_ptr[0] << 8) | identity_list_ptr[1];
        uint8_t* identity_data = identity_list_ptr + 2;

        if(binder_lens[psk_index] != hash_len) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Binder length does not match expected hash length for PSK identity %d. Expected: %d, Got: %d", psk_index, hash_len, binder_lens[psk_index]);
            NEXT_IDENTITY();
        }

        uint8_t plaintext[4096];

        int32_t ciphertext_len = identity_len - 16; // Subtract tag length

        uint8_t* psk_encryption_key;
        uint8_t* psk_encryption_iv;
        uint8_t* psk_aed_key;

        if(ctx->config->get_psk_encryption_keys_callback(ctx, false, &psk_encryption_key, &psk_encryption_iv, &psk_aed_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to retrieve PSK encryption keys for PSK identity %d", psk_index);
            NEXT_IDENTITY();
        }

        int32_t status = aes_gcm_decrypt_with_aad_with_tag(
            plaintext, identity_data, ciphertext_len,
            psk_encryption_key, AES256_KEY_SIZE,
            psk_encryption_iv, 12,
            psk_aed_key, 16,
            identity_data + ciphertext_len, 16
            );

        if (status != 0) {
            // get previous keys
            if(ctx->config->get_psk_encryption_keys_callback(ctx, true, &psk_encryption_key, &psk_encryption_iv, &psk_aed_key) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to retrieve previous PSK encryption keys for PSK identity %d", psk_index);
                NEXT_IDENTITY();
            }

            status = aes_gcm_decrypt_with_aad_with_tag(
                plaintext, identity_data, ciphertext_len,
                psk_encryption_key, AES256_KEY_SIZE,
                psk_encryption_iv, 12,
                psk_aed_key, 16,
                identity_data + ciphertext_len, 16
                );

            if (status != 0) {
                PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to decrypt PSK identity %d with both current and previous keys", psk_index);
                NEXT_IDENTITY();
            }
        }

        int32_t offset = 0;

        tls13_cipher_suite_t cipher_suite = (plaintext[offset] << 8) | plaintext[offset + 1]; // Extract cipher suite from decrypted PSK identity
        offset += 2;

        if(cipher_suite != ctx->connection_state.selected_cipher_suite) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Cipher suite mismatch between PSK identity and ClientHello: 0x%04x vs 0x%04x", cipher_suite, ctx->connection_state.selected_cipher_suite);
            NEXT_IDENTITY();
        }

        uint8_t* psk_bytes = &plaintext[offset];
        offset += hash_len;

        uint32_t lifetime_s = (plaintext[offset] << 24) | (plaintext[offset + 1] << 16) | (plaintext[offset + 2] << 8) | plaintext[offset + 3];
        offset += 4;
        uint32_t lifetime = lifetime_s * 1000; // Convert to milliseconds

        uint32_t ticket_age_add = (plaintext[offset] << 24) | (plaintext[offset + 1] << 16) | (plaintext[offset + 2] << 8) | plaintext[offset + 3];
        offset += 4;

        uint64_t now_ns_in_ticket = plaintext[offset++];
        now_ns_in_ticket <<= 8;
        now_ns_in_ticket  |= plaintext[offset++];
        now_ns_in_ticket <<= 8;
        now_ns_in_ticket  |= plaintext[offset++];
        now_ns_in_ticket <<= 8;
        now_ns_in_ticket  |= plaintext[offset++];
        now_ns_in_ticket <<= 8;
        now_ns_in_ticket  |= plaintext[offset++];
        now_ns_in_ticket <<= 8;
        now_ns_in_ticket  |= plaintext[offset++];
        now_ns_in_ticket <<= 8;
        now_ns_in_ticket  |= plaintext[offset++];
        now_ns_in_ticket <<= 8;
        now_ns_in_ticket  |= plaintext[offset++];

        uint32_t alpn_name_len = (plaintext[offset] << 8) | plaintext[offset + 1];
        offset += 2;

        char_t alpn_name[256];
        if (alpn_name_len < sizeof(alpn_name)) {
            memory_memcopy(&plaintext[offset], alpn_name, alpn_name_len);
            alpn_name[alpn_name_len] = '\0';
        } else {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "ALPN name in PSK identity too long: %d", alpn_name_len);
            NEXT_IDENTITY();
        }

        const char_t* expected_alpn = ctx->connection_state.alpn_h2 ? "h2" : (ctx->connection_state.alpn_http11 ? "http/1.1" : NULL);
        if (expected_alpn && strlen(alpn_name) == strlen(expected_alpn) && strcmp(alpn_name, expected_alpn) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "ALPN name mismatch between PSK identity and ClientHello: %s vs %s", alpn_name, expected_alpn);
            NEXT_IDENTITY();
        }

        uint32_t obfuscated_ticket_age = (identity_data[identity_len] << 24) | (identity_data[identity_len + 1] << 16) | (identity_data[identity_len + 2] << 8) | identity_data[identity_len + 3];

        uint32_t reported_age = obfuscated_ticket_age - ticket_age_add;

        if (reported_age > lifetime) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "PSK ticket has expired based on reported age. Reported age: %u ms, Lifetime: %u ms", reported_age, lifetime);
            NEXT_IDENTITY();
        }

        uint64_t now_ns = time_ns(NULL);
        uint64_t ticket_age = (now_ns - now_ns_in_ticket) / 1000000; // Convert to milliseconds

        if (ticket_age > lifetime) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "PSK ticket has expired. Ticket age: %llu ms, Lifetime: %u ms", ticket_age, lifetime);
            NEXT_IDENTITY();
        }

        uint8_t* hmac = NULL;

        uint8_t empty_hash[SHA384_OUTPUT_SIZE] = {0};
        uint8_t early_secret[SHA384_OUTPUT_SIZE];
        uint8_t binder_key[SHA384_OUTPUT_SIZE];
        uint8_t finished_key[SHA384_OUTPUT_SIZE];

        if(tls13_hash_get_empty(ctx, empty_hash) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get empty hash");
            return -1;
        }

        if(hkdf_extract(ctx, NULL, 0, psk_bytes, hash_len, early_secret) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute early secret");
            return -1;
        }

        if(hkdf_expand_label_ext(ctx, early_secret, "res binder", empty_hash, hash_len, binder_key, hash_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute binder key");
            return -1;
        }

        if(hkdf_expand_label_ext(ctx, binder_key, "finished", NULL, 0, finished_key, hash_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute finished key");
            return -1;
        }

        uint8_t handshake_hash[SHA384_OUTPUT_SIZE];

        if(tls13_hash_compute(ctx, handshake, binders_data_start - handshake, handshake_hash) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute handshake hash for PSK binder verification");
            return -1;
        }

        if(tls13_hash_hmac(ctx->connection_state.selected_hash_algorithm, finished_key, hash_len, handshake_hash, hash_len, &hmac) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute PSK binder HMAC");
            return -1;
        }

        int32_t diff = 0;
        for (int i = 0; i < hash_len; i++) {
            diff |= (hmac[i] ^ binder_locations[psk_index][i]);
        }

        memory_free(hmac); // Free the HMAC result after use

        if(diff == 0) {
            ctx->connection_state.session_resumed = true; // Mark session as resumed based on valid PSK binder
            ctx->connection_state.selected_identity_index = psk_index; // Store the index of the selected PSK identity
            memory_memcopy(psk_bytes, ctx->connection_state.selected_psk_value, hash_len); // Store the selected PSK identity for later use
            break; // Stop after the first valid binder is found
        } else {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "PSK binder verification failed for identity index %d", psk_index);
            NEXT_IDENTITY();
        }

        NEXT_IDENTITY();
    }

    return 0;
}

static int8_t tls13_parse_client_hello_extension_post_handshake_auth(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    UNUSED(ctx);
    UNUSED(ext_ptr);
    UNUSED(ext_len);
    // Not implemented, reserved for future use
    PRINTLOG(CRYPTOLIB, LOG_WARNING, "Received Post-Handshake Authentication extension, but it's not implemented");
    return 0;
}

static int8_t tls13_parse_client_hello_extension_status_request(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    UNUSED(ctx);
    UNUSED(ext_ptr);
    UNUSED(ext_len);
    // Not implemented, reserved for future use
    PRINTLOG(CRYPTOLIB, LOG_WARNING, "Received Status Request extension, but it's not implemented");
    return 0;
}

static int8_t tls13_parse_client_hello_extension_signed_certificate_timestamp(tls13_session_t* ctx, uint8_t* ext_ptr, uint16_t ext_len) {
    UNUSED(ctx);
    UNUSED(ext_ptr);
    UNUSED(ext_len);
    // Not implemented, reserved for future use
    PRINTLOG(CRYPTOLIB, LOG_WARNING, "Received Signed Certificate Timestamp extension, but it's not implemented");
    return 0;
}

static int8_t tls13_process_client_hello(tls13_session_t* ctx) {
    if(!ctx) {
        return -1;
    }

    uint8_t header[5];

    int32_t received = ctx->config->network_recv(ctx->connection_state.network_client_identifier, header, 5, 0);

    if(received != 5) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to receive TLS record header");
        return -1;
    }

    int8_t check = tls13_check_plain_text_protcol(ctx, header);
    if(check < 0) {
        return check; // -1 for error, -2 for handled HTTP request
    }

    int32_t record_len = (header[3] << 8) | header[4];

    uint8_t* buffer = memory_malloc(record_len);
    if(!buffer) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for TLS record failed");
        return -1;
    }

    received = ctx->config->network_recv(ctx->connection_state.network_client_identifier, buffer, record_len, 0);

    if(received != record_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to receive complete TLS record");
        memory_free(buffer);
        return -1;
    }

    int32_t offset = 0;

    // 2. Move to Handshake Layer (Offset 5)
    uint8_t * handshake = buffer;
    uint8_t msg_type = handshake[0];
    if (msg_type != TLS13_HANDSHAKE_TYPE_CLIENT_HELLO) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Not a Client Hello (Type: 0x%02x)", msg_type);
        memory_free(buffer);
        return -1;
    }

    // 3. Skip Handshake header (1 byte type + 3 bytes length = 4 bytes)
    // Client Version (2 bytes)
    uint16_t client_version = (handshake[4] << 8) | handshake[5];
    ctx->connection_state.version = client_version;

    offset = 6; // Start of Client Random

    // 4. Client Random (32 bytes)
    uint8_t* client_random = &handshake[offset];
    memory_memcopy(client_random, ctx->client_state.client_random, 32);

    offset += 32; // Move past Client Random

    // 5. Session ID (Variable length)
    uint8_t session_id_len = handshake[offset++];
    uint8_t* session_id = &handshake[offset];
    ctx->connection_state.session_id_len = session_id_len;
    if (session_id_len > 0) {
        ctx->connection_state.session_id = (uint8_t*)memory_malloc(session_id_len);
        if (!ctx->connection_state.session_id) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation for session_id failed");
            memory_free(buffer);
            return -1;
        }
        memory_memcopy(session_id, ctx->connection_state.session_id, session_id_len);
    } else {
        ctx->connection_state.session_id = NULL;
    }

    offset += session_id_len; // Move past Session ID


    // 6. Cipher Suites (Variable length)
    // The offset depends on session_id_len
    uint16_t cipher_suites_len = (handshake[offset] << 8) | handshake[offset + 1];
    uint8_t* cipher_suites = &handshake[offset + 2];

    if (tls13_parse_client_hello_cipher_suites(ctx, cipher_suites, cipher_suites_len) < 0) {
        memory_free(buffer);
        return -1; // No supported cipher suites found or error in parsing
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
        tls13_extension_type_t ext_type = (ext_ptr[0] << 8) | ext_ptr[1];
        uint16_t ext_len = (ext_ptr[2] << 8) | ext_ptr[3];

        int8_t ext_res = 0;

        switch(ext_type) {
        case TLS_EXTENSION_SNI:
            ext_res = tls13_parse_client_hello_extension_sni(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_ALPN:
            ext_res = tls13_parse_client_hello_extension_alpn(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_SUPPORTED_VERSIONS:
            ext_res = tls13_parse_client_hello_extension_supported_versions(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_KEY_SHARE:
            ext_res = tls13_parse_client_hello_extension_key_share(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_SIGNATURE_ALGORITHMS:
            ext_res = tls13_parse_client_hello_extension_signature_algorithms(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_SUPPORTED_GROUPS:
            ext_res = tls13_parse_client_hello_extension_supported_groups(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_PSK_KEY_EXCHANGE_MODES:
            ext_res = tls13_parse_client_hello_extension_psk_key_exchange_modes(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_PRE_SHARED_KEY:
            ext_res = tls13_parse_client_hello_extension_pre_shared_key(ctx, ext_ptr, ext_len, handshake);
            break;
        case TLS_EXTENSION_POST_HANDSHAKE_AUTH:
            ext_res = tls13_parse_client_hello_extension_post_handshake_auth(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_STATUS_REQUEST:
            ext_res = tls13_parse_client_hello_extension_status_request(ctx, ext_ptr, ext_len);
            break;
        case TLS_EXTENSION_SIGNED_CERTIFICATE_TIMESTAMP:
            ext_res = tls13_parse_client_hello_extension_signed_certificate_timestamp(ctx, ext_ptr, ext_len);
            break;
        default:
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Skipping unsupported extension type: 0x%04x", ext_type);
            break;
        }

        if (ext_res < 0) {
            memory_free(buffer);
            return -1; // Error parsing extension, already logged
        }

        ext_ptr += 4 + ext_len;
        parsed_len += 4 + ext_len;
    }

    if (parsed_len != extensions_total_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Extensions length mismatch");
        memory_free(buffer);
        return -1;
    }

    if (!ctx->connection_state.tls13_supported) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client does not support TLS 1.3");
        memory_free(buffer);
        return -1;
    }

    if (ctx->connection_state.selected_group == TLS_GROUP_NONE) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No supported key exchange group found");
        memory_free(buffer);
        return -1;
    }

    boolean_t selected_group_supported_by_client = false;
    for (size_t i = 0; i < ctx->client_state.client_supported_groups_len; i++) {
        if (ctx->client_state.client_supported_groups[i] == ctx->connection_state.selected_group) {
            selected_group_supported_by_client = true;
            break;
        }
    }
    if (!selected_group_supported_by_client) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Selected key exchange group not supported by client");
        memory_free(buffer);
        return -1;
    }

    if(ctx->connection_state.selected_hash_algorithm == TLS_HASH_NONE) {
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

static int32_t tls13_send_server_hello(tls13_session_t* ctx) {
    uint8_t msg[4096];
    int32_t p = 5; // Start after Record Header

    // Handshake Type & Placeholder for Length
    msg[p++] = TLS13_HANDSHAKE_TYPE_SERVER_HELLO;
    int32_t hs_len_ptr = p;
    p += 3;

    // Legacy Version
    msg[p++] = 0x03; msg[p++] = 0x03;

    // Server Random
    memory_memcopy(ctx->server_state.server_random, &msg[p], 32);
    p += 32;

    // Echo Session ID
    msg[p++] = ctx->connection_state.session_id_len;
    if (ctx->connection_state.session_id_len > 0) {
        memory_memcopy(ctx->connection_state.session_id, &msg[p], ctx->connection_state.session_id_len);
        p += ctx->connection_state.session_id_len;
    }

    // Selected Cipher Suite
    msg[p++] = (ctx->connection_state.selected_cipher_suite >> 8) & 0xFF;
    msg[p++] = ctx->connection_state.selected_cipher_suite & 0xFF;

    // Compression Method (null)
    msg[p++] = 0x00;

    // Extensions
    int32_t ext_len_ptr = p;
    p += 2;

    // Extension: Supported Versions (0x002b)
    msg[p++] = 0x00; msg[p++] = 0x2b;
    msg[p++] = 0x00; msg[p++] = 0x02;
    msg[p++] = 0x03; msg[p++] = 0x04; // TLS 1.3

    size_t key_len = ctx->server_state.server_key_exchange_public_key_len;
    size_t key_len_placeholder = 4 + key_len; // 2 bytes for group + 2 bytes for key length + key data
    uint8_t* key_data = ctx->server_state.server_key_exchange_public_key;

    // Extension: Key Share (0x0033)
    msg[p++] = 0x00; msg[p++] = 0x33;
    msg[p++] = (key_len_placeholder >> 8) & 0xFF;
    msg[p++] = key_len_placeholder & 0xFF;
    // Key Share Group
    msg[p++] = ((ctx->connection_state.selected_group >> 8) & 0xFF);
    msg[p++] = (ctx->connection_state.selected_group & 0xFF);
    // Key Length
    msg[p++] = ((key_len >> 8) & 0xFF);
    msg[p++] = (key_len & 0xFF);
    // Key Data
    memory_memcopy(key_data, &msg[p], key_len);
    p += key_len;

    if(ctx->connection_state.session_resumed) {
        msg[p++] = (TLS_EXTENSION_PRE_SHARED_KEY >> 8) & 0xFF;
        msg[p++] = TLS_EXTENSION_PRE_SHARED_KEY & 0xFF;
        msg[p++] = 0x00; msg[p++] = 0x02; // Extension length
        msg[p++] = (ctx->connection_state.selected_identity_index >> 8) & 0xFF;
        msg[p++] = ctx->connection_state.selected_identity_index & 0xFF;
    }

    // Fix up Lengths
    uint32_t hs_body_len = p - hs_len_ptr - 3;
    msg[hs_len_ptr] = (hs_body_len >> 16) & 0xFF;
    msg[hs_len_ptr + 1] = (hs_body_len >> 8) & 0xFF;
    msg[hs_len_ptr + 2] = hs_body_len & 0xFF;

    uint32_t ext_total_len = p - ext_len_ptr - 2;
    msg[ext_len_ptr] = (ext_total_len >> 8) & 0xFF;
    msg[ext_len_ptr + 1] = ext_total_len & 0xFF;

    // Fix Record Header
    msg[0] = TLS13_CONTENT_TYPE_HANDSHAKE;
    msg[1] = 0x03; msg[2] = 0x03;
    uint16_t rec_len = p - 5;
    msg[3] = (rec_len >> 8) & 0xFF;
    msg[4] = rec_len & 0xFF;

    if(tls13_hash_update(ctx, msg + 5, p - 5) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update handshake hash with Server Hello");
        return -1;
    }

    return ctx->config->network_send(ctx->connection_state.network_client_identifier, msg, p, 0);
}

static int8_t tls13_generate_handshake_key_and_iv(tls13_session_t* ctx) {
    uint32_t hlen = ctx->connection_state.handshake_hash_len;
    uint32_t key_len = ctx->connection_state.handshake_key_len;
    uint32_t iv_len  = ctx->connection_state.handshake_iv_len;

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

    // sizes are enough for SHA-256, SHA-384 we dont implemented SHA-512 ciphersuites yet so we dont need 64 bytes here.
    uint8_t zero_ikm[SHA384_OUTPUT_SIZE] = {0};
    uint8_t early_secret[SHA384_OUTPUT_SIZE], derived_early[SHA384_OUTPUT_SIZE],
            handshake_secret[SHA384_OUTPUT_SIZE], s_hs_traffic_secret[SHA384_OUTPUT_SIZE],
            c_hs_traffic_secret[SHA384_OUTPUT_SIZE];

    // 1. Early Secret
    if(ctx->connection_state.session_resumed) {
        // If resuming, use the selected PSK as the IKM for early secret
        if(hkdf_extract(ctx, NULL, 0, ctx->connection_state.selected_psk_value, hlen, early_secret) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive early secret from PSK");
            return -1;
        }
    } else {
        if(hkdf_extract(ctx, NULL, 0, zero_ikm, hlen, early_secret) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive early secret");
            return -1;
        }
    }

    // 2. Derived Secret
    if(hkdf_expand_label_ext(ctx, early_secret, "derived", (uint8_t*)empty_hash, hlen, derived_early, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive derived early secret");
        return -1;
    }

    // 3. Handshake Secret
    if(hkdf_extract(ctx, derived_early, hlen, ctx->connection_state.shared_secret, ctx->connection_state.shared_secret_len, handshake_secret) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive handshake secret");
        return -1;
    }

    // 4a. Server Handshake Traffic Secret
    if(hkdf_expand_label_ext(ctx, handshake_secret, "s hs traffic", current_hash, hlen, s_hs_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server handshake traffic secret");
        return -1;
    }
    memory_memcopy(s_hs_traffic_secret, ctx->server_state.server_handshake_traffic_secret, hlen);

    // 4b. Client Handshake Traffic Secret (Uses the same current_hash)
    if(hkdf_expand_label_ext(ctx, handshake_secret, "c hs traffic", current_hash, hlen, c_hs_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client handshake traffic secret");
        return -1;
    }
    memory_memcopy(c_hs_traffic_secret, ctx->client_state.client_handshake_traffic_secret, hlen);

    // 5. SERVER HANDSHAKE KEYS
    if(hkdf_expand_label_ext(ctx, s_hs_traffic_secret, "key", NULL, 0, ctx->server_state.server_handshake_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server handshake key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, s_hs_traffic_secret, "iv", NULL, 0, ctx->server_state.server_handshake_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server handshake IV");
        return -1;
    }

    // 6. CLIENT HANDSHAKE KEYS (Used to decrypt the Client Finished message)
    if(hkdf_expand_label_ext(ctx, c_hs_traffic_secret, "key", NULL, 0, ctx->client_state.client_handshake_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client handshake key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, c_hs_traffic_secret, "iv", NULL, 0, ctx->client_state.client_handshake_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client handshake IV");
        return -1;
    }

    // 7. Finished Keys
    if(hkdf_expand_label_ext(ctx, ctx->server_state.server_handshake_traffic_secret, "finished", NULL, 0, ctx->server_state.server_finished_key, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server finished key");
        return -1;
    }

    if(hkdf_expand_label_ext(ctx, ctx->client_state.client_handshake_traffic_secret, "finished", NULL, 0, ctx->client_state.client_finished_key, hlen) != 0) {
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

    memory_memcopy(master_secret, ctx->connection_state.master_secret, hlen);

    return 0;
}

static int8_t tls13_send_encrypted_extensions(tls13_session_t* ctx) {
    uint8_t plaintext[4096]; // Increased slightly for safety
    uint8_t ciphertext[4096 + 16];

    // We start reverse filling from the end of the DATA part,
    // leaving 1 byte for the Inner Content Type (0x16)
    int32_t reverse_p = 4000;
    int32_t start_pos = reverse_p;

    /* --- ALPN Extension (Reverse) --- */
    if(ctx->connection_state.has_alpn) {
        int32_t alpn_extension_end = reverse_p;

        const char* alpn_selected = ctx->connection_state.alpn_h2 ? "h2" : "http/1.1";
        int name_len = strlen(alpn_selected);

        // 1. The actual string
        reverse_p -= name_len;
        memory_memcopy(alpn_selected, &plaintext[reverse_p], name_len);

        // 2. Protocol name length (1 byte)
        plaintext[--reverse_p] = (uint8_t)name_len;

        // 3. Protocol List Length (2 bytes)
        uint16_t list_len = alpn_extension_end - reverse_p;
        plaintext[--reverse_p] = list_len & 0xff;
        plaintext[--reverse_p] = (list_len >> 8) & 0xff;

        // 4. Extension Type (0x0010) and Extension Length
        uint16_t this_ext_data_len = alpn_extension_end - reverse_p;
        plaintext[--reverse_p] = this_ext_data_len & 0xff;
        plaintext[--reverse_p] = (this_ext_data_len >> 8) & 0xff;

        plaintext[--reverse_p] = TLS_EXTENSION_ALPN & 0xff;
        plaintext[--reverse_p] = (TLS_EXTENSION_ALPN >> 8) & 0xff;
    }

    // Suported groups
    int32_t supported_groups_list_end = reverse_p;
    plaintext[--reverse_p] = TLS_GROUP_SECP256R1 & 0xff;
    plaintext[--reverse_p] = (TLS_GROUP_SECP256R1 >> 8) & 0xff;
    plaintext[--reverse_p] = TLS_GROUP_X25519 & 0xff;
    plaintext[--reverse_p] = (TLS_GROUP_X25519 >> 8) & 0xff;

    int32_t supported_groups_list_len = supported_groups_list_end - reverse_p;
    plaintext[--reverse_p] = supported_groups_list_len & 0xff;
    plaintext[--reverse_p] = (supported_groups_list_len >> 8) & 0xff;

    int32_t supported_groups_ext_data_len = supported_groups_list_end - reverse_p;
    plaintext[--reverse_p] = supported_groups_ext_data_len & 0xff;
    plaintext[--reverse_p] = (supported_groups_ext_data_len >> 8) & 0xff;

    plaintext[--reverse_p] = TLS_EXTENSION_SUPPORTED_GROUPS & 0xff;
    plaintext[--reverse_p] = (TLS_EXTENSION_SUPPORTED_GROUPS >> 8) & 0xff;


    /* Extensions length (2 bytes) */
    int32_t ext_len = start_pos - reverse_p;
    plaintext[--reverse_p] = ext_len & 0xff;
    plaintext[--reverse_p] = (ext_len >> 8) & 0xff;

    /* --- Handshake Header --- */
    uint32_t handshake_body_len = start_pos - reverse_p;
    plaintext[--reverse_p] = (handshake_body_len) & 0xff;
    plaintext[--reverse_p] = (handshake_body_len >> 8) & 0xff;
    plaintext[--reverse_p] = (handshake_body_len >> 16) & 0xff;

    // Type: Encrypted Extensions (0x08)
    plaintext[--reverse_p] = TLS13_HANDSHAKE_TYPE_ENCRYPTED_EXTENSIONS;

    /* --- Calculation for AEAD --- */
    // 'len' is the total bytes of the handshake message
    int32_t handshake_total_len = (start_pos - reverse_p);
    uint8_t* handshake_start = &plaintext[reverse_p];

    // Update Transcript Hash
    tls13_hash_update(ctx, handshake_start, handshake_total_len);

    // --- Content Type (Inner) ---
    // The 0x16 byte MUST immediately follow the handshake data
    plaintext[start_pos] = TLS13_CONTENT_TYPE_HANDSHAKE;
    int32_t aead_plaintext_len = handshake_total_len + 1;

    /* --- Nonce and AAD --- */
    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_state.server_handshake_iv, ctx->server_state.write_seq_num, nonce);

    uint16_t encrypted_record_len = aead_plaintext_len + 16;
    uint8_t aad[5] = {
        TLS13_CONTENT_TYPE_APPLICATION_DATA,
        0x03, 0x03,
        (encrypted_record_len >> 8), (encrypted_record_len & 0xff)
    };

    /* --- Encrypt --- */
    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        ciphertext,
        handshake_start, aead_plaintext_len, // Encrypt Handshake + 0x16
        ctx->server_state.server_handshake_key, ctx->connection_state.handshake_key_len,
        nonce, 12, aad, 5,
        ciphertext + aead_plaintext_len, 16);

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS Encryption failed");
        return -1;
    }

    /* --- Send record --- */
    if (ctx->config->network_send(ctx->connection_state.network_client_identifier, aad, 5, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send TLS record header");
        return -1;
    }

    if (ctx->config->network_send(ctx->connection_state.network_client_identifier, ciphertext, encrypted_record_len, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send encrypted extensions");
        return -1;
    }

    ctx->server_state.write_seq_num++;
    return 0;
}

static int8_t tls13_send_certificate_request(tls13_session_t* ctx) {
    if(ctx->connection_state.session_resumed) {
        // No need to request certificate if session is resumed
        return 0;
    }

    uint8_t** ca_dn_list = NULL;
    size_t* ca_dn_len_list = NULL;
    size_t ca_count = 0;

    size_t msg_predicted_len = 128; // Initial estimate, will adjust if CA list is provided

    if(ctx->config->client_certificates_ca_dn_list_callback) {
        if(ctx->config->client_certificates_ca_dn_list_callback(ctx, &ca_dn_list, &ca_dn_len_list, &ca_count) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get CA DN list for CertificateRequest");
            return -1;
        }
    }

    if(ca_count > 0) {
        msg_predicted_len +=  6; // extension header (2 bytes type + 2 bytes length) + CA list length (2 bytes)
        // Each CA DN entry has 2 bytes for length + actual DN
        msg_predicted_len += 2 * ca_count; // For lengths
        for(size_t i = 0; i < ca_count; i++) {
            msg_predicted_len += ca_dn_len_list[i];
        }
    }

    uint8_t plaintext[msg_predicted_len];
    uint8_t ciphertext[msg_predicted_len + 16];

    int32_t reverse_p = msg_predicted_len - 28; // Start offset
    int32_t start_pos = reverse_p;

    if(ca_count > 0) {
        // --- Extensions: certificate_authorities (Reverse) ---
        int32_t ca_list_end = reverse_p;

        for(size_t i = 0; i < ca_count; i++) {
            // CA DN (2 bytes length + actual DN)
            uint16_t dn_len = ca_dn_len_list[i];
            reverse_p -= dn_len;
            memory_memcopy(ca_dn_list[i], &plaintext[reverse_p], dn_len);

            // Length prefix for this CA DN
            plaintext[--reverse_p] = dn_len & 0xff;
            plaintext[--reverse_p] = (dn_len >> 8) & 0xff;

            memory_free(ca_dn_list[i]); // Free each CA DN after copying
        }

        memory_free(ca_dn_list); // Free the list of CA DN pointers
        memory_free(ca_dn_len_list); // Free the list of CA DN lengths

        int32_t ca_list_start = reverse_p;

        int32_t ca_list_len = ca_list_end - ca_list_start;

        // 2. CA List Length (2 bytes)
        plaintext[--reverse_p] = ca_list_len & 0xff;
        plaintext[--reverse_p] = (ca_list_len >> 8) & 0xff;

        // 3. Extension Data Length (same as list length + 2: 0x0004)
        uint16_t ext_data_len = 2 + ca_list_len; // 2 bytes for list length + actual list
        plaintext[--reverse_p] = ext_data_len & 0xff;
        plaintext[--reverse_p] = (ext_data_len >> 8) & 0xff;

        // 4. Extension Type: certificate_authorities (0x000b)
        plaintext[--reverse_p] = TLS_EXTENSION_CERTIFICATE_AUTHORITIES & 0xff;
        plaintext[--reverse_p] = (TLS_EXTENSION_CERTIFICATE_AUTHORITIES >> 8) & 0xff;
    }

    // --- Extensions: signature_algorithms (Reverse) ---
    int32_t alg_list_end = reverse_p;
    // secp256r1 (0x0403)
    plaintext[--reverse_p] = TLS_SIG_ALG_ECDSA_SECP256R1_SHA256 & 0xff;
    plaintext[--reverse_p] = (TLS_SIG_ALG_ECDSA_SECP256R1_SHA256 >> 8) & 0xff;
    // ed25519 (0x0807)
    plaintext[--reverse_p] = TLS_SIG_ALG_ED25519 & 0xff;
    plaintext[--reverse_p] = (TLS_SIG_ALG_ED25519 >> 8) & 0xff;

    int32_t alg_list_start = reverse_p;

    int32_t alg_list_len = alg_list_end - alg_list_start;

    // 2. The Algorithm List Length (2 bytes: 0x0002)
    plaintext[--reverse_p] = alg_list_len & 0xff;
    plaintext[--reverse_p] = (alg_list_len >> 8) & 0xff;

    // 3. Extension Data Length (same as list length + 2: 0x0004)
    // Actually, it's just the list length here: 0x0004
    uint16_t ext_data_len = 2 + alg_list_len; // 2 bytes for list length + actual list
    plaintext[--reverse_p] = (uint8_t)(ext_data_len & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((ext_data_len >> 8) & 0xFF);

    // 4. Extension Type: signature_algorithms (0x000d)
    plaintext[--reverse_p] = TLS_EXTENSION_SIGNATURE_ALGORITHMS & 0xff;
    plaintext[--reverse_p] = (TLS_EXTENSION_SIGNATURE_ALGORITHMS >> 8) & 0xff;

    // --- Extensions: signature_algorithms_cert (Reverse) ---
    int32_t cert_alg_list_end = reverse_p;
    // secp256r1 (0x0403)
    plaintext[--reverse_p] = TLS_SIG_ALG_ECDSA_SECP256R1_SHA256 & 0xff;
    plaintext[--reverse_p] = (TLS_SIG_ALG_ECDSA_SECP256R1_SHA256 >> 8) & 0xff;
    // ed25519 (0x0807)
    plaintext[--reverse_p] = TLS_SIG_ALG_ED25519 & 0xff;
    plaintext[--reverse_p] = (TLS_SIG_ALG_ED25519 >> 8) & 0xff;

    int32_t cert_alg_list_start = reverse_p;

    int32_t cert_alg_list_len = cert_alg_list_end - cert_alg_list_start;

    // 2. The Algorithm List Length (2 bytes: 0x0002)
    plaintext[--reverse_p] = cert_alg_list_len & 0xff;
    plaintext[--reverse_p] = (cert_alg_list_len >> 8) & 0xff;

    // 3. Extension Data Length (same as list length + 2: 0x0004)
    // Actually, it's just the list length here: 0x0004
    uint16_t cert_ext_data_len = 2 + cert_alg_list_len; // 2 bytes for list length + actual list
    plaintext[--reverse_p] = (uint8_t)(cert_ext_data_len & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((cert_ext_data_len >> 8) & 0xFF);

    // 4. Extension Type: signature_algorithms_cert (0x0032)
    plaintext[--reverse_p] = TLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT & 0xff;
    plaintext[--reverse_p] = (TLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT >> 8) & 0xff;

    // --- Handshake Body (Reverse) ---
    uint16_t extensions_vec_len = start_pos - reverse_p;

    // 5. Extensions Vector Length (2 bytes)
    plaintext[--reverse_p] = (uint8_t)(extensions_vec_len & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((extensions_vec_len >> 8) & 0xFF);

    // 6. Request Context Length (0x00 for handshake)
    plaintext[--reverse_p] = 0x00;

    // --- Handshake Header (Reverse) ---
    uint32_t handshake_body_len = start_pos - reverse_p;

    // 7. Handshake Length (3 bytes: Big Endian)
    plaintext[--reverse_p] = (uint8_t)(handshake_body_len & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((handshake_body_len >> 8) & 0xFF);
    plaintext[--reverse_p] = (uint8_t)((handshake_body_len >> 16) & 0xFF);

    // 8. Handshake Type: CertificateRequest (0x0d)
    plaintext[--reverse_p] = TLS13_HANDSHAKE_TYPE_CERTIFICATE_REQUEST;

    /* --- Handshake calculation and Encryption --- */
    int32_t handshake_total_len = (start_pos - reverse_p);
    uint8_t* handshake_start = &plaintext[reverse_p];

    // Update Transcript
    tls13_hash_update(ctx, handshake_start, handshake_total_len);

    // Append Inner Content Type
    plaintext[start_pos] = TLS13_CONTENT_TYPE_HANDSHAKE;
    int32_t aead_plaintext_len = handshake_total_len + 1;

    /* --- Nonce and AAD --- */
    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_state.server_handshake_iv, ctx->server_state.write_seq_num, nonce);

    uint16_t encrypted_record_len = aead_plaintext_len + 16;
    uint8_t aad[5] = {
        TLS13_CONTENT_TYPE_APPLICATION_DATA,
        0x03, 0x03,
        (encrypted_record_len >> 8), (encrypted_record_len & 0xff)
    };

    /* --- Encrypt --- */
    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        ciphertext,
        handshake_start, aead_plaintext_len, // Encrypt Handshake + 0x16
        ctx->server_state.server_handshake_key, ctx->connection_state.handshake_key_len,
        nonce, 12, aad, 5,
        ciphertext + aead_plaintext_len, 16);

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS Encryption failed");
        return -1;
    }

    /* --- Send record --- */
    if (ctx->config->network_send(ctx->connection_state.network_client_identifier, aad, 5, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send TLS record header");
        return -1;
    }

    if (ctx->config->network_send(ctx->connection_state.network_client_identifier, ciphertext, encrypted_record_len, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send encrypted extensions");
        return -1;
    }

    ctx->server_state.write_seq_num++;

    return 0;
}

static int8_t tls13_send_certificate_and_verify(tls13_session_t* ctx) {
    if(ctx->connection_state.session_resumed) {
        // No need to send certificate if session is resumed
        return 0;
    }

    if(!ctx->config->load_server_certificate_and_key) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load server certificate and key");
        return -1;
    }

    x509_certificate_t* ca_certificate = NULL;
    x509_certificate_t* server_certificate = NULL;
    uint8_t* server_private_key = NULL;
    size_t server_private_key_len = 0;

    if(ctx->config->load_server_certificate_and_key(ctx,
                                                    ctx->client_state.client_supported_signature_algorithms_x509,
                                                    &ca_certificate,
                                                    &server_certificate,
                                                    &server_private_key,
                                                    &server_private_key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load server certificate and key");
        return -1;
    }

    x509_algorithm_t cert_alg = x509_certificate_get_public_key_algorithm(server_certificate);

    // We need the raw DER for both
    size_t server_der_len = 0;
    uint8_t* server_der = x509_certificate_get_der(server_certificate, &server_der_len);

    size_t ca_der_len = 0;
    uint8_t* ca_der = x509_certificate_get_der(ca_certificate, &ca_der_len);

    x509_certificate_free(server_certificate);

    if (!server_der || !ca_der) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get DER encoding of certificates server %d ca %d",
                 server_der ? 1 : 0, ca_der ? 1 : 0);
        memory_free(server_der);
        memory_free(ca_der);
        memory_free(server_private_key);
        return -1;
    }

    size_t total_cert_len = 3 + server_der_len + 2 + 3 + ca_der_len + 2;
    size_t estimated_hs_len = 1 + 3 + 1 + 3 + total_cert_len;
    estimated_hs_len += 4096 - (estimated_hs_len % 4096); // Padding for safety

    uint8_t* plaintext  = (uint8_t*)memory_malloc(estimated_hs_len);
    uint8_t* ciphertext = (uint8_t*)memory_malloc(estimated_hs_len + 16);

    if (!plaintext || !ciphertext) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Certificate message");
        memory_free(plaintext);
        memory_free(ciphertext);
        memory_free(server_der);
        memory_free(ca_der);
        memory_free(server_private_key);
        return -1;
    }

    int32_t p = 0;

    /* --- Build Handshake Body --- */
    plaintext[p++] = TLS13_HANDSHAKE_TYPE_CERTIFICATE; // Type: Certificate
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
    plaintext[p++] = TLS13_CONTENT_TYPE_HANDSHAKE; // Inner Type: Handshake

    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_state.server_handshake_iv, ctx->server_state.write_seq_num, nonce);

    size_t key_len = ctx->connection_state.handshake_key_len;
    uint16_t encrypted_len = p + 16;

    uint8_t aad[5] = {
        TLS13_CONTENT_TYPE_APPLICATION_DATA,
        0x03, 0x03,
        (encrypted_len >> 8), (encrypted_len & 0xFF)
    };

    aes_gcm_encrypt_with_aad_with_tag(ciphertext, plaintext, p,
                                      ctx->server_state.server_handshake_key, key_len,
                                      nonce, 12, aad, 5, ciphertext + p, 16);

    ctx->config->network_send(ctx->connection_state.network_client_identifier, aad, 5, 0);
    ctx->config->network_send(ctx->connection_state.network_client_identifier, ciphertext, encrypted_len, 0);

    ctx->server_state.write_seq_num++;

    memory_free(plaintext);
    memory_free(ciphertext);

    const size_t space_count  = 64;
    const char_t* sign_string = "TLS 1.3, server CertificateVerify";
    uint8_t sign_buffer[space_count + strlen(sign_string) + 1 + SHA384_OUTPUT_SIZE];
    // 1. Construct the buffer to be signed
    memory_memset(sign_buffer, 0x20, space_count); // 64 spaces
    memory_memcopy(sign_string, sign_buffer + space_count, strlen(sign_string));
    sign_buffer[space_count + strlen(sign_string)] = 0x00; // Null terminator

    // Get the current snapshot of the handshake hash
    uint32_t hlen = ctx->connection_state.handshake_hash_len;

    // Note: This must include the Certificate message bytes!
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for CertificateVerify");
        memory_free(server_private_key);
        return -1;
    }

    memory_memcopy(current_hash, sign_buffer + space_count + strlen(sign_string) + 1, hlen);

    // 2. Sign the buffer
    size_t sign_buffer_len = space_count + strlen(sign_string) + 1 + hlen;
    uint16_t signature_len = 0;
    tls13_signature_algorithm_t sig_alg = TLS_SIG_ALG_NONE;
    uint8_t* signature = NULL;

    if (cert_alg == X509_ALGORITHM_ED25519) {
        sig_alg = TLS_SIG_ALG_ED25519;
        signature_len = 64;
        signature = (uint8_t*)memory_malloc(signature_len);

        if (!signature) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for signature");
            memory_free(server_private_key);
            return -1;
        }

        if (ed25519_sign(signature, sign_buffer, sign_buffer_len,
                         server_private_key) != 0) {
            memory_free(signature);
            memory_free(server_private_key);
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Ed25519 signing failed");
            return -1;
        }

        memory_free(server_private_key);
    } else if(cert_alg == X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
        sig_alg = TLS_SIG_ALG_ECDSA_SECP256R1_SHA256;
        signature_len = ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN;
        signature = (uint8_t*)memory_malloc(signature_len);

        if (!signature) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for signature");
            return -1;
        }

        if(ellipticcurve_secp256r1_sign(signature, sign_buffer, sign_buffer_len,
                                        server_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "ECDSA P-256 signing failed");
            memory_free(signature);
            memory_free(server_private_key);
            return -1;
        }

        memory_free(server_private_key);

        size_t der_signature_len = 0;
        uint8_t* der_signature = ellipticcurve_secp256r1_encode_signature(signature, &der_signature_len);
        memory_free(signature);

        if(!der_signature) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "ECDSA P-256 sign to der sign failed");
            return -1;
        }

        signature = der_signature;
        signature_len = der_signature_len;
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported certificate public key algorithm for signing: %d", cert_alg);
        return -1;
    }

    plaintext = memory_malloc(4 + 2 + 2 + signature_len); // Handshake Header (4) + Sig Alg (2) + Sig Len (2) + Signature

    if (!plaintext) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for CertificateVerify message");
        memory_free(signature);
        return -1;
    }

    p = 0;

    /* --- Build Handshake Message --- */
    plaintext[p++] = TLS13_HANDSHAKE_TYPE_CERTIFICATE_VERIFY; // Type: Certificate Verify
    // Length: 2 (Algorithm) + 2 (Sig Len) + signature_len (Signature)
    uint16_t hs_body_len = 2 + 2 + signature_len;
    plaintext[p++] = ((hs_body_len >> 16) & 0xFF);
    plaintext[p++] = ((hs_body_len >> 8) & 0xFF);
    plaintext[p++] = (hs_body_len & 0xFF);

    // Algorithm: 2 bytes
    plaintext[p++] = ((sig_alg >> 8) & 0xFF);
    plaintext[p++] = (sig_alg & 0xFF);

    // Signature Length: signature_len bytes
    plaintext[p++] = ((signature_len >> 8) & 0xFF);
    plaintext[p++] = (signature_len & 0xFF);

    // Signature bytes
    memory_memcopy(signature, &plaintext[p], signature_len);
    p += signature_len;
    memory_free(signature);

    /* --- Finalize Transcript and Send --- */
    // 1. Update hash with the Certificate Verify message (p bytes)
    tls13_hash_update(ctx, plaintext, p);

    // 2. Wrap in encrypted record
    plaintext[p++] = TLS13_CONTENT_TYPE_HANDSHAKE; // Inner Type: Handshake

    tls13_make_nonce(ctx->server_state.server_handshake_iv, ctx->server_state.write_seq_num, nonce);

    encrypted_len = p + 16;
    aad[0] = TLS13_CONTENT_TYPE_APPLICATION_DATA;
    aad[1] = 0x03;
    aad[2] = 0x03;
    aad[3] = (encrypted_len >> 8) & 0xFF;
    aad[4] = (encrypted_len & 0xFF);

    ciphertext = memory_malloc(encrypted_len);
    if (!ciphertext) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for CertificateVerify ciphertext");
        memory_free(plaintext);
        return -1;
    }

    aes_gcm_encrypt_with_aad_with_tag(ciphertext, plaintext, p,
                                      ctx->server_state.server_handshake_key, key_len,
                                      nonce, 12, aad, 5, ciphertext + p, 16);

    memory_free(plaintext);

    ctx->config->network_send(ctx->connection_state.network_client_identifier, aad, 5, 0);
    ctx->config->network_send(ctx->connection_state.network_client_identifier, ciphertext, encrypted_len, 0);

    memory_free(ciphertext);

    ctx->server_state.write_seq_num++;
    return 0;
}

static int32_t tls13_write_chunk(tls13_session_t* ctx, const uint8_t* data, uint32_t len, tls13_content_type_t content_type) {
    // 16384 is the max TLS record size
    uint32_t p_len = len + 1;
    uint8_t plaintext[p_len];
    uint8_t ciphertext[p_len + 16];

    // Copy payload and append Inner Content Type (0x17 for Application Data)
    memory_memcopy(data, plaintext, len);
    plaintext[len] = content_type;

    // Prepare Nonce (IV ^ write_seq_num)
    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_state.server_application_iv, ctx->server_state.write_seq_num, nonce);

    // Prepare AAD (5-byte Record Header)
    uint16_t encrypted_record_len = p_len + 16;
    uint8_t aad[5] = {
        TLS13_CONTENT_TYPE_APPLICATION_DATA,
        0x03, 0x03,
        (encrypted_record_len >> 8), (encrypted_record_len & 0xFF)
    };

    // Encrypt
    size_t key_len = ctx->connection_state.handshake_key_len;
    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        ciphertext, plaintext, p_len,
        ctx->server_state.server_application_key, key_len,
        nonce, 12, aad, 5, ciphertext + p_len, 16
        );

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Encryption Failed! Nonce/Key/AAD mismatch.");
        return -1;
    }

    // Send Header + Ciphertext
    if (ctx->config->network_send(ctx->connection_state.network_client_identifier, aad, 5, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send TLS record header");
        return -1;
    }
    if (ctx->config->network_send(ctx->connection_state.network_client_identifier, ciphertext, encrypted_record_len, 0) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send TLS record ciphertext");
        return -1;
    }

    ctx->server_state.write_seq_num++;
    return len;
}

static int32_t tls13_write_ext(tls13_session_t* ctx, const uint8_t* data, uint32_t len, tls13_content_type_t content_type) {
    if(!ctx || !data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS context or data buffer is NULL");
        return -1;
    }

    if(len == 0) {
        return 0;
    }

    if(ctx->connection_state.connection_closed) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Attempted to write to a closed TLS connection");
        return -1;
    }

    int64_t remaining  = len;
    int32_t total_sent = 0;

    while(remaining > 0) {
        uint32_t chunk_size = remaining > 16384 ? 16384 : (uint32_t)remaining;
        int32_t sent = tls13_write_chunk(ctx, data + total_sent, chunk_size, content_type);
        if(sent < 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to write TLS chunk");
            return -1;
        }
        total_sent += sent;
        remaining  -= sent;
    }

    return len;
}

int32_t tls13_write(tls13_session_t* ctx, const uint8_t* data, uint32_t len) {
    return tls13_write_ext(ctx, data, len, TLS13_CONTENT_TYPE_APPLICATION_DATA);
}

int32_t tls13_read(tls13_session_t* ctx, uint8_t* out_data, uint32_t max_len) {
    if(!ctx || !out_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS context or output buffer is NULL");
        return -1;
    }

    if(ctx->connection_state.connection_closed) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Attempted to read from a closed TLS connection");
        return 0;
    }

    if(max_len == 0) {
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Read called with max_len=0, returning 0");
        return 0;
    }

    if(!ctx->client_state.read_buffer) {
        ctx->client_state.read_buffer = pipeline_create(16384); // 16KB buffer
        if(!ctx->client_state.read_buffer) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create read buffer pipeline");
            return -1;
        }
    }

    int32_t remaining = max_len;

    int32_t total_read = pipeline_read(ctx->client_state.read_buffer, remaining, out_data);
    remaining -= total_read;

    if(remaining == 0) {
        return total_read;
    }

    uint8_t header[5];

    int32_t bytes_read = ctx->config->network_recv(ctx->connection_state.network_client_identifier, header, 5, 0);

    if(bytes_read == 0) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "TLS connection closed by peer");
        ctx->connection_state.connection_closed = true;
        return total_read; // Return what we have so far
    } else if (bytes_read < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Network receive error while reading TLS record header");
        ctx->connection_state.connection_closed = true;
        return -1;
    }

    // Handle legacy ChangeCipherSpec if it pops up mid-stream (unlikely but possible)
    if (header[0] == TLS13_CONTENT_TYPE_CHANGE_CIPHER_SPEC) {
        uint16_t ccs_len = (header[3] << 8) | header[4];
        uint8_t dummy[16];
        ctx->config->network_recv(ctx->connection_state.network_client_identifier, dummy, ccs_len, 0);
        return tls13_read(ctx, out_data, max_len);
    }

    if (header[0] != TLS13_CONTENT_TYPE_APPLICATION_DATA) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected encrypted record (0x17), got 0x%02x", header[0]);
        return -1;
    }

    uint16_t record_len = (header[3] << 8) | header[4];
    uint8_t* buffer = memory_malloc(record_len);

    bytes_read = ctx->config->network_recv(ctx->connection_state.network_client_identifier, buffer, record_len, 0);

    if (bytes_read <= 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read TLS record payload");
        memory_free(buffer);
        ctx->connection_state.connection_closed = true;
        return -1;
    }

    // Decrypt using Application Keys
    uint8_t nonce[12];
    tls13_make_nonce(ctx->client_state.client_application_iv, ctx->client_state.read_seq_num, nonce);

    uint8_t* plaintext = memory_malloc(record_len);
    uint32_t ciphertext_len = record_len - 16;

    int32_t status = aes_gcm_decrypt_with_aad_with_tag(
        plaintext, buffer, ciphertext_len,
        ctx->client_state.client_application_key, ctx->connection_state.handshake_key_len,
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
    if (inner_type == TLS13_CONTENT_TYPE_ALERT) { // ALERT
        if (plaintext[0] == TLS13_ALERT_LEVEL_WARNING
            && plaintext[1] == TLS13_ALERT_DESCRIPTION_CLOSE_NOTIFY) {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Received Close Notify.");
            ctx->connection_state.connection_closed = true;
            memory_free(plaintext);
            return 0;
        }
        tls13_print_alert(plaintext[0], plaintext[1]);
        memory_free(plaintext);
        return -1;
    }

    if (inner_type == TLS13_CONTENT_TYPE_HANDSHAKE) { // POST-HANDSHAKE (e.g. KeyUpdate or NewSessionTicket)
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Received post-handshake message type 0x%02x", plaintext[0]);
        // Note: KeyUpdate is 0x18. If you don't handle it,
        // the next record will fail decryption because keys didn't rotate!
        memory_free(plaintext);
        return tls13_read(ctx, out_data, max_len);
    }

    if (inner_type != TLS13_CONTENT_TYPE_APPLICATION_DATA) {
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
        pipeline_write(ctx->client_state.read_buffer, real_data_len - to_copy, &plaintext[to_copy]);
    }

    ctx->client_state.read_seq_num++;
    memory_free(plaintext);
    return to_copy + total_read;
}

static int8_t tls13_send_finished(tls13_session_t* ctx) {
    uint8_t verify_data[SHA384_OUTPUT_SIZE];
    uint8_t hlen = ctx->connection_state.handshake_hash_len;
    size_t key_len = ctx->connection_state.handshake_key_len;

    // Get the current Transcript Hash (includes ClientHello...CertificateVerify)
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for Server Finished");
        return -1;
    }

    // Compute HMAC(finished_key, current_hash)
    uint8_t* hmac_out;
    if(tls13_hash_hmac(ctx->connection_state.selected_hash_algorithm,
                       ctx->server_state.server_finished_key, hlen,
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

    plaintext[p++] = TLS13_HANDSHAKE_TYPE_FINISHED; // Type: Finished
    plaintext[p++] = 0x00; plaintext[p++] = 0x00; plaintext[p++] = hlen; // Length
    memory_memcopy(verify_data, &plaintext[p], hlen);
    p += hlen;

    /* --- Update Hash (The Finished message IS hashed for the next steps) --- */
    tls13_hash_update(ctx, plaintext, p);

    /* --- Wrap in Encrypted Record --- */
    plaintext[p++] = TLS13_CONTENT_TYPE_HANDSHAKE; // Inner Type: Handshake

    uint8_t nonce[12];
    tls13_make_nonce(ctx->server_state.server_handshake_iv, ctx->server_state.write_seq_num, nonce);

    uint16_t encrypted_len = p + 16;
    uint8_t aad[5] = {
        TLS13_CONTENT_TYPE_APPLICATION_DATA,
        0x03, 0x03,
        (encrypted_len >> 8), (encrypted_len & 0xFF)
    };

    aes_gcm_encrypt_with_aad_with_tag(ciphertext, plaintext, p,
                                      ctx->server_state.server_handshake_key, key_len,
                                      nonce, 12, aad, 5, ciphertext + p, 16);

    ctx->config->network_send(ctx->connection_state.network_client_identifier, aad, 5, 0);
    ctx->config->network_send(ctx->connection_state.network_client_identifier, ciphertext, encrypted_len, 0);

    ctx->server_state.write_seq_num++;

    return 0;
}

static int8_t tls13_read_client_handshake_message(tls13_session_t* ctx, uint8_t** out_buffer, uint16_t* out_len) {
    uint8_t header[5];
    int32_t ret = ctx->config->network_recv(ctx->connection_state.network_client_identifier, header, 5, 0);

    if(ret == 0) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client closed the connection");
        ctx->connection_state.connection_closed = true;
        return 0;
    }

    if (ret != 5) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read TLS record header for Client Handshake message: ret=%d", ret);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    // Handle Dummy ChangeCipherSpec (CCS is Type 0x14)
    if (header[0] == TLS13_CONTENT_TYPE_CHANGE_CIPHER_SPEC) {
        uint16_t ccs_len = (header[3] << 8) | header[4];
        uint8_t dummy[16];
        ctx->config->network_recv(ctx->connection_state.network_client_identifier, dummy, ccs_len, 0);
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Received Dummy ChangeCipherSpec before Client Finished");
        return tls13_read_client_handshake_message(ctx, out_buffer, out_len);
    }

    if(header[0] == TLS13_CONTENT_TYPE_ALERT) {
        int32_t alert_len = (header[3] << 8) | header[4];
        if(alert_len != 2) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Alert message with invalid length: %d", alert_len);
            return -1;
        }

        uint8_t alert_payload[2];
        ctx->config->network_recv(ctx->connection_state.network_client_identifier, alert_payload, 2, 0);
        tls13_alert_level_t alert_level = alert_payload[0];
        tls13_alert_description_t alert_desc = alert_payload[1];
        tls13_print_alert(alert_level, alert_desc);
        return -1;
    }

    if (header[0] != TLS13_CONTENT_TYPE_APPLICATION_DATA) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected encrypted record (0x17), got 0x%02x", header[0]);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE;
        return -1;
    }

    int32_t record_len = (header[3] << 8) | header[4];

    PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Reading Client Handshake record of length %d", record_len);

    uint8_t* buffer = (uint8_t*)memory_malloc(record_len);

    if (!buffer) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for record buffer");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    ret = ctx->config->network_recv(ctx->connection_state.network_client_identifier, buffer, record_len, 0);
    if (ret != record_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read full Client Message");
        memory_free(buffer);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    uint32_t ciphertext_len = record_len - 16;
    uint8_t* tag = buffer + ciphertext_len;

    uint8_t nonce[12];
    tls13_make_nonce(ctx->client_state.client_handshake_iv, ctx->client_state.read_seq_num, nonce);

    uint8_t* plaintext = (uint8_t*)memory_malloc(ciphertext_len);
    if (!plaintext) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for Client Message plaintext");
        memory_free(buffer);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    int32_t status = aes_gcm_decrypt_with_aad_with_tag(
        plaintext,
        buffer, ciphertext_len,
        ctx->client_state.client_handshake_key, ctx->connection_state.handshake_key_len,
        nonce, 12,
        header, 5,
        tag, 16
        );

    memory_free(buffer);

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Decryption Failed! Nonce/Key/AAD mismatch.");
        memory_free(plaintext);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_BAD_RECORD_MAC;
        return -1;
    }

    // Identify real content type (ignores potential padding)
    int32_t type_pos = ciphertext_len - 1;
    while (type_pos > 0 && plaintext[type_pos] == 0x00) {type_pos--;}
    uint8_t inner_type = plaintext[type_pos];

    if (inner_type == TLS13_CONTENT_TYPE_ALERT) { // ALERT
        tls13_alert_level_t alert_level = plaintext[0];
        tls13_alert_description_t alert_desc = plaintext[1];
        tls13_print_alert(alert_level, alert_desc);
        memory_free(plaintext);
        return -1;
    }

    if(inner_type != TLS13_CONTENT_TYPE_HANDSHAKE) { // Handshake
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unexpected Inner Content Type 0x%02x received", inner_type);
        memory_free(plaintext);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE;
        return -1;
    }

    uint16_t inner_len = plaintext[1] << 16 | plaintext[2] << 8 | plaintext[3];
    if (inner_len > type_pos) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Handshake message length %d exceeds actual data length %d", inner_len, type_pos);
        memory_free(plaintext);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    *out_buffer = plaintext;
    *out_len = type_pos; // Data ends before the Inner Content Type byte

    return 0;
}

static int8_t tls13_process_client_certificate_internal(tls13_session_t*     ctx,
                                                        uint8_t*             handshake_message,
                                                        uint16_t             handshake_message_len,
                                                        x509_certificate_t** client_certificate) {

    if (handshake_message_len < 4) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    if (handshake_message[0] != TLS13_HANDSHAKE_TYPE_CERTIFICATE) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected Certificate handshake message, got 0x%02x", handshake_message[0]);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE;
        return -1;
    }

    uint16_t body_len = (handshake_message[1] << 16) | (handshake_message[2] << 8) | handshake_message[3];
    if (body_len > handshake_message_len - 4) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Certificate message length longer than actual data received");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    uint8_t* p = handshake_message + 4;
    uint8_t* end = p + body_len;

    // 1. request_context (1 byte length prefix)
    if (p + 1 > end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    uint8_t context_len = *p++;
    p += context_len; // Skip context

    // 2. certificate_list (3 bytes length prefix)
    if (p + 3 > end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    uint32_t cert_list_len = (p[0] << 16) | (p[1] << 8) | p[2];
    p += 3;

    if (cert_list_len == 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Empty client certificate list - Auth Failed");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_CERTIFICATE_REQUIRED;
        return -1;
    }

    uint8_t* tmp_p = p;
    uint8_t* tmp_end = p + cert_list_len;
    int32_t cert_count = 0;

    while (tmp_p + 3 <= tmp_end) {
        uint32_t cert_data_len = (tmp_p[0] << 16) | (tmp_p[1] << 8) | tmp_p[2];
        tmp_p += 3 + cert_data_len + 2; // Skip cert_data and extensions
        cert_count++;
    }

    if (tmp_p != tmp_end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message - cert_list length mismatch");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    if (cert_count == 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No certificates found in client certificate list");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_CERTIFICATE_REQUIRED;
        return -1;
    }

    x509_certificate_t* cert_list[cert_count];
    memory_memclean(cert_list, sizeof(cert_list));

    int32_t cert_index = 0;

    // fill cert_list with parsed certificates for potential future use (e.g. client cert chain handling)
    while(cert_list_len > 0) {
        if (p + 3 > end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message while counting certificates");
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
            return -1;
        }

        uint32_t cert_data_len = (p[0] << 16) | (p[1] << 8) | p[2];
        p += 3;

        if (p + cert_data_len > end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message while counting certificates");
            for (int32_t i = 0; i < cert_count - 1; i++) {
                if(cert_list[i]) {
                    x509_certificate_free(cert_list[i]);
                }
            }
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
            return -1;
        }

        cert_list[cert_index] = x509_certificate_from_der(p, cert_data_len);
        if (!cert_list[cert_index]) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to parse client certificate while counting certificates");
            for (int32_t i = 0; i < cert_count - 1; i++) {
                if(cert_list[i]) {
                    x509_certificate_free(cert_list[i]);
                }
            }
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
            return -1;
        }

        p += cert_data_len;

        if (p + 2 > end) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message while counting certificates");
            for (int32_t i = 0; i < cert_count; i++) {
                if(cert_list[i]) {
                    x509_certificate_free(cert_list[i]);
                }
            }
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
            return -1;
        }

        uint16_t ext_len = (p[0] << 8) | p[1];
        p += 2 + ext_len;

        cert_list_len -= (3 + cert_data_len + 2 + ext_len);
        cert_index++;
    }

    // Safety check: ensure we didn't overrun the handshake message
    if (p > end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Certificate message");
        return -1;
    }

    if(!ctx->config->client_certificate_verify_callback) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "No client certificate verification callback provided - cannot verify client certificate");
        for (int32_t i = 0; i < cert_count; i++) {
            if(cert_list[i]) {
                x509_certificate_free(cert_list[i]);
            }
        }
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    if (ctx->config->client_certificate_verify_callback(ctx, cert_list, cert_count) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client certificate verification callback failed - Auth Failed");
        for (int32_t i = 0; i < cert_count; i++) {
            if(cert_list[i]) {
                x509_certificate_free(cert_list[i]);
            }
        }
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE;
        return -1;
    }

    *client_certificate = cert_list[0]; // For now, we only verify the first certificate in the list (the leaf)

    for (int32_t i = 1; i < cert_count; i++) {
        if(cert_list[i]) {
            x509_certificate_free(cert_list[i]);
        }
    }

    // Update transcript with decrypted Handshake message
    if(tls13_hash_update(ctx, handshake_message, body_len + 4) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update transcript hash with Client Handshake message");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    return 0;
}

static int8_t tls13_process_client_certificate_verify_internal(tls13_session_t*    ctx,
                                                               uint8_t*            handshake_message,
                                                               uint16_t            handshake_message_len,
                                                               x509_certificate_t* client_certificate) {

    if (handshake_message[0] != TLS13_HANDSHAKE_TYPE_CERTIFICATE_VERIFY) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected CertificateVerify handshake message, got 0x%02x", handshake_message[0]);
        x509_certificate_free(client_certificate);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE;
        return -1;
    }

    uint16_t verify_data_len = (handshake_message[1] << 16) | (handshake_message[2] << 8) | handshake_message[3];
    uint8_t* received_verify_data = handshake_message + 4;

    if (verify_data_len > handshake_message_len - 4) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "CertificateVerify message length longer than actual data received");
        x509_certificate_free(client_certificate);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    uint16_t algorithm = (received_verify_data[0] << 8) | received_verify_data[1];;
    uint16_t sig_len = (received_verify_data[2] << 8) | received_verify_data[3];

    if(algorithm == TLS_SIG_ALG_ED25519) {
        if (sig_len != ED25519_SIGNATURE_LEN || (4 + sig_len) != verify_data_len) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid signature length in CertificateVerify");
            x509_certificate_free(client_certificate);
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
            return -1;
        }
    } else if(algorithm == TLS_SIG_ALG_ECDSA_SECP256R1_SHA256) {
        // ECDSA signatures can vary in length due to DER encoding, but we can set a reasonable max (e.g. 72 bytes)
        if (sig_len == 0 || sig_len > 72 || (4 + sig_len) != verify_data_len) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid signature length in CertificateVerify for ECDSA");
            x509_certificate_free(client_certificate);
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
            return -1;
        }
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported signature algorithm in CertificateVerify: 0x%04x", algorithm);
        x509_certificate_free(client_certificate);
        return -1;
    }

    x509_algorithm_t cert_alg = x509_certificate_get_public_key_algorithm(client_certificate);

    if ((algorithm == TLS_SIG_ALG_ED25519 && cert_alg != X509_ALGORITHM_ED25519) ||
        (algorithm == TLS_SIG_ALG_ECDSA_SECP256R1_SHA256 && cert_alg != X509_ALGORITHM_ECDSA_SECP256R1_SHA256)) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Signature algorithm in CertificateVerify does not match client certificate public key algorithm: 0x%04x vs cert alg %d", algorithm, cert_alg);
        x509_certificate_free(client_certificate);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE;
        return -1;
    }

    const size_t space_count = 64;
    const char_t* verify_string = "TLS 1.3, client CertificateVerify";
    uint8_t verify_buffer[space_count + strlen(verify_string) + 1 + SHA384_OUTPUT_SIZE];
    memory_memset(verify_buffer, 0x20, space_count); // 64 spaces
    memory_memcopy(verify_string, verify_buffer + space_count, strlen(verify_string));
    verify_buffer[64 + strlen(verify_string)] = 0x00; // Null terminator

    // Get the current snapshot of the handshake hash
    uint32_t hlen = ctx->connection_state.handshake_hash_len;

    // Note: This must include the Certificate message bytes!
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for CertificateVerify");
        x509_certificate_free(client_certificate);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    memory_memcopy(current_hash, verify_buffer + space_count + strlen(verify_string) + 1, hlen);
    size_t total_len = space_count + strlen(verify_string) + 1 + hlen;

    size_t public_key_len = 0;
    uint8_t* public_key = x509_certificate_get_public_key_data(client_certificate, &public_key_len);
    if (!public_key) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get public key from client certificate");
        x509_certificate_free(client_certificate);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    if(algorithm == TLS_SIG_ALG_ED25519) {
        uint8_t signature[ED25519_SIGNATURE_LEN];
        memory_memcopy(&received_verify_data[4], signature, ED25519_SIGNATURE_LEN);

        if (ed25519_verify(signature, verify_buffer, total_len, public_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client CertificateVerify signature verification failed");
            memory_free(public_key);
            x509_certificate_free(client_certificate);
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE;
            return -1;
        }
    } else if(algorithm == TLS_SIG_ALG_ECDSA_SECP256R1_SHA256) {
        uint8_t* der_signature = &received_verify_data[4];
        size_t der_signature_len = sig_len;

        uint8_t* raw_signature = ellipticcurve_secp256r1_decode_signature(der_signature, der_signature_len);
        if (!raw_signature) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode DER signature in CertificateVerify");
            memory_free(public_key);
            x509_certificate_free(client_certificate);
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
            return -1;
        }

        if (ellipticcurve_secp256r1_verify(raw_signature, verify_buffer, total_len, public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client CertificateVerify ECDSA signature verification failed");
            memory_free(raw_signature);
            memory_free(public_key);
            x509_certificate_free(client_certificate);
            ctx->connection_state.has_alert = true;
            ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
            ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_BAD_CERTIFICATE;
            return -1;
        }
        memory_free(raw_signature);
    } else { // never hit but paranoid checks are good
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported signature algorithm in CertificateVerify: 0x%04x", algorithm);
        memory_free(public_key);
        x509_certificate_free(client_certificate);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    memory_free(public_key);
    x509_certificate_free(client_certificate);

    PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Client CertificateVerify processed successfully");

    // Update transcript with decrypted Handshake message
    if(tls13_hash_update(ctx, handshake_message, verify_data_len + 4) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update transcript hash with Client Handshake message");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    return 0;
}

static int8_t tls13_process_client_finished_internal(tls13_session_t* ctx,
                                                     uint8_t*         handshake_message,
                                                     uint16_t         handshake_message_len) {
    uint8_t hlen = ctx->connection_state.handshake_hash_len;

    if (handshake_message_len < 4) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Malformed Client Finished message");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    if (handshake_message[0] != TLS13_HANDSHAKE_TYPE_FINISHED) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Expected Client Finished handshake message, got 0x%02x", handshake_message[0]);
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE;
        return -1;
    }

    uint16_t verify_data_len = (handshake_message[1] << 16) | (handshake_message[2] << 8) | handshake_message[3];
    uint8_t* received_verify_data = handshake_message + 4;

    if (verify_data_len != hlen) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client Finished: Invalid verify_data length");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_DECODE_ERROR;
        return -1;
    }

    // 1. Get the Transcript Hash
    // This snapshot must include everything up to your Server Finished
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for Client Finished");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    // 2. Compute the expected HMAC
    uint8_t expected_verify_data[SHA384_OUTPUT_SIZE];
    uint8_t* hmac_out;
    if(tls13_hash_hmac(ctx->connection_state.selected_hash_algorithm,
                       ctx->client_state.client_finished_key, hlen,
                       current_hash, hlen,
                       &hmac_out) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute expected HMAC for Client Finished");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }
    memory_memcopy(hmac_out, expected_verify_data, hlen);
    memory_free(hmac_out);

    // 3. Constant-time comparison (if available in your library)
    if (memory_memcompare(expected_verify_data, received_verify_data, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Client Finished: HMAC verification failed!");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_HANDSHAKE_FAILURE;
        return -1;
    }

    // Update transcript with decrypted Handshake message
    if(tls13_hash_update(ctx, handshake_message, verify_data_len + 4) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to update transcript hash with Client Handshake message");
        ctx->connection_state.has_alert = true;
        ctx->connection_state.alert_level = TLS13_ALERT_LEVEL_FATAL;
        ctx->connection_state.alert_description = TLS13_ALERT_DESCRIPTION_INTERNAL_ERROR;
        return -1;
    }

    ctx->client_state.read_seq_num = 0; // Reset sequence number for application data

    return 0;
}

static int8_t tls13_handle_client_handshake_read(tls13_session_t* ctx) {
    uint8_t* handshake_message = NULL;
    uint16_t handshake_message_len = 0;

    if (tls13_read_client_handshake_message(ctx, &handshake_message, &handshake_message_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read Client Certificate message");
        tls13_print_alert(ctx->connection_state.alert_level, ctx->connection_state.alert_description);
        uint8_t alert_payload[2] = {ctx->connection_state.alert_level, ctx->connection_state.alert_description};
        tls13_write_ext(ctx, alert_payload, sizeof(alert_payload), TLS13_CONTENT_TYPE_ALERT);
        return -1;
    }

    if(ctx->connection_state.connection_closed) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Connection closed by client during handshake");
        return 0;
    }

    uint8_t* original_handshake_message = handshake_message; // Keep track for freeing later

    if(ctx->connection_state.session_resumed || !ctx->config->require_client_certificate) {
        if(tls13_process_client_finished_internal(ctx,
                                                  handshake_message,
                                                  handshake_message_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to process client finished for resumed session");
            memory_free(original_handshake_message);
            tls13_print_alert(ctx->connection_state.alert_level, ctx->connection_state.alert_description);
            uint8_t alert_payload[2] = {ctx->connection_state.alert_level, ctx->connection_state.alert_description};
            tls13_write_ext(ctx, alert_payload, sizeof(alert_payload), TLS13_CONTENT_TYPE_ALERT);
            return -1;
        }

        memory_free(original_handshake_message);

        return 0;
    }

    uint16_t body_len = (handshake_message[1] << 16) | (handshake_message[2] << 8) | handshake_message[3];

    x509_certificate_t* client_certificate = NULL;

    if(tls13_process_client_certificate_internal(ctx,
                                                 handshake_message,
                                                 handshake_message_len,
                                                 &client_certificate) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to process client certificate");
        x509_certificate_free(client_certificate);
        client_certificate = NULL; // Avoid double free in error case
    }

    if(handshake_message_len - body_len - 4 > 0) {
        // Client sent combined messages (Certificate + CertificateVerify, may be Finished).
        handshake_message += (4 + body_len);
        handshake_message_len -= (4 + body_len);
    } else {
        memory_free(original_handshake_message);

        ctx->client_state.read_seq_num++;

        if(tls13_read_client_handshake_message(ctx, &handshake_message, &handshake_message_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read Client CertificateVerify message");
            tls13_print_alert(ctx->connection_state.alert_level, ctx->connection_state.alert_description);
            uint8_t alert_payload[2] = {ctx->connection_state.alert_level, ctx->connection_state.alert_description};
            tls13_write_ext(ctx, alert_payload, sizeof(alert_payload), TLS13_CONTENT_TYPE_ALERT);
            return -1;
        }

        if(ctx->connection_state.connection_closed) {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Connection closed by client during handshake");
            x509_certificate_free(client_certificate);
            return 0;
        }

        original_handshake_message = handshake_message; // Keep track for freeing later
    }

    body_len = (handshake_message[1] << 16) | (handshake_message[2] << 8) | handshake_message[3];

    if(ctx->connection_state.has_alert) {
        x509_certificate_free(client_certificate);
        client_certificate = NULL; // Avoid double free in error case

        if(handshake_message[0] == TLS13_HANDSHAKE_TYPE_FINISHED) {
            memory_free(original_handshake_message);
            tls13_print_alert(ctx->connection_state.alert_level, ctx->connection_state.alert_description);
            uint8_t alert_payload[2] = {ctx->connection_state.alert_level, ctx->connection_state.alert_description};
            tls13_write_ext(ctx, alert_payload, sizeof(alert_payload), TLS13_CONTENT_TYPE_ALERT);
            return -1;
        }
    }

    if(!ctx->connection_state.has_alert && tls13_process_client_certificate_verify_internal(ctx,
                                                                                            handshake_message,
                                                                                            handshake_message_len,
                                                                                            client_certificate) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to process client certificate verify");
        x509_certificate_free(client_certificate);
        client_certificate = NULL; // Avoid double free in error case
    }

    if(handshake_message_len - body_len - 4 > 0) {
        // Client sent combined messages (CertificateVerify + Finished).
        handshake_message += (4 + body_len);
        handshake_message_len -= (4 + body_len);
    } else {
        memory_free(original_handshake_message);

        ctx->client_state.read_seq_num++;

        // Read the next message which should be Finished
        if(tls13_read_client_handshake_message(ctx, &handshake_message, &handshake_message_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read Client Finished message after CertificateVerify");
            uint8_t alert_payload[2] = {ctx->connection_state.alert_level, ctx->connection_state.alert_description};
            tls13_write_ext(ctx, alert_payload, sizeof(alert_payload), TLS13_CONTENT_TYPE_ALERT);
            return -1;
        }

        if(ctx->connection_state.connection_closed) {
            PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Connection closed by client during handshake");
            x509_certificate_free(client_certificate);
            return 0;
        }

        original_handshake_message = handshake_message; // Keep track for freeing later
    }

    if(ctx->connection_state.has_alert) {
        x509_certificate_free(client_certificate);
        client_certificate = NULL; // Avoid double free in error case
    }

    if(!ctx->connection_state.has_alert && tls13_process_client_finished_internal(ctx,
                                                                                  handshake_message,
                                                                                  handshake_message_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to process client finished");
    }

    memory_free(original_handshake_message);

    if(!ctx->connection_state.has_alert) {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Handshake with client completed successfully");
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Handshake with client failed.");
        tls13_print_alert(ctx->connection_state.alert_level, ctx->connection_state.alert_description);
        uint8_t alert_payload[2] = {ctx->connection_state.alert_level, ctx->connection_state.alert_description};
        tls13_write_ext(ctx, alert_payload, sizeof(alert_payload), TLS13_CONTENT_TYPE_ALERT);
        return -1;
    }

    return 0;
}

static int8_t tls13_send_new_session_ticket(tls13_session_t* ctx) {
    if(ctx->connection_state.psk_key_exchange_mode != TLS13_PSK_KEY_EXCHANGE_MODE_DHE_PSK) {
        return 0; // No ticket if not doing DHE_PSK
    }

    if(!ctx->config->get_psk_encryption_keys_callback) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "No callback provided for getting PSK encryption keys - cannot send NewSessionTicket");
        return 0;
    }

    uint8_t ticket_nonce[16];
    get_random_bytes(ticket_nonce, sizeof(ticket_nonce));

    uint8_t resumption_secret[SHA384_OUTPUT_SIZE];
    uint8_t hlen = ctx->connection_state.handshake_hash_len;
    if(hkdf_expand_label_ext(ctx, ctx->connection_state.resumption_master_secret, "resumption", ticket_nonce, sizeof(ticket_nonce), resumption_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive resumption secret for NewSessionTicket");
        return -1;
    }

    uint64_t now_ns = time_ns(NULL);
    uint32_t lifetime = 24 * 60 * 60;
    uint32_t ticket_age_add = 0;
    get_random_bytes((uint8_t*)&ticket_age_add, sizeof(ticket_age_add));

    uint8_t psk_identity[1024];
    int32_t psk_len = 0;

    // put cipher suite
    psk_identity[psk_len++] = (ctx->connection_state.selected_cipher_suite >> 8) & 0xFF;
    psk_identity[psk_len++] = (ctx->connection_state.selected_cipher_suite & 0xFF);

    // put resumption secret
    memory_memcopy(resumption_secret, &psk_identity[psk_len], hlen);
    psk_len += hlen;
    memory_memclean(resumption_secret, sizeof(resumption_secret));

    // put lifetime
    psk_identity[psk_len++] = (lifetime >> 24) & 0xFF;
    psk_identity[psk_len++] = (lifetime >> 16) & 0xFF;
    psk_identity[psk_len++] = (lifetime >> 8) & 0xFF;
    psk_identity[psk_len++] = (lifetime & 0xFF);

    // put ticket_age_add
    psk_identity[psk_len++] = (ticket_age_add >> 24) & 0xFF;
    psk_identity[psk_len++] = (ticket_age_add >> 16) & 0xFF;
    psk_identity[psk_len++] = (ticket_age_add >> 8) & 0xFF;
    psk_identity[psk_len++] = (ticket_age_add & 0xFF);

    // put ticket now_ns
    psk_identity[psk_len++] = (now_ns >> 56) & 0xFF;
    psk_identity[psk_len++] = (now_ns >> 48) & 0xFF;
    psk_identity[psk_len++] = (now_ns >> 40) & 0xFF;
    psk_identity[psk_len++] = (now_ns >> 32) & 0xFF;
    psk_identity[psk_len++] = (now_ns >> 24) & 0xFF;
    psk_identity[psk_len++] = (now_ns >> 16) & 0xFF;
    psk_identity[psk_len++] = (now_ns >> 8) & 0xFF;
    psk_identity[psk_len++] = (now_ns & 0xFF);

    const char* alpn_selected = ctx->connection_state.alpn_h2 ? "h2" : "http/1.1";
    int32_t name_len = strlen(alpn_selected);

    psk_identity[psk_len++] = (name_len >> 8) & 0xFF;
    psk_identity[psk_len++] = (name_len & 0xFF);
    memory_memcopy(alpn_selected, &psk_identity[psk_len], name_len);
    psk_len += name_len;

    // put nonce
    memory_memcopy(ticket_nonce, &psk_identity[psk_len], sizeof(ticket_nonce));
    psk_len += sizeof(ticket_nonce);

    uint8_t psk_identity_ciphertext[1024];

    uint8_t* psk_encryption_key;
    uint8_t* psk_encryption_iv;
    uint8_t* psk_aed_key;

    if(ctx->config->get_psk_encryption_keys_callback(ctx, false, &psk_encryption_key, &psk_encryption_iv, &psk_aed_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive ticket encryption key for NewSessionTicket");
        return -1;
    }

    int32_t status = aes_gcm_encrypt_with_aad_with_tag(
        psk_identity_ciphertext, psk_identity, psk_len,
        psk_encryption_key, AES256_KEY_SIZE,
        psk_encryption_iv, 12,
        psk_aed_key, 16,
        psk_identity_ciphertext + psk_len,
        16
        );

    memory_memclean(psk_identity, sizeof(psk_identity));

    if (status != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encrypt PSK Identity for NewSessionTicket");
        return -1;
    }

    uint8_t plaintext[4096];
    int32_t p = 0;

    plaintext[p++] = TLS13_HANDSHAKE_TYPE_NEW_SESSION_TICKET; // Type: NewSessionTicket
    int32_t hs_len_ptr = p; p += 3; // Placeholder for length

    // Ticket Lifetime (4 bytes) - 24 hours
    plaintext[p++] = (lifetime >> 24) & 0xFF;
    plaintext[p++] = (lifetime >> 16) & 0xFF;
    plaintext[p++] = (lifetime >> 8) & 0xFF;
    plaintext[p++] = (lifetime & 0xFF);

    // Ticket Age Add (4 bytes) - Random value
    plaintext[p++] = (ticket_age_add >> 24) & 0xFF;
    plaintext[p++] = (ticket_age_add >> 16) & 0xFF;
    plaintext[p++] = (ticket_age_add >> 8) & 0xFF;
    plaintext[p++] = (ticket_age_add & 0xFF);

    // Ticket Nonce (1 byte length + nonce)
    plaintext[p++] = sizeof(ticket_nonce);
    memory_memcopy(ticket_nonce, &plaintext[p], sizeof(ticket_nonce));
    p += sizeof(ticket_nonce);
    memory_memclean(ticket_nonce, sizeof(ticket_nonce));

    // Ticket Value (2 bytes length + resumption secret)
    uint16_t ticket_value_len = psk_len + 16; // Encrypted PSK Identity + Tag
    plaintext[p++] = (ticket_value_len >> 8) & 0xFF;
    plaintext[p++] = (ticket_value_len & 0xFF);
    memory_memcopy(psk_identity_ciphertext, &plaintext[p], ticket_value_len);
    p += ticket_value_len;
    memory_memclean(psk_identity_ciphertext, sizeof(psk_identity_ciphertext));

    // Extensions (2 bytes length + empty for now)
    plaintext[p++] = 0x00; // Extensions length high byte
    plaintext[p++] = 0x00; // Extensions length low byte

    // Fix Handshake Length
    uint32_t hs_body_len = p - hs_len_ptr - 3;
    plaintext[hs_len_ptr] = (hs_body_len >> 16) & 0xFF;
    plaintext[hs_len_ptr + 1] = (hs_body_len >> 8) & 0xFF;
    plaintext[hs_len_ptr + 2] = (hs_body_len & 0xFF);

    return tls13_write_ext(ctx, plaintext, p, TLS13_CONTENT_TYPE_HANDSHAKE) < p ? -1 : 0;
}

static int8_t tls13_generate_resumption_keys(tls13_session_t* ctx) {
    size_t hlen = ctx->connection_state.handshake_hash_len;
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for post-handshake processing");
        return -1;
    }

    if(ctx->connection_state.psk_key_exchange_mode == TLS13_PSK_KEY_EXCHANGE_MODE_DHE_PSK) {
        uint8_t resumption_master_secret[64];
        if(hkdf_expand_label_ext(ctx, ctx->connection_state.master_secret, "res master", current_hash, hlen, resumption_master_secret, hlen) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive resumption secret");
            return -1;
        }
        memory_memcopy(resumption_master_secret, ctx->connection_state.resumption_master_secret, hlen);
        memory_memclean(resumption_master_secret, sizeof(resumption_master_secret));
    }

    return 0;
}

static int8_t tls13_generate_application_keys(tls13_session_t* ctx) {
    uint32_t hlen = ctx->connection_state.handshake_hash_len;
    uint32_t key_len = ctx->connection_state.handshake_key_len;
    uint32_t iv_len  = ctx->connection_state.handshake_iv_len;

    // 1. Get current transcript hash (Includes EVERYTHING up to Client Finished)
    uint8_t current_hash[SHA384_OUTPUT_SIZE] = {0};
    if(tls13_hash_get_current(ctx, current_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get current handshake hash for application key generation");
        return -1;
    }

    // 2. Derive Application Traffic Secrets (using transcript hash)
    uint8_t s_ap_traffic_secret[64];
    uint8_t c_ap_traffic_secret[64];

    if(hkdf_expand_label_ext(ctx, ctx->connection_state.master_secret, "s ap traffic", current_hash, hlen, s_ap_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server application traffic secret");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, ctx->connection_state.master_secret, "c ap traffic", current_hash, hlen, c_ap_traffic_secret, hlen) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client application traffic secret");
        return -1;
    }

    // 3. Generate the actual Application Keys/IVs (Context length must be 0)
    if(hkdf_expand_label_ext(ctx, s_ap_traffic_secret, "key", NULL, 0, ctx->server_state.server_application_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server application key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, s_ap_traffic_secret, "iv", NULL, 0, ctx->server_state.server_application_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive server application IV");
        return -1;
    }

    if(hkdf_expand_label_ext(ctx, c_ap_traffic_secret, "key", NULL, 0, ctx->client_state.client_application_key, key_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client application key");
        return -1;
    }
    if(hkdf_expand_label_ext(ctx, c_ap_traffic_secret, "iv", NULL, 0, ctx->client_state.client_application_iv, iv_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive client application IV");
        return -1;
    }

    memory_memclean(s_ap_traffic_secret, sizeof(s_ap_traffic_secret));
    memory_memclean(c_ap_traffic_secret, sizeof(c_ap_traffic_secret));

    // 5. CRITICAL: Reset sequence numbers for Application Phase
    ctx->server_state.write_seq_num = 0;
    // ctx->client_state.read_seq_num = 0; // it should set to 0 after processing Client Finished

    return 0;
}

int8_t tls13_send_close_notify(tls13_session_t* ctx) {
    if(ctx->connection_state.connection_closed) {
        return 0; // Already closed, no need to send again
    }

    uint8_t plaintext[2] = {
        TLS13_ALERT_LEVEL_WARNING,
        TLS13_ALERT_DESCRIPTION_CLOSE_NOTIFY,
    }; // Warning, CloseNotify, InnerType: Alert

    return tls13_write_ext(ctx, plaintext, sizeof(plaintext), TLS13_CONTENT_TYPE_ALERT) < (int32_t)sizeof(plaintext) ? -1 : 0;
}

int8_t tls13_handle_handshake(tls13_session_t* ctx) {
    int32_t res_client_hello = tls13_process_client_hello(ctx);

    if(res_client_hello == -2) {
        PRINTLOG(CRYPTOLIB, LOG_WARNING, "Redirecting HTTP/1.1 client to HTTPS URL");
        return -1;
    }

    if(res_client_hello == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to parse Client Hello");
        return -1;
    }

    get_random_bytes(ctx->server_state.server_random, sizeof(ctx->server_state.server_random));

    if(tls13_send_server_hello(ctx) < 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Server Hello");
        return -1;
    }

    if(tls13_generate_handshake_key_and_iv(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate handshake key and IV");
        return -1;
    }

    if(tls13_send_encrypted_extensions(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Encrypted Extensions");
        return -1;
    }

    if(ctx->config->require_client_certificate) {
        if(tls13_send_certificate_request(ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Certificate Request");
            return -1;
        }
    }

    if(tls13_send_certificate_and_verify(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Certificate");
        return -1;
    }

    if(tls13_send_finished(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send Finished");
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

    if(ctx->connection_state.connection_closed) {
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Connection closed by client after handshake");
        return -1;
    }

    if(tls13_generate_resumption_keys(ctx) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate application keys");
        return -1;
    }

    if(ctx->connection_state.psk_key_exchange_mode == TLS13_PSK_KEY_EXCHANGE_MODE_DHE_PSK) {
        if(tls13_send_new_session_ticket(ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to send NewSessionTicket");
            return -1;
        }
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Sent NewSessionTicket to client for session resumption");
    } else {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Not sending NewSessionTicket since PSK key exchange mode is not DHE_PSK");
    }

    return 0;
}


