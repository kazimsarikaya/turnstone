/**
 * @file x509.64.c
 * @brief X.509 Certificate Implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <crypto/x509.h>
#include <time.h>
#include <memory.h>
#include <random.h>
#include <buffer.h>
#include <strings.h>
#include <logging.h>
#include <crypto/x25519.h>
#include <base64.h>
#include <crypto/pem.h>
#include <crypto/der.h>

MODULE("turnstone.lib.crypto");


typedef struct x509_subject_alternative_name_t x509_subject_alternative_name_t;

struct x509_subject_alternative_name_t {
    x509_subject_alternative_name_type_t type;
    char_t*                              value;
    x509_subject_alternative_name_t*     next;
};

typedef struct x509_extension_t x509_extension_t;

struct x509_extension_t {
    x509_extension_type_t type;
    boolean_t             is_critical;
    union {
        // X509_EXTENSION_BASIC_CONSTRAINTS
        struct {
            boolean_t is_ca;
            int32_t   path_len; // -1 if no limit
        } basic_constraints;

        // X509_EXTENSION_KEY_USAGE
        x509_key_usage_t key_usage;

        // X509_EXTENSION_EXTENDED_KEY_USAGE
        x509_extended_key_usage_t eku;

        // X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME
        x509_subject_alternative_name_t* san_list;

        struct {
            size_t   length;
            uint8_t* data;
        } skid; // Subject Key Identifier
        struct {
            size_t   length;
            uint8_t* data;
        } akid; // Authority Key Identifier
    }                 data;
    x509_extension_t* next;
};

struct x509_certificate_t {
    uint32_t  version;
    uint128_t serial_number;

    char_t* issuer_common_name;
    char_t* subject_common_name;

    time_t not_before;
    time_t not_after;

    // The Extension Chain
    x509_extension_t* extensions;

    // Public Key Info (The "SubjectPublicKeyInfo" part)
    x509_algorithm_t public_key_algorithm;
    size_t           public_key_length;
    uint8_t*         public_key;

    // Signature Info
    x509_algorithm_t signature_algorithm;
    size_t           signature_length;
    uint8_t*         signature;

    // tbs data cache
    uint8_t* tbs_data;
    size_t   tbs_length;

    // assembled certificate cache
    uint8_t* certificate_data;
    size_t   certificate_length;
};

x509_certificate_t* x509_certificate_new(void) {
    x509_certificate_t* cert = memory_malloc(sizeof(x509_certificate_t));

    if (cert == NULL) {
        return NULL;
    }

    cert->version = 2;

    // generate a random serial number like guid v7
    time_t now = time_ns(NULL);
    uint64_t time_low = (uint64_t)(now & 0xFFFFFFFF);
    uint64_t time_mid = (uint64_t)((now >> 32) & 0xFFFF);
    uint64_t time_hi_and_version = (uint64_t)((now >> 48) & 0x0FFF);
    time_hi_and_version |= (7 << 12); // version 7
    uint64_t clock_seq = (uint64_t)(((uint16_t)rand()) & 0x3FFF);
    uint64_t node = ((uint64_t)rand() << 32) | ((uint64_t)rand() << 16) | ((uint64_t)rand());
    cert->serial_number = ((uint128_t)time_low << 96) | ((uint128_t)time_mid << 80) | ((uint128_t)time_hi_and_version << 64) | ((uint128_t)clock_seq << 48) | (uint128_t)node;

    return cert;
}

void x509_certificate_free(x509_certificate_t* cert) {
    if (cert == NULL) {
        return;
    }

    // Free issuer and subject common names
    if (cert->issuer_common_name != NULL) {
        memory_free(cert->issuer_common_name);
    }

    if (cert->subject_common_name != NULL) {
        memory_free(cert->subject_common_name);
    }

    // Free extensions
    x509_extension_t* ext = cert->extensions;
    while (ext != NULL) {
        x509_extension_t* next_ext = ext->next;

        if (ext->type == X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME) {
            x509_subject_alternative_name_t* san = ext->data.san_list;
            while (san != NULL) {
                x509_subject_alternative_name_t* next_san = san->next;
                if (san->value != NULL) {
                    memory_free(san->value);
                }
                memory_free(san);
                san = next_san;
            }
        }

        if (ext->type == X509_EXTENSION_SKID) {
            if (ext->data.skid.data != NULL) {
                memory_free(ext->data.skid.data);
            }
        }

        if (ext->type == X509_EXTENSION_AKID) {
            if (ext->data.akid.data != NULL) {
                memory_free(ext->data.akid.data);
            }
        }

        memory_free(ext);
        ext = next_ext;
    }

    // Free public key
    if (cert->public_key != NULL) {
        memory_free(cert->public_key);
    }

    // Free signature
    if (cert->signature != NULL) {
        memory_free(cert->signature);
    }

    if (cert->tbs_data != NULL) {
        memory_free(cert->tbs_data);
    }

    if (cert->certificate_data != NULL) {
        memory_free(cert->certificate_data);
    }

    // Finally, free the certificate itself
    memory_free(cert);
}

int8_t x509_certificate_add_issuer_common_name(x509_certificate_t* cert, const char_t* common_name){
    if (cert == NULL || common_name == NULL) {
        return -1;
    }

    cert->issuer_common_name = strdup(common_name);
    if (cert->issuer_common_name == NULL) {
        return -1;
    }
    return 0;
}

int8_t x509_certificate_add_subject_common_name(x509_certificate_t* cert, const char_t* common_name){
    if (cert == NULL || common_name == NULL) {
        return -1;
    }

    cert->subject_common_name = strdup(common_name);
    if (cert->subject_common_name == NULL) {
        return -1;
    }
    return 0;
}

int8_t x509_certificate_add_duration(x509_certificate_t* cert, uint32_t days_valid){
    if (cert == NULL) {
        return -1;
    }

    time_t now = time_ns(NULL);
    cert->not_before = now;
    cert->not_after = now + (days_valid * 24 * 60 * 60 * 1000000000ULL); // days to nanoseconds
    return 0;
}

int8_t x509_certificate_set_is_ca(x509_certificate_t* cert, boolean_t is_ca, int32_t path_len) {
    if (cert == NULL) {
        return -1;
    }

    x509_extension_t* ext = memory_malloc(sizeof(x509_extension_t));
    if (ext == NULL) {
        return -1;
    }

    ext->type = X509_EXTENSION_BASIC_CONSTRAINTS;
    ext->is_critical = true;
    ext->data.basic_constraints.is_ca = is_ca;
    ext->data.basic_constraints.path_len = path_len;
    ext->next = cert->extensions;
    cert->extensions = ext;

    return 0;
}

int8_t x509_certificate_add_key_usage(x509_certificate_t* cert, x509_key_usage_t key_usage) {
    if (cert == NULL) {
        return -1;
    }

    x509_extension_t* ext = memory_malloc(sizeof(x509_extension_t));
    if (ext == NULL) {
        return -1;
    }

    ext->type = X509_EXTENSION_KEY_USAGE;
    ext->is_critical = true;
    ext->data.key_usage = key_usage;
    ext->next = cert->extensions;
    cert->extensions = ext;

    return 0;
}

int8_t x509_certificate_add_extended_key_usage(x509_certificate_t* cert, x509_extended_key_usage_t eku) {
    if (cert == NULL) {
        return -1;
    }

    x509_extension_t* ext = memory_malloc(sizeof(x509_extension_t));
    if (ext == NULL) {
        return -1;
    }

    ext->type = X509_EXTENSION_EXTENDED_KEY_USAGE;
    ext->is_critical = false;
    ext->data.eku = eku;
    ext->next = cert->extensions;
    cert->extensions = ext;

    return 0;
}

int8_t x509_certificate_add_subject_alternative_name(x509_certificate_t* cert, x509_subject_alternative_name_type_t type, const char_t* value) {
    if (cert == NULL || value == NULL) {
        return -1;
    }

    x509_extension_t* ext = cert->extensions;
    x509_extension_t* san_ext = NULL;

    // Find existing SAN extension
    while (ext != NULL) {
        if (ext->type == X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME) {
            san_ext = ext;
            break;
        }
        ext = ext->next;
    }

    // If SAN extension does not exist, create it
    if (san_ext == NULL) {
        san_ext = memory_malloc(sizeof(x509_extension_t));
        if (san_ext == NULL) {
            return -1;
        }
        san_ext->type = X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME;
        san_ext->is_critical = false;
        san_ext->data.san_list = NULL;
        san_ext->next = cert->extensions;
        cert->extensions = san_ext;
    }

    // Create new SAN entry
    x509_subject_alternative_name_t* san_entry = memory_malloc(sizeof(x509_subject_alternative_name_t));
    if (san_entry == NULL) {
        return -1;
    }
    san_entry->type = type;
    san_entry->value = strdup(value);
    if (san_entry->value == NULL) {
        memory_free(san_entry);
        return -1;
    }
    san_entry->next = san_ext->data.san_list;
    san_ext->data.san_list = san_entry;

    return 0;
}

int8_t x509_certificate_add_subject_key_identifier(x509_certificate_t* cert, const uint8_t* skid, size_t skid_length) {
    if (cert == NULL || skid == NULL || skid_length == 0) {
        return -1;
    }

    x509_extension_t* ext = memory_malloc(sizeof(x509_extension_t));
    if (ext == NULL) {
        return -1;
    }

    ext->type = X509_EXTENSION_SKID;
    ext->is_critical = false;
    ext->data.skid.length = skid_length;
    ext->data.skid.data = memory_malloc(skid_length);
    if (ext->data.skid.data == NULL) {
        memory_free(ext);
        return -1;
    }
    memory_memcopy(skid, ext->data.skid.data, skid_length);
    ext->next = cert->extensions;
    cert->extensions = ext;

    return 0;
}

int8_t x509_certificate_add_authority_key_identifier(x509_certificate_t* cert, const uint8_t* akid, size_t akid_length) {
    if (cert == NULL || akid == NULL || akid_length == 0) {
        return -1;
    }

    x509_extension_t* ext = memory_malloc(sizeof(x509_extension_t));
    if (ext == NULL) {
        return -1;
    }

    ext->type = X509_EXTENSION_AKID;
    ext->is_critical = false;
    ext->data.akid.length = akid_length;
    ext->data.akid.data = memory_malloc(akid_length);
    if (ext->data.akid.data == NULL) {
        memory_free(ext);
        return -1;
    }
    memory_memcopy(akid, ext->data.akid.data, akid_length);
    ext->next = cert->extensions;
    cert->extensions = ext;

    return 0;
}

int8_t x509_certificate_add_public_key(x509_certificate_t* cert,
                                       x509_algorithm_t    algorithm,
                                       const uint8_t*      public_key,
                                       size_t              public_key_length) {
    if (cert == NULL || public_key == NULL || public_key_length == 0) {
        return -1;
    }

    cert->public_key_algorithm = algorithm;
    cert->public_key_length = public_key_length;
    cert->public_key = memory_malloc(public_key_length);
    if (cert->public_key == NULL) {
        return -1;
    }
    memory_memcopy(public_key, cert->public_key, public_key_length);

    return 0;
}

static int8_t x509_encode_name(der_encoder_t* der_encoder, const char_t* common_name) {
    if(!der_encoder || !common_name) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_start_set(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_encode_object_identifier(der_encoder, DER_OID_CN) != 0) {
        return -1;
    }

    if(der_encoder_encode_printable_string(der_encoder, common_name, strlen(common_name)) != 0) {
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_end_set(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_parse_ipv4address(const char_t* ip_str, uint8_t* out_bytes) {
    if (ip_str == NULL || out_bytes == NULL) {
        return -1;
    }

    const char_t* ptr = ip_str;
    for (int32_t i = 0; i < 4; i++) {
        uint32_t val = 0;
        uint8_t digits = 0;

        // Parse digits of current octet
        while (*ptr >= '0' && *ptr <= '9') {
            val = (val * 10) + (*ptr - '0');
            ptr++;
            digits++;

            // Octet cannot exceed 255 or have more than 3 digits
            if (val > 255 || digits > 3) {
                return -1;
            }
        }

        // Must have at least one digit per octet
        if (digits == 0) {
            return -1;
        }

        out_bytes[i] = (uint8_t)val;

        // Handle separators
        if (i < 3) {
            if (*ptr != '.') {
                return -1; // Expected a dot
            }
            ptr++;
        } else {
            if (*ptr != '\0') {
                return -1; // Expected end of string after 4th octet
            }
        }
    }

    return 0;
}

static int8_t x509_encode_extension_basic_conntraints(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if(!der_encoder || !ext) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_encode_boolean(der_encoder, ext->data.basic_constraints.is_ca) != 0) {
        return -1;
    }

    if (ext->data.basic_constraints.path_len >= 0) {
        if(der_encoder_encode_integer(der_encoder, (uint128_t)ext->data.basic_constraints.path_len) != 0) {
            return -1;
        }
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_extension_key_usage(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if (!der_encoder || !ext) {
        return -1;
    }

    if(der_encoder_encode_bit_string(der_encoder, (uint8_t*)&ext->data.key_usage, 1) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_extension_extended_key_usage(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if (!der_encoder || !ext) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    // OID for EKU based on type
    switch (ext->data.eku) {
    case X509_EXTENDED_KEY_USAGE_SERVER_AUTH: {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_SERVER_AUTH) != 0) {
            return -1;
        }
        break;
    }
    case X509_EXTENDED_KEY_USAGE_CLIENT_AUTH: {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_CLIENT_AUTH) != 0) {
            return -1;
        }
        break;
    }
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported EKU type");
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_extension_subject_alternative_name(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if (!der_encoder || !ext) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    x509_subject_alternative_name_t* san = ext->data.san_list;
    while (san) {
        switch (san->type) {
        case X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS: {
            if(der_encoder_encode_context_specific_string(der_encoder, 2, (uint8_t*)san->value, strlen(san->value)) != 0) {
                return -1;
            }
            break;
        }
        case X509_SUBJECT_ALTERNATIVE_NAME_TYPE_IP: {
            uint8_t ip_bytes[4];
            if (x509_parse_ipv4address(san->value, ip_bytes) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid IP address format");
                return -1;
            }
            if(der_encoder_encode_context_specific_string(der_encoder, 7, ip_bytes, 4) != 0) {
                return -1;
            }
            break;
        }
        case X509_SUBJECT_ALTERNATIVE_NAME_TYPE_EMAIL: {
            if(der_encoder_encode_context_specific_string(der_encoder, 1, (uint8_t*)san->value, strlen(san->value)) != 0) {
                return -1;
            }
            break;
        }
        default:
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported SAN type");
            return -1;
        }
        san = san->next;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_extension_skid(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if (!der_encoder || !ext) {
        return -1;
    }

    if(der_encoder_encode_octet_string(der_encoder, ext->data.skid.data, ext->data.skid.length) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_extension_akid(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if (!der_encoder || !ext) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_encode_context_specific_string(der_encoder, 0, ext->data.akid.data, ext->data.akid.length) != 0) {
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_extension_value(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if (!der_encoder || !ext) {
        return -1;
    }

    switch (ext->type) {
    case X509_EXTENSION_BASIC_CONSTRAINTS: return x509_encode_extension_basic_conntraints(der_encoder, ext);
    case X509_EXTENSION_KEY_USAGE: return x509_encode_extension_key_usage(der_encoder, ext);
    case X509_EXTENSION_EXTENDED_KEY_USAGE: return x509_encode_extension_extended_key_usage(der_encoder, ext);
    case X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME: return x509_encode_extension_subject_alternative_name(der_encoder, ext);
    case X509_EXTENSION_SKID: return x509_encode_extension_skid(der_encoder, ext);
    case X509_EXTENSION_AKID: return x509_encode_extension_akid(der_encoder, ext);
    default:
    }

    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported extension type: %d", ext->type);

    return -1;
}

static int8_t x509_encode_extensions(der_encoder_t* der_encoder, x509_certificate_t* cert) {
    if(!der_encoder || !cert) {
        return -1;
    }

    if(cert->extensions == NULL) {
        return 0; // No extensions to encode
    }

    if(der_encoder_start_explicit_tag(der_encoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 3) != 0) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    x509_extension_t* ext = cert->extensions;
    while (ext) {
        if(der_encoder_start_sequence(der_encoder) != 0) {
            return -1;
        }

        // Encode OID based on extension type
        switch (ext->type) {
        case X509_EXTENSION_BASIC_CONSTRAINTS: {
            if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EXT_BASIC_CONSTRAINTS) != 0) {
                return -1;
            }
            break;
        }
        case X509_EXTENSION_KEY_USAGE: {
            if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EXT_KEY_USAGE) != 0) {
                return -1;
            }
            break;
        }
        case X509_EXTENSION_EXTENDED_KEY_USAGE: {
            if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EXT_EXTENDED_KEY_USAGE) != 0) {
                return -1;
            }
            break;
        }
        case X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME: {
            if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EXT_SAN) != 0) {
                return -1;
            }
            break;
        }
        case X509_EXTENSION_SKID: {
            if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EXT_SKID) != 0) {
                return -1;
            }
            break;
        }
        case X509_EXTENSION_AKID: {
            if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EXT_AKID) != 0) {
                return -1;
            }
            break;
        }
        default:
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported extension type: %d", ext->type);
            return -1;
        }

        if (ext->is_critical) {
            if(der_encoder_encode_boolean(der_encoder, true) != 0) {
                return -1;
            }
        }

        if(der_encoder_start_octet_string(der_encoder) != 0) {
            return -1;
        }

        if(x509_encode_extension_value(der_encoder, ext) != 0) {
            return -1;
        }

        if(der_encoder_end_octet_string(der_encoder) != 0) {
            return -1;
        }

        if(der_encoder_end_sequence(der_encoder) != 0) {
            return -1;
        }

        ext = ext->next;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_end_explicit_tag(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_validity(der_encoder_t* der_encoder, time_t not_before, time_t not_after) {
    if(!der_encoder) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_encode_utc_time(der_encoder, not_before) != 0) {
        return -1;
    }

    if(der_encoder_encode_utc_time(der_encoder, not_after) != 0) {
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_algorithm_identifier(der_encoder_t* der_encoder, x509_algorithm_t algorithm) {
    if(!der_encoder || algorithm == 0) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    switch (algorithm) {
    case X509_ALGORITHM_ED25519: {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_ED25519) != 0) {
            return -1;
        }
        break;
    }
    case X509_ALGORITHM_X25519: {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_X25519) != 0) {
            return -1;
        }
        break;
    }
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported algorithm: %d", algorithm);
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_data_with_bit_string_with_alogrithm_identifier(der_encoder_t*   der_encoder,
                                                                         x509_algorithm_t algorithm,
                                                                         const uint8_t*   data,
                                                                         size_t           data_length) {
    if(!der_encoder || algorithm == 0 || !data || data_length == 0) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    if(x509_encode_algorithm_identifier(der_encoder, algorithm) != 0) {
        return -1;
    }

    if(der_encoder_encode_bit_string(der_encoder, data, data_length) != 0) {
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_tbs_internal(der_encoder_t* der_encoder, x509_certificate_t* cert) {
    if (!cert || !der_encoder || !cert->issuer_common_name || !cert->subject_common_name ||
        !cert->public_key || cert->not_before == 0 || cert->not_after == 0) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    // 1. Version [0] EXPLICIT INTEGER (v3 = 2)
    // Hex: A0 03 02 01 02
    if(der_encoder_start_explicit_tag(der_encoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 0) != 0) {
        return -1;
    }

    if(der_encoder_encode_integer(der_encoder, (uint128_t)cert->version) != 0) {
        return -1;
    }

    if(der_encoder_end_explicit_tag(der_encoder) != 0) {
        return -1;
    }

    // 2. Serial Number (Integer)
    if(der_encoder_encode_integer_u128(der_encoder, cert->serial_number) != 0) {
        return -1;
    }

    // 3. Signature Algorithm Identifier
    if(x509_encode_algorithm_identifier(der_encoder, cert->signature_algorithm) != 0) {
        return -1;
    }

    //// 4. Issuer (Helper for DN)
    // Encodes: SEQUENCE { SET { SEQUENCE { OID(CN), PrintableString(val) } } }
    if(x509_encode_name(der_encoder, cert->issuer_common_name) != 0) {
        return -1;
    }

    // 5. Validity
    if(x509_encode_validity(der_encoder, cert->not_before, cert->not_after) != 0) {
        return -1;
    }

    // 6. Subject (Helper for DN)
    if(x509_encode_name(der_encoder, cert->subject_common_name) != 0) {
        return -1;
    }

    // 7. SubjectPublicKeyInfo (Crucial structure for X25519)
    if(x509_encode_data_with_bit_string_with_alogrithm_identifier(der_encoder,
                                                                  cert->public_key_algorithm,
                                                                  cert->public_key,
                                                                  cert->public_key_length) != 0) {
        return -1;
    }

    // 8. Extensions [3] EXPLICIT SEQUENCE
    if(x509_encode_extensions(der_encoder, cert) != 0) {
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    if(der_encoder_get_der_data(der_encoder, &cert->tbs_data, &cert->tbs_length) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_encode_tbs(x509_certificate_t* cert) {
    der_encoder_t* der_encoder = der_encoder_new();
    if (!der_encoder) {
        return -1;
    }

    int8_t result = x509_encode_tbs_internal(der_encoder, cert);
    der_encoder_destroy(der_encoder);
    return result;
}

static int8_t x509_certificate_sign_with_ed25519(x509_certificate_t* cert,
                                                 const uint8_t*      private_key,
                                                 size_t              private_key_length) {
    if (cert == NULL || private_key == NULL || private_key_length == 0) {
        return -1;
    }

    cert->signature_algorithm = X509_ALGORITHM_ED25519;

    if (x509_encode_tbs(cert) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to encode TBS data");
        return -1;
    }

    uint8_t signature[64];
    if (ed25519_sign(signature, cert->tbs_data, cert->tbs_length, private_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to sign TBS data");
        return -1;
    }

    cert->signature_length = 64;
    cert->signature = memory_malloc(64);
    if (cert->signature == NULL) {
        return -1;
    }
    memory_memcopy(signature, cert->signature, 64);

    return 0;
}

int8_t x509_certificate_sign(x509_certificate_t* cert,
                             x509_algorithm_t    algorithm,
                             const uint8_t*      private_key,
                             size_t              private_key_length) {
    if (cert == NULL || private_key == NULL || private_key_length == 0) {
        return -1;
    }

    switch (algorithm) {
    case X509_ALGORITHM_ED25519:
        return x509_certificate_sign_with_ed25519(cert, private_key, private_key_length);
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "unsupported signature algorithm: %d", algorithm);
        return -1;
    }

    return 0;
}

static int8_t x509_certificate_assemble_internal(der_encoder_t* der_encoder, x509_certificate_t* cert) {
    if (der_encoder == NULL || cert == NULL || cert->tbs_data == NULL || cert->tbs_length == 0 || cert->signature == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "invalid certificate state for assembly");
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    // 1. TBSCertificate
    if(der_encoder_encode_raw_bytes(der_encoder, cert->tbs_data, cert->tbs_length) != 0) {
        return -1;
    }

    // 2. Signature Algorithm Identifier
    if(x509_encode_algorithm_identifier(der_encoder, cert->signature_algorithm) != 0) {
        return -1;
    }

    // 3. Signature Value
    if(der_encoder_encode_bit_string(der_encoder, cert->signature, cert->signature_length) != 0) {
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        return -1;
    }

    return 0;
}

int8_t x509_certificate_assemble(x509_certificate_t* cert) {
    if (cert == NULL) {
        return -1;
    }

    der_encoder_t* der_encoder = der_encoder_new();
    if (der_encoder == NULL) {
        return -1;
    }

    int8_t result = x509_certificate_assemble_internal(der_encoder, cert);
    if (result != 0) {
        der_encoder_destroy(der_encoder);
        return result;
    }

    result = der_encoder_get_der_data(der_encoder, &cert->certificate_data, &cert->certificate_length);
    der_encoder_destroy(der_encoder);
    return result;
}

uint8_t* x509_certificate_get_der(x509_certificate_t* cert, size_t* out_length) {
    if (cert == NULL || out_length == NULL) {
        return NULL;
    }

    if(cert->certificate_data == NULL || cert->certificate_length == 0) {
        if (x509_certificate_assemble(cert) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to assemble certificate");
            return NULL;
        }
    }

    *out_length = cert->certificate_length;

    uint8_t* der_data = memory_malloc(cert->certificate_length);
    if (der_data == NULL) {
        return NULL;
    }

    memory_memcopy(cert->certificate_data, der_data, cert->certificate_length);

    return der_data;
}

char_t* x509_certificate_get_pem(x509_certificate_t* cert) {
    if (cert == NULL) {
        return NULL;
    }

    if(cert->certificate_data == NULL || cert->certificate_length == 0) {
        if (x509_certificate_assemble(cert) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to assemble certificate");
            return NULL;
        }
    }

    size_t pem_length = 0;
    char_t* pem_data = NULL;

    if(pem_encode("CERTIFICATE", cert->certificate_data, cert->certificate_length, &pem_data, &pem_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to PEM encode certificate");
        return NULL;
    }

    return pem_data;
}
