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
#include <crypto/sha2.h>
#include <crypto/x25519.h>
#include <crypto/ellipticcurve.h>
#include <base64.h>
#include <crypto/pem.h>
#include <crypto/der.h>
#include <crypto/sha2.h>

MODULE("turnstone.lib.crypto");


typedef struct x509_subject_alternative_name_t x509_subject_alternative_name_t;

struct x509_subject_alternative_name_t {
    x509_subject_alternative_name_type_t type;
    char_t*                              value;
    x509_subject_alternative_name_t*     next;
};

typedef struct x509_extension_t x509_extension_t;

struct x509_extension_t {
    boolean_t             is_valid;
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

        // X509_EXTENSION_NETSCAPE_CERT_TYPE
        x509_netscape_cert_type_t netscape_cert_type;

    } data;
};

static const der_object_identifier_t ISSUER_SUBJECT_FIELD_OIDS[X509_ISSUER_SUBJECT_FIELD_COUNT] = {
    [X509_ISSUER_SUBJECT_FIELD_UNKNOWN] = DER_OID_UNDEFINED,
    [X509_ISSUER_SUBJECT_FIELD_ORGANIZATION] = DER_OID_ORGANIZATION,
    [X509_ISSUER_SUBJECT_FIELD_ORGANIZATIONAL_UNIT] = DER_OID_ORGANIZATIONAL_UNIT,
    [X509_ISSUER_SUBJECT_FIELD_COUNTRY] = DER_OID_COUNTRY,
    [X509_ISSUER_SUBJECT_FIELD_COMMON_NAME] = DER_OID_CN,
};

struct x509_certificate_t {
    uint32_t version;
    uint8_t  serial_number[20]; // up to 160 bits

    // Issuer and Subject Info
    char_t* issuer[X509_ISSUER_SUBJECT_FIELD_COUNT];
    char_t* subject[X509_ISSUER_SUBJECT_FIELD_COUNT];

    time_t not_before;
    time_t not_after;

    // The Extension Chain
    x509_extension_t extensions[X509_EXTENSION_COUNT];

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
    uint128_t pre_serial_number = ((uint128_t)time_low << 96) | ((uint128_t)time_mid << 80) | ((uint128_t)time_hi_and_version << 64) | ((uint128_t)clock_seq << 48) | (uint128_t)node;

    get_random_bytes(cert->serial_number, 20 - sizeof(uint128_t)); // pad with random bytes
    memory_memcopy(&pre_serial_number, cert->serial_number + 20 - sizeof(uint128_t), sizeof(uint128_t)); // append the generated part

    return cert;
}

void x509_certificate_free(x509_certificate_t* cert) {
    if (cert == NULL) {
        return;
    }

    // Free issuer and subject
    for (size_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
        if (cert->issuer[i] != NULL) {
            memory_free(cert->issuer[i]);
        }
        if (cert->subject[i] != NULL) {
            memory_free(cert->subject[i]);
        }
    }

    // Free extensions
    for (size_t i = 0; i < X509_EXTENSION_COUNT; i++) {
        x509_extension_t* ext = &cert->extensions[i];
        if (ext->is_valid) {
            if (ext->type == X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME) {
                x509_subject_alternative_name_t* san_entry = ext->data.san_list;
                while (san_entry != NULL) {
                    x509_subject_alternative_name_t* next_entry = san_entry->next;
                    if (san_entry->value != NULL) {
                        memory_free(san_entry->value);
                    }
                    memory_free(san_entry);
                    san_entry = next_entry;
                }
            } else if (ext->type == X509_EXTENSION_SKID) {
                if (ext->data.skid.data != NULL) {
                    memory_free(ext->data.skid.data);
                }
            } else if (ext->type == X509_EXTENSION_AKID) {
                if (ext->data.akid.data != NULL) {
                    memory_free(ext->data.akid.data);
                }
            }
        }
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

static int8_t x509_encode_dn(der_encoder_t* der_encoder, char_t* fields[X509_ISSUER_SUBJECT_FIELD_COUNT]) {
    if(!der_encoder || !fields) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    for (size_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
        if (fields[i] == NULL) {
            continue; // skip empty fields
        }

        if(der_encoder_start_set(der_encoder) != 0) {
            return -1;
        }

        if(der_encoder_start_sequence(der_encoder) != 0) {
            return -1;
        }

        if(der_encoder_encode_object_identifier(der_encoder, ISSUER_SUBJECT_FIELD_OIDS[i]) != 0) {
            return -1;
        }

        if(der_encoder_encode_printable_string(der_encoder, fields[i], strlen(fields[i])) != 0) {
            return -1;
        }

        if(der_encoder_end_sequence(der_encoder) != 0) {
            return -1;
        }

        if(der_encoder_end_set(der_encoder) != 0) {
            return -1;
        }
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

    uint8_t ku_byte = 0;
    uint32_t flags  = ext->data.key_usage;

    // Use the ENUM constants to check flags and construct the byte.
    // Since your enum matches ASN.1 bit positions (0x80, 0x40...),
    // we can OR them directly.

    if (flags & X509_KEY_USAGE_DIGITAL_SIGNATURE) {
        ku_byte |= X509_KEY_USAGE_DIGITAL_SIGNATURE;
    }

    if (flags & X509_KEY_USAGE_NON_REPUDIATION) {
        ku_byte |= X509_KEY_USAGE_NON_REPUDIATION;
    }

    if (flags & X509_KEY_USAGE_KEY_ENCIPHERMENT) {
        ku_byte |= X509_KEY_USAGE_KEY_ENCIPHERMENT;
    }

    if (flags & X509_KEY_USAGE_DATA_ENCIPHERMENT) {
        ku_byte |= X509_KEY_USAGE_DATA_ENCIPHERMENT;
    }

    if (flags & X509_KEY_USAGE_KEY_AGREEMENT) {
        ku_byte |= X509_KEY_USAGE_KEY_AGREEMENT;
    }

    if (flags & X509_KEY_USAGE_KEY_CERT_SIGN) {
        ku_byte |= X509_KEY_USAGE_KEY_CERT_SIGN;
    }

    if (flags & X509_KEY_USAGE_CRL_SIGN) {
        ku_byte |= X509_KEY_USAGE_CRL_SIGN;
    }

    if (flags & X509_KEY_USAGE_ENCIPHER_ONLY) {
        ku_byte |= X509_KEY_USAGE_ENCIPHER_ONLY;
    }

    // Pass the constructed byte to your encoder.
    // Note: Your bit_string encoder adds the "Unused Bits: 0" byte automatically,
    // which is valid here (asserting the unused bits are effectively 0/False).
    if(der_encoder_encode_bit_string(der_encoder, &ku_byte, 1) != 0) {
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
    if(ext->data.eku & X509_EXTENDED_KEY_USAGE_SERVER_AUTH) {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_SERVER_AUTH) != 0) {
            return -1;
        }
    }

    if(ext->data.eku & X509_EXTENDED_KEY_USAGE_CLIENT_AUTH) {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_CLIENT_AUTH) != 0) {
            return -1;
        }
    }

    if(ext->data.eku & X509_EXTENDED_KEY_USAGE_CODE_SIGNING) {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_CODE_SIGNING) != 0) {
            return -1;
        }
    }

    if(ext->data.eku & X509_EXTENDED_KEY_USAGE_EMAIL_PROTECTION) {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EMAIL_PROTECTION) != 0) {
            return -1;
        }
    }

    if(ext->data.eku & X509_EXTENDED_KEY_USAGE_TIME_STAMPING) {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_TIME_STAMPING) != 0) {
            return -1;
        }
    }

    if(ext->data.eku & X509_EXTENDED_KEY_USAGE_OCSP_SIGNING) {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_OCSP_SIGNING) != 0) {
            return -1;
        }
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

static int8_t x509_encode_extension_netscape_cert_type(der_encoder_t* der_encoder, x509_extension_t* ext) {
    if (!der_encoder || !ext) {
        return -1;
    }

    uint8_t ku_byte = 0;
    uint32_t flags  = ext->data.netscape_cert_type;

    // Use the ENUM constants to check flags and construct the byte.
    // Since your enum matches ASN.1 bit positions (0x80, 0x40...),
    // we can OR them directly.

    if (flags & X509_NETSCAPE_CERT_TYPE_SSL_CLIENT) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_SSL_CLIENT;
    }

    if (flags & X509_NETSCAPE_CERT_TYPE_SSL_SERVER) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_SSL_SERVER;
    }

    if (flags & X509_NETSCAPE_CERT_TYPE_SMIME) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_SMIME;
    }

    if (flags & X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING;
    }

    if (flags & X509_NETSCAPE_CERT_TYPE_RESERVED) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_RESERVED;
    }

    if (flags & X509_NETSCAPE_CERT_TYPE_SSL_CA) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_SSL_CA;
    }

    if (flags & X509_NETSCAPE_CERT_TYPE_SMIME_CA) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_SMIME_CA;
    }

    if (flags & X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING_CA) {
        ku_byte |= X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING_CA;
    }

// Pass the constructed byte to your encoder.
// Note: Your bit_string encoder adds the "Unused Bits: 0" byte automatically,
// which is valid here (asserting the unused bits are effectively 0/False).
    if(der_encoder_encode_bit_string(der_encoder, &ku_byte, 1) != 0) {
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
    case X509_EXTENSION_NETSCAPE_CERT_TYPE: return x509_encode_extension_netscape_cert_type(der_encoder, ext);
    default:
    }

    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported extension type: %d", ext->type);

    return -1;
}

static int8_t x509_encode_extensions(der_encoder_t* der_encoder, x509_certificate_t* cert) {
    if(!der_encoder || !cert) {
        return -1;
    }

    boolean_t has_extensions = false;

    for (size_t i = 0; i < X509_EXTENSION_COUNT; i++) {
        if (cert->extensions[i].is_valid) {
            has_extensions = true;
            break;
        }
    }

    if (!has_extensions) {
        return 0; // No extensions to encode
    }

    if(der_encoder_start_explicit_tag(der_encoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 3) != 0) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        return -1;
    }

    for (size_t i = 0; i < X509_EXTENSION_COUNT; i++) {
        x509_extension_t* ext = &cert->extensions[i];
        if (!ext->is_valid) {
            continue;
        }

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
        case X509_EXTENSION_NETSCAPE_CERT_TYPE: {
            if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EXT_NETSCAPE_CERT_TYPE) != 0) {
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
    case X509_ALGORITHM_ECDSA_WITH_SHA256: {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_ECDSA_WITH_SHA256) != 0) {
            return -1;
        }
        break;
    }
    case X509_ALGORITHM_ECDSA_SECP256R1_SHA256: {
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_ECDSA_PUBLIC_KEY) != 0) {
            return -1;
        }
        if(der_encoder_encode_object_identifier(der_encoder, DER_OID_EC_SECP256R1) != 0) {
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
    if (!cert || !der_encoder ||
        !cert->public_key || cert->not_before == 0 || cert->not_after == 0) {
        return -1;
    }

    // min one field is not null in issuer and subject
    boolean_t has_issuer_field  = false;
    boolean_t has_subject_field = false;
    for (size_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
        if (cert->issuer[i] != NULL) {
            has_issuer_field = true;
        }
        if (cert->subject[i] != NULL) {
            has_subject_field = true;
        }
    }

    if (!has_issuer_field || !has_subject_field) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Issuer and Subject must have at least one field set");
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start TBS sequence");
        return -1;
    }

    // 1. Version [0] EXPLICIT INTEGER (v3 = 2)
    // Hex: A0 03 02 01 02
    if(der_encoder_start_explicit_tag(der_encoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 0) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start version explicit tag");
        return -1;
    }

    if(der_encoder_encode_integer(der_encoder, (uint128_t)cert->version) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode version integer");
        return -1;
    }

    if(der_encoder_end_explicit_tag(der_encoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end version explicit tag");
        return -1;
    }

    // 2. Serial Number (Integer)
    if(der_encoder_encode_integer_u160(der_encoder, cert->serial_number) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode serial number");
        return -1;
    }

    // 3. Signature Algorithm Identifier
    if(x509_encode_algorithm_identifier(der_encoder, cert->signature_algorithm) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode signature algorithm identifier");
        return -1;
    }

    //// 4. Issuer (Helper for DN)
    // Encodes: SEQUENCE { SET { SEQUENCE { OID(CN), PrintableString(val) } } }
    if(x509_encode_dn(der_encoder, cert->issuer) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode issuer DN");
        return -1;
    }

    // 5. Validity
    if(x509_encode_validity(der_encoder, cert->not_before, cert->not_after) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode validity");
        return -1;
    }

    // 6. Subject (Helper for DN)
    if(x509_encode_dn(der_encoder, cert->subject) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode subject DN");
        return -1;
    }

    // 7. SubjectPublicKeyInfo
    if(x509_encode_data_with_bit_string_with_alogrithm_identifier(der_encoder,
                                                                  cert->public_key_algorithm,
                                                                  cert->public_key,
                                                                  cert->public_key_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode subject public key info");
        return -1;
    }

    // 8. Extensions [3] EXPLICIT SEQUENCE
    if(x509_encode_extensions(der_encoder, cert) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode extensions");
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end TBS sequence");
        return -1;
    }

    if(der_encoder_get_der_data(der_encoder, &cert->tbs_data, &cert->tbs_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get DER data for TBS");
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

    uint8_t public_key[32];
    if (ed25519_derive_pubkey(public_key, private_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to derive public key from private key");
        return -1;
    }

    if(ed25519_verify(signature, cert->tbs_data, cert->tbs_length, public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "signature verification failed after signing");
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

static int8_t x509_certificate_sign_with_ecdsa_secp256r1(x509_certificate_t* cert,
                                                         const uint8_t*      private_key,
                                                         size_t              private_key_length) {
    if (cert == NULL || private_key == NULL || private_key_length == 0) {
        return -1;
    }

    cert->signature_algorithm = X509_ALGORITHM_ECDSA_WITH_SHA256;

    if (x509_encode_tbs(cert) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to encode TBS data");
        return -1;
    }

    uint8_t signature[64];
    if (ellipticcurve_secp256r1_sign(signature, cert->tbs_data, cert->tbs_length, private_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to sign TBS data with ECDSA SECP256R1");
        return -1;
    }

    uint8_t public_key[64];
    if (ellipticcurve_secp256r1_derive_pubkey(public_key, private_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to derive public key from private key for ECDSA SECP256R1");
        return -1;
    }

    if(ellipticcurve_secp256r1_verify(signature, cert->tbs_data, cert->tbs_length, public_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "ECDSA SECP256R1 signature verification failed after signing");
        return -1;
    }

    // ECDSA signatures are typically encoded as SEQUENCE { r INTEGER, s INTEGER }
    // We will encode it in that format for the certificate.
    der_encoder_t* der_encoder = der_encoder_new();
    if (der_encoder == NULL) {
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(der_encoder_encode_integer_u256(der_encoder, signature) != 0) {
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(der_encoder_encode_integer_u256(der_encoder, signature + 32) != 0) {
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(der_encoder_get_der_data(der_encoder, &cert->signature, &cert->signature_length) != 0) {
        der_encoder_destroy(der_encoder);
        return -1;
    }

    der_encoder_destroy(der_encoder);

    return 0;
}

static int8_t x509_compute_subject_authority_key_identifier(x509_algorithm_t algorithm,
                                                            const uint8_t*   public_key,
                                                            size_t           public_key_length,
                                                            uint8_t*         out_skid,
                                                            size_t           out_skid_length) {
    if (public_key == NULL || public_key_length == 0 || out_skid == NULL || out_skid_length != SHA256_OUTPUT_SIZE) {
        return -1;
    }

    const uint8_t* data_to_hash = NULL;
    size_t data_length = 0;

    if(algorithm == X509_ALGORITHM_ED25519 || algorithm == X509_ALGORITHM_X25519) {
        // For Ed25519 and X25519, the public key is used as-is for SKID/AKID computation
        data_to_hash = public_key;
        data_length  = public_key_length;
    } else if (algorithm == X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
        // For ECDSA SECP256R1, we should use ASN.1 DER-encoded SubjectPublicKeyInfo for SKID/AKID computation
        der_encoder_t* der_encoder = der_encoder_new();
        if (der_encoder == NULL) {
            return -1;
        }

        if(x509_encode_data_with_bit_string_with_alogrithm_identifier(der_encoder, algorithm, public_key, public_key_length) != 0) {
            der_encoder_destroy(der_encoder);
            return -1;
        }

        uint8_t* der_data = NULL;

        if(der_encoder_get_der_data(der_encoder, &der_data, &data_length) != 0) {
            der_encoder_destroy(der_encoder);
            return -1;
        }

        der_encoder_destroy(der_encoder);

        data_to_hash = der_data; // Use the DER-encoded SubjectPublicKeyInfo for hashing

    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "unsupported algorithm for SKID computation: %d", algorithm);
        return -1;
    }

    // Simple SKID computation: SHA-256 hash of the public key
    uint8_t* hash = sha256_hash(data_to_hash, data_length);
    if (hash == NULL) {
        if(algorithm == X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
            memory_free((void*)data_to_hash); // Free the DER-encoded data if we allocated it
        }
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to compute SHA-256 hash for SKID");
        return -1;
    }

    memory_memcopy(hash, out_skid, out_skid_length);

    if(algorithm == X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
        memory_free((void*)data_to_hash); // Free the DER-encoded data if we allocated it
    }

    memory_free(hash);

    return 0;
}

static int8_t x509_certificate_add_authority_key_identifier(x509_certificate_t* cert, x509_certificate_t* ca_cert) {

    cert->extensions[X509_EXTENSION_AKID].is_valid = true;
    cert->extensions[X509_EXTENSION_AKID].type = X509_EXTENSION_AKID;
    cert->extensions[X509_EXTENSION_AKID].is_critical = false;
    cert->extensions[X509_EXTENSION_AKID].data.akid.length = SHA256_OUTPUT_SIZE;
    cert->extensions[X509_EXTENSION_AKID].data.akid.data = memory_malloc(SHA256_OUTPUT_SIZE);
    if (cert->extensions[X509_EXTENSION_AKID].data.akid.data == NULL) {
        cert->extensions[X509_EXTENSION_AKID].is_valid = false;
        return -1;
    }

    if (x509_compute_subject_authority_key_identifier(ca_cert->public_key_algorithm,
                                                      ca_cert->public_key,
                                                      ca_cert->public_key_length,
                                                      cert->extensions[X509_EXTENSION_AKID].data.akid.data,
                                                      cert->extensions[X509_EXTENSION_AKID].data.akid.length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to compute authority key identifier");
        cert->extensions[X509_EXTENSION_AKID].is_valid = false;
        memory_free(cert->extensions[X509_EXTENSION_AKID].data.akid.data);
        return -1;
    }

    return 0;
}

int8_t x509_certificate_sign(x509_certificate_t* cert,
                             x509_certificate_t* ca_cert,
                             x509_algorithm_t    algorithm,
                             const uint8_t*      private_key,
                             size_t              private_key_length) {
    if (cert == NULL || private_key == NULL || private_key_length == 0) {
        return -1;
    }

    if(ca_cert == NULL) {
        ca_cert = cert; // Self-signed
    }

    if (x509_certificate_add_authority_key_identifier(cert, ca_cert) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to add authority key identifier");
        return -1;
    }

    switch (algorithm) {
    case X509_ALGORITHM_ED25519:
        return x509_certificate_sign_with_ed25519(cert, private_key, private_key_length);
    case X509_ALGORITHM_ECDSA_SECP256R1_SHA256:
        return x509_certificate_sign_with_ecdsa_secp256r1(cert, private_key, private_key_length);
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "unsupported signature algorithm: %d", algorithm);
        return -1;
    }

    return 0;
}

int8_t x509_certificate_add_issuer_field(x509_certificate_t* cert, x509_issuer_subject_field_t field, const char_t* value){
    if (cert == NULL || value == NULL || field <= X509_ISSUER_SUBJECT_FIELD_UNKNOWN || field >= X509_ISSUER_SUBJECT_FIELD_COUNT) {
        return -1;
    }

    cert->issuer[field] = strdup(value);
    if (cert->issuer[field] == NULL) {
        return -1;
    }

    return 0;
}

int8_t x509_certificate_add_subject_field(x509_certificate_t* cert, x509_issuer_subject_field_t field, const char_t* value){
    if (cert == NULL || value == NULL || field <= X509_ISSUER_SUBJECT_FIELD_UNKNOWN || field >= X509_ISSUER_SUBJECT_FIELD_COUNT) {
        return -1;
    }

    cert->subject[field] = strdup(value);
    if (cert->subject[field] == NULL) {
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
    cert->not_after  = now + (days_valid * 24 * 60 * 60 * 1000000000ULL); // days to nanoseconds
    return 0;
}

int8_t x509_certificate_set_is_ca(x509_certificate_t* cert, boolean_t is_ca, int32_t path_len) {
    if (cert == NULL) {
        return -1;
    }

    cert->extensions[X509_EXTENSION_BASIC_CONSTRAINTS].is_valid = true;
    cert->extensions[X509_EXTENSION_BASIC_CONSTRAINTS].type = X509_EXTENSION_BASIC_CONSTRAINTS;
    cert->extensions[X509_EXTENSION_BASIC_CONSTRAINTS].is_critical = true;
    cert->extensions[X509_EXTENSION_BASIC_CONSTRAINTS].data.basic_constraints.is_ca = is_ca;
    cert->extensions[X509_EXTENSION_BASIC_CONSTRAINTS].data.basic_constraints.path_len = path_len;

    return 0;
}

int8_t x509_certificate_add_key_usage(x509_certificate_t* cert, x509_key_usage_t key_usage) {
    if (cert == NULL) {
        return -1;
    }

    cert->extensions[X509_EXTENSION_KEY_USAGE].is_valid = true;
    cert->extensions[X509_EXTENSION_KEY_USAGE].type = X509_EXTENSION_KEY_USAGE;
    cert->extensions[X509_EXTENSION_KEY_USAGE].is_critical = true;
    cert->extensions[X509_EXTENSION_KEY_USAGE].data.key_usage = key_usage;

    return 0;
}

int8_t x509_certificate_add_extended_key_usage(x509_certificate_t* cert, x509_extended_key_usage_t eku) {
    if (cert == NULL) {
        return -1;
    }

    cert->extensions[X509_EXTENSION_EXTENDED_KEY_USAGE].is_valid = true;
    cert->extensions[X509_EXTENSION_EXTENDED_KEY_USAGE].type = X509_EXTENSION_EXTENDED_KEY_USAGE;
    cert->extensions[X509_EXTENSION_EXTENDED_KEY_USAGE].is_critical = false;
    cert->extensions[X509_EXTENSION_EXTENDED_KEY_USAGE].data.eku = eku;

    return 0;
}

int8_t x509_certificate_add_subject_alternative_name(x509_certificate_t* cert, x509_subject_alternative_name_type_t type, const char_t* value) {
    if (cert == NULL || value == NULL) {
        return -1;
    }

    x509_extension_t* ext = &cert->extensions[X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME];

    if (!ext->is_valid) {
        ext->is_valid = true;
        ext->type = X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME;
        ext->is_critical = false;
        ext->data.san_list = NULL;
    }

    x509_subject_alternative_name_t* san_entry = memory_malloc(sizeof(x509_subject_alternative_name_t));
    if (san_entry == NULL) {
        ext->is_valid = false;
        return -1;
    }

    san_entry->type  = type;
    san_entry->value = strdup(value);
    if (san_entry->value == NULL) {
        ext->is_valid = false;
        memory_free(san_entry);
        return -1;
    }

    san_entry->next = ext->data.san_list;
    ext->data.san_list = san_entry;

    return 0;
}

static int8_t x509_certificate_add_subject_key_identifier(x509_certificate_t* cert) {
    cert->extensions[X509_EXTENSION_SKID].is_valid = true;
    cert->extensions[X509_EXTENSION_SKID].type = X509_EXTENSION_SKID;
    cert->extensions[X509_EXTENSION_SKID].is_critical = false;
    cert->extensions[X509_EXTENSION_SKID].data.skid.length = SHA256_OUTPUT_SIZE;
    cert->extensions[X509_EXTENSION_SKID].data.skid.data = memory_malloc(SHA256_OUTPUT_SIZE);
    if (cert->extensions[X509_EXTENSION_SKID].data.skid.data == NULL) {
        cert->extensions[X509_EXTENSION_SKID].is_valid = false;
        return -1;
    }

    if(x509_compute_subject_authority_key_identifier(cert->public_key_algorithm,
                                                     cert->public_key, cert->public_key_length,
                                                     cert->extensions[X509_EXTENSION_SKID].data.skid.data,
                                                     cert->extensions[X509_EXTENSION_SKID].data.skid.length) != 0) {
        cert->extensions[X509_EXTENSION_SKID].is_valid = false;
        memory_free(cert->extensions[X509_EXTENSION_SKID].data.skid.data);
        return -1;
    }

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

    return x509_certificate_add_subject_key_identifier(cert);
}

static int8_t x509_decode_secp256r1_signature(x509_certificate_t* cert, uint8_t* out_signature, size_t* out_signature_length) {
    if (cert == NULL || cert->signature == NULL || cert->signature_length == 0 ||
        out_signature == NULL || out_signature_length == NULL) {
        return -1;
    }

    der_decoder_t* decoder = der_decoder_new(cert->signature, cert->signature_length);
    if (decoder == NULL) {
        return -1;
    }

    // ECDSA signatures are encoded as SEQUENCE { r INTEGER, s INTEGER }
    if (der_decoder_start_sequence(decoder) != 0) {
        der_decoder_destroy(decoder);
        return -1;
    }

    uint8_t r[32];
    if (der_decoder_decode_integer_u256(decoder, r) != 0) {
        der_decoder_destroy(decoder);
        return -1;
    }

    uint8_t s[32];
    if (der_decoder_decode_integer_u256(decoder, s) != 0) {
        der_decoder_destroy(decoder);
        return -1;
    }

    if (der_decoder_end_sequence(decoder) != 0) {
        der_decoder_destroy(decoder);
        return -1;
    }

    der_decoder_destroy(decoder);

    memory_memcopy(r, out_signature, 32);
    memory_memcopy(s, out_signature + 32, 32);
    *out_signature_length = 64;

    return 0;
}

static int8_t x509_certificate_verify_signature_internal(x509_certificate_t* cert,
                                                         const uint8_t*      public_key,
                                                         size_t              public_key_length) {
    if (cert == NULL || public_key == NULL || public_key_length == 0 ||
        cert->tbs_data == NULL || cert->tbs_length == 0 ||
        cert->signature == NULL || cert->signature_length == 0) {
        return -1;
    }

    switch (cert->signature_algorithm) {
    case X509_ALGORITHM_ED25519: {
        if (ed25519_verify(cert->signature, cert->tbs_data, cert->tbs_length, public_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Signature verification failed on raw data");
            return -1;
        }
        break;
    }
    case X509_ALGORITHM_ECDSA_WITH_SHA256: {
        if(cert->public_key_algorithm != X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Algorithm Mismatch: Signature algorithm is ECDSA with SHA-256 but public key is not ECDSA SECP256R1");
            return -1;
        }
        uint8_t signature[64];
        size_t signature_length = 0;
        if (x509_decode_secp256r1_signature(cert, signature, &signature_length) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode ECDSA signature from certificate");
            return -1;
        }
        if(ellipticcurve_secp256r1_verify(signature, cert->tbs_data, cert->tbs_length, public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "ECDSA Signature verification failed on raw data");
            return -1;
        }
        break;
    }
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported signature algorithm: %d", cert->signature_algorithm);
        return -1;
    }

    return 0;
}

int8_t x509_certificate_verify_signature_with_rebuild(x509_certificate_t* cert,
                                                      x509_algorithm_t    public_key_algorithm,
                                                      const uint8_t*      public_key,
                                                      size_t              public_key_length,
                                                      boolean_t           rebuild) {
    if (cert == NULL || public_key == NULL || public_key_length == 0 ||
        cert->tbs_data == NULL || cert->tbs_length == 0 ||
        cert->signature == NULL || cert->signature_length == 0) {
        return -1;
    }

    // --- Step 1: Authority Key Identifier (AKID) Link Check ---
    // Verifies: "Is 'public_key' really the parent of this cert?"
    x509_extension_t* ext = &cert->extensions[X509_EXTENSION_AKID];
    if (ext->is_valid) {
        if (ext->data.akid.length != SHA256_OUTPUT_SIZE) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "AKID Error: Legacy/Invalid length %llu (Strict SHA-256 required)", ext->data.akid.length);
            return -1;
        }

        uint8_t derived_akid[SHA256_OUTPUT_SIZE];
        if (x509_compute_subject_authority_key_identifier(public_key_algorithm,
                                                          public_key,
                                                          public_key_length,
                                                          derived_akid,
                                                          sizeof(derived_akid)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to hash parent public key for AKID check");
            return -1;
        }

        if (memory_memcompare(ext->data.akid.data, derived_akid, sizeof(derived_akid)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "AKID Mismatch: Provided key is not the issuer");
            return -1;
        }
    }

    // --- Step 2: Subject Key Identifier (SKID) Integrity Check ---
    // Verifies: "Is this cert's internal key ID consistent with its actual key?"
    ext = &cert->extensions[X509_EXTENSION_SKID];
    if (ext->is_valid) {
        if (ext->data.skid.length != SHA256_OUTPUT_SIZE) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "SKID Error: Legacy/Invalid length %llu (Strict SHA-256 required)", ext->data.skid.length);
            return -1;
        }

        uint8_t derived_skid[SHA256_OUTPUT_SIZE];
        if (x509_compute_subject_authority_key_identifier(cert->public_key_algorithm,
                                                          cert->public_key,
                                                          cert->public_key_length,
                                                          derived_skid,
                                                          sizeof(derived_skid)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to hash subject public key for SKID check");
            return -1;
        }

        if (memory_memcompare(ext->data.skid.data, derived_skid, sizeof(derived_skid)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "SKID Integrity Error: Extension does not match public key");
            return -1;
        }
    }

    // --- Step 3: Raw Verification (The "Real" Verify) ---
    // We must ALWAYS verify the raw bytes first. If this fails, the cert is 100% invalid.
    if (x509_certificate_verify_signature_internal(cert, public_key, public_key_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Raw signature verification failed");
        return -1;
    }

    // --- Step 4: Re-encoding Check (Optional Strict Mode) ---
    // This ensures your internal struct perfectly captures the DER representation.
    if (rebuild) {
        // Free raw data to force reconstruction
        uint8_t* old_tbs_data = cert->tbs_data;
        size_t old_tbs_length = cert->tbs_length;

        cert->tbs_data = NULL;
        cert->tbs_length = 0;

        if (x509_encode_tbs(cert) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to rebuild TBS data");
            cert->tbs_data = old_tbs_data; // Restore old data on failure
            cert->tbs_length = old_tbs_length;
            return -1;
        }

        if (x509_certificate_verify_signature_internal(cert, public_key, public_key_length) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Signature verification failed after rebuilding TBS data");
            memory_free(cert->tbs_data); // Free the newly built TBS data
            cert->tbs_data = old_tbs_data; // Restore old data on failure
            cert->tbs_length = old_tbs_length;
            return -1;
        }

        // If we reach here, the rebuild was successful and consistent with the original signature
        memory_free(old_tbs_data); // Free the old TBS data as it's no longer needed
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
    char_t* pem_data  = NULL;

    if(pem_encode("CERTIFICATE", cert->certificate_data, cert->certificate_length, &pem_data, &pem_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to PEM encode certificate");
        return NULL;
    }

    return pem_data;
}

static int8_t x509_decode_dn(der_decoder_t* der_decoder, char_t* values[X509_ISSUER_SUBJECT_FIELD_COUNT]) {
    if(!der_decoder || !values) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start sequence for DN");
        return -1;
    }

    while(!der_decoder_has_container_ended(der_decoder)) {
        if(der_decoder_start_set(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start set for DN");
            return -1;
        }

        if(der_decoder_start_sequence(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start inner sequence for DN");
            return -1;
        }

        der_object_identifier_t oid;

        if(der_decoder_decode_object_identifier(der_decoder, &oid) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode OID for DN");
            return -1;
        }

        size_t name_length = 0;
        char_t* value = NULL;

        if(der_decoder_decode_printable_string(der_decoder, &value, &name_length) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode PrintableString for CN");
            return -1;
        }

        if(strlen(value) != name_length) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "CN length mismatch: expected %llu, got %llu", name_length, strlen(value));
            memory_free(value);
            return -1;
        }

        switch (oid) {
        case DER_OID_ORGANIZATION:
            values[X509_ISSUER_SUBJECT_FIELD_ORGANIZATION] = value;
            break;
        case DER_OID_ORGANIZATIONAL_UNIT:
            values[X509_ISSUER_SUBJECT_FIELD_ORGANIZATIONAL_UNIT] = value;
            break;
        case DER_OID_COUNTRY:
            values[X509_ISSUER_SUBJECT_FIELD_COUNTRY] = value;
            break;
        case DER_OID_CN:
            values[X509_ISSUER_SUBJECT_FIELD_COMMON_NAME] = value;
            break;
        default:
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Unknown OID in DN: %d", oid);
            memory_free(value);
            break;
        }

        if(der_decoder_end_sequence(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end inner sequence for DN");
            return -1;
        }

        if(der_decoder_end_set(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end set for DN");
            return -1;
        }
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end sequence for DN");
        return -1;
    }

    return 0;
}

static int8_t x509_decode_extension_basic_conntraints(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if(!der_decoder || !ext) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start sequence for Basic Constraints");
        return -1;
    }

    if(!der_decoder_has_container_ended(der_decoder)) {
        if(der_decoder_decode_boolean(der_decoder, &ext->data.basic_constraints.is_ca) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode is_ca for Basic Constraints");
            return -1;
        }
    } else {
        ext->data.basic_constraints.is_ca = false;
    }

    if(!der_decoder_has_container_ended(der_decoder)) {
        int64_t path_len = 0;
        if(der_decoder_decode_integer(der_decoder, &path_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode path_len for Basic Constraints");
            return -1;
        }
        ext->data.basic_constraints.path_len = (int32_t)path_len;
    } else {
        ext->data.basic_constraints.path_len = -1;
    }


    if(der_decoder_end_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end sequence for Basic Constraints");
        return -1;
    }

    return 0;
}

static int8_t x509_decode_extension_key_usage(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if (!der_decoder || !ext) {
        return -1;
    }

    size_t flags_length = 0;
    uint8_t* flags_data = NULL;

    if(der_decoder_decode_bit_string(der_decoder, &flags_data, &flags_length) != 0) {
        return -1;
    }

    if (flags_length != 1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid key usage length: %llu", flags_length);
        memory_free(flags_data);
        return -1;
    }

    uint8_t ku_byte = flags_data[0];
    memory_free(flags_data);

    uint32_t flags = 0;

    // Use the ENUM constants to check flags and construct the byte.
    // Since your enum matches ASN.1 bit positions (0x80, 0x40...),
    // we can OR them directly.

    if (ku_byte & X509_KEY_USAGE_DIGITAL_SIGNATURE) {
        flags |= X509_KEY_USAGE_DIGITAL_SIGNATURE;
    }

    if (ku_byte & X509_KEY_USAGE_NON_REPUDIATION) {
        flags |= X509_KEY_USAGE_NON_REPUDIATION;
    }

    if (ku_byte & X509_KEY_USAGE_KEY_ENCIPHERMENT) {
        flags |= X509_KEY_USAGE_KEY_ENCIPHERMENT;
    }

    if (ku_byte & X509_KEY_USAGE_DATA_ENCIPHERMENT) {
        flags |= X509_KEY_USAGE_DATA_ENCIPHERMENT;
    }

    if (ku_byte & X509_KEY_USAGE_KEY_AGREEMENT) {
        flags |= X509_KEY_USAGE_KEY_AGREEMENT;
    }

    if (ku_byte & X509_KEY_USAGE_KEY_CERT_SIGN) {
        flags |= X509_KEY_USAGE_KEY_CERT_SIGN;
    }

    if (ku_byte & X509_KEY_USAGE_CRL_SIGN) {
        flags |= X509_KEY_USAGE_CRL_SIGN;
    }

    if (ku_byte & X509_KEY_USAGE_ENCIPHER_ONLY) {
        flags |= X509_KEY_USAGE_ENCIPHER_ONLY;
    }

    ext->data.key_usage = flags;

    return 0;
}

static int8_t x509_decode_extension_extended_key_usage(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if (!der_decoder || !ext) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        return -1;
    }

    x509_extended_key_usage_t eku_flags = X509_EXTENDED_KEY_USAGE_UNKNOWN;

    while(!der_decoder_has_container_ended(der_decoder)) {
        der_object_identifier_t oid;
        if(der_decoder_decode_object_identifier(der_decoder, &oid) != 0) {
            return -1;
        }

        switch (oid) {
        case DER_OID_SERVER_AUTH:
            eku_flags |= X509_EXTENDED_KEY_USAGE_SERVER_AUTH;
            break;
        case DER_OID_CLIENT_AUTH:
            eku_flags |= X509_EXTENDED_KEY_USAGE_CLIENT_AUTH;
            break;
        case DER_OID_CODE_SIGNING:
            eku_flags |= X509_EXTENDED_KEY_USAGE_CODE_SIGNING;
            break;
        case DER_OID_EMAIL_PROTECTION:
            eku_flags |= X509_EXTENDED_KEY_USAGE_EMAIL_PROTECTION;
            break;
        case DER_OID_TIME_STAMPING:
            eku_flags |= X509_EXTENDED_KEY_USAGE_TIME_STAMPING;
            break;
        case DER_OID_OCSP_SIGNING:
            eku_flags |= X509_EXTENDED_KEY_USAGE_OCSP_SIGNING;
            break;
        default:
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported EKU OID: %d", oid);
            return -1;
        }
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        return -1;
    }

    ext->data.eku = eku_flags;

    return 0;
}

static int8_t x509_decode_extension_subject_alternative_name(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if (!der_decoder || !ext) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        return -1;
    }

    while(!der_decoder_has_container_ended(der_decoder)) {
        if(der_decoder_has_context_specific_tag(der_decoder, 2)) {
            // DNS
            x509_subject_alternative_name_t* san = memory_malloc(sizeof(x509_subject_alternative_name_t));
            if (!san) {
                return -1;
            }
            san->type = X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS;
            size_t value_length = 0;
            if(der_decoder_decode_context_specific_string(der_decoder, 2, (uint8_t**)&san->value, &value_length) != 0) {
                memory_free(san);
                return -1;
            }
            san->next = ext->data.san_list;
            ext->data.san_list = san;
        } else if(der_decoder_has_context_specific_tag(der_decoder, 7)) {
            // IP
            x509_subject_alternative_name_t* san = memory_malloc(sizeof(x509_subject_alternative_name_t));
            if (!san) {
                return -1;
            }
            san->type = X509_SUBJECT_ALTERNATIVE_NAME_TYPE_IP;
            uint8_t* ip_bytes;
            size_t ip_length = 0;
            if(der_decoder_decode_context_specific_string(der_decoder, 7, &ip_bytes, &ip_length) != 0) {
                memory_free(san);
                return -1;
            }
            if (ip_length != 4) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid IP address length in SAN: %llu", ip_length);
                memory_free(san);
                return -1;
            }
            san->value = strprintf("%u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
            san->next  = ext->data.san_list;
            ext->data.san_list = san;
        } else if(der_decoder_has_context_specific_tag(der_decoder, 1)) {
            // Email
            x509_subject_alternative_name_t* san = memory_malloc(sizeof(x509_subject_alternative_name_t));
            if (!san) {
                return -1;
            }
            san->type = X509_SUBJECT_ALTERNATIVE_NAME_TYPE_EMAIL;
            size_t value_length = 0;
            if(der_decoder_decode_context_specific_string(der_decoder, 1, (uint8_t**)&san->value, &value_length) != 0) {
                memory_free(san);
                return -1;
            }
            san->next = ext->data.san_list;
            ext->data.san_list = san;
        } else {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported SAN tag");
            return -1;
        }
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_decode_extension_skid(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if (!der_decoder || !ext) {
        return -1;
    }

    if(der_decoder_decode_octet_string(der_decoder, &ext->data.skid.data, &ext->data.skid.length) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_decode_extension_akid(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if (!der_decoder || !ext) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        return -1;
    }

    if(der_decoder_decode_context_specific_string(der_decoder, 0, &ext->data.akid.data, &ext->data.akid.length) != 0) {
        return -1;
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_decode_extension_netscape_cert_type(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if (!der_decoder || !ext) {
        return -1;
    }

    size_t flags_length = 0;
    uint8_t* flags_data = NULL;

    if(der_decoder_decode_bit_string(der_decoder, &flags_data, &flags_length) != 0) {
        return -1;
    }

    if (flags_length != 1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid key usage length: %llu", flags_length);
        memory_free(flags_data);
        return -1;
    }

    uint8_t ku_byte = flags_data[0];
    memory_free(flags_data);

    uint32_t flags = 0;

    // Use the ENUM constants to check flags and construct the byte.
    // Since your enum matches ASN.1 bit positions (0x80, 0x40...),
    // we can OR them directly.

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_SSL_CLIENT) {
        flags |= X509_NETSCAPE_CERT_TYPE_SSL_CLIENT;
    }

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_SSL_SERVER) {
        flags |= X509_NETSCAPE_CERT_TYPE_SSL_SERVER;
    }

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_SMIME) {
        flags |= X509_NETSCAPE_CERT_TYPE_SMIME;
    }

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING) {
        flags |= X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING;
    }

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_RESERVED) {
        flags |= X509_NETSCAPE_CERT_TYPE_RESERVED;
    }

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_SSL_CA) {
        flags |= X509_NETSCAPE_CERT_TYPE_SSL_CA;
    }

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_SMIME_CA) {
        flags |= X509_NETSCAPE_CERT_TYPE_SMIME_CA;
    }

    if (ku_byte & X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING_CA) {
        flags |= X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING_CA;
    }

    ext->data.netscape_cert_type = flags;

    return 0;
}

static int8_t x509_decode_extension_value(der_decoder_t* der_decoder, x509_extension_t* ext) {
    if (!der_decoder || !ext) {
        return -1;
    }

    switch (ext->type) {
    case X509_EXTENSION_BASIC_CONSTRAINTS: return x509_decode_extension_basic_conntraints(der_decoder, ext);
    case X509_EXTENSION_KEY_USAGE: return x509_decode_extension_key_usage(der_decoder, ext);
    case X509_EXTENSION_EXTENDED_KEY_USAGE: return x509_decode_extension_extended_key_usage(der_decoder, ext);
    case X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME: return x509_decode_extension_subject_alternative_name(der_decoder, ext);
    case X509_EXTENSION_SKID: return x509_decode_extension_skid(der_decoder, ext);
    case X509_EXTENSION_AKID: return x509_decode_extension_akid(der_decoder, ext);
    case X509_EXTENSION_NETSCAPE_CERT_TYPE: return x509_decode_extension_netscape_cert_type(der_decoder, ext);
    default:
    }

    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported extension type: %d", ext->type);

    return -1;
}

static int8_t x509_decode_extensions(der_decoder_t* der_decoder, x509_certificate_t* cert) {
    if(!der_decoder || !cert) {
        return -1;
    }

    if(!der_decoder_has_explicit_tag(der_decoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 3)) {
        return 0; // No extensions present
    }

    if(der_decoder_start_explicit_tag(der_decoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 3) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start extensions explicit tag");
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start extensions sequence");
        return -1;
    }

    while (!der_decoder_has_container_ended(der_decoder)) {
        if(der_decoder_start_sequence(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start extension sequence");
            return -1;
        }

        der_object_identifier_t oid;

        if(der_decoder_decode_object_identifier(der_decoder, &oid) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode extension OID");
            return -1;
        }

        x509_extension_type_t ext_type;

        switch (oid) {
        case DER_OID_EXT_BASIC_CONSTRAINTS:
            ext_type = X509_EXTENSION_BASIC_CONSTRAINTS;
            break;
        case DER_OID_EXT_KEY_USAGE:
            ext_type = X509_EXTENSION_KEY_USAGE;
            break;
        case DER_OID_EXT_EXTENDED_KEY_USAGE:
            ext_type = X509_EXTENSION_EXTENDED_KEY_USAGE;
            break;
        case DER_OID_EXT_SAN:
            ext_type = X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME;
            break;
        case DER_OID_EXT_SKID:
            ext_type = X509_EXTENSION_SKID;
            break;
        case DER_OID_EXT_AKID:
            ext_type = X509_EXTENSION_AKID;
            break;
        case DER_OID_EXT_NETSCAPE_CERT_TYPE:
            ext_type = X509_EXTENSION_NETSCAPE_CERT_TYPE;
            break;
        default:
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported extension OID: %d", oid);
            return -1;
        }

        boolean_t is_critical = false;

        if(der_decoder_has_boolean_tag(der_decoder)) {
            if(der_decoder_decode_boolean(der_decoder, &is_critical) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode extension critical flag");
                return -1;
            }
        }

        if(der_decoder_start_octet_string(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start extension value octet string");
            return -1;
        }

        x509_extension_t* ext = &cert->extensions[ext_type];

        ext->is_valid = true;
        ext->type = ext_type;
        ext->is_critical = is_critical;

        if(x509_decode_extension_value(der_decoder, ext) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode extension value for type %d", ext_type);
            return -1;
        }

        if(der_decoder_end_octet_string(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end extension value octet string");
            return -1;
        }

        if(der_decoder_end_sequence(der_decoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end extension sequence");
            return -1;
        }
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end extensions sequence");
        return -1;
    }

    if(der_decoder_end_explicit_tag(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end extensions explicit tag");
        return -1;
    }

    return 0;
}

static int8_t x509_decode_validity(der_decoder_t* der_decoder, time_t* not_before, time_t* not_after) {
    if(!der_decoder || !not_before || !not_after) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        return -1;
    }

    if(der_decoder_decode_utc_time(der_decoder, not_before) != 0) {
        return -1;
    }

    if(der_decoder_decode_utc_time(der_decoder, not_after) != 0) {
        return -1;
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_decode_algorithm_identifier(der_decoder_t* der_decoder, x509_algorithm_t* algorithm) {
    if(!der_decoder || algorithm == 0) {
        return -1;
    }

    *algorithm = X509_ALGORITHM_UNKNOWN;

    if(der_decoder_start_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to start algorithm identifier sequence");
        return -1;
    }

    der_object_identifier_t oid;

    if(der_decoder_decode_object_identifier(der_decoder, &oid) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode algorithm OID");
        return -1;
    }

    switch (oid) {
    case DER_OID_ED25519: {
        *algorithm = X509_ALGORITHM_ED25519;
        break;
    }
    case DER_OID_X25519: {
        *algorithm = X509_ALGORITHM_X25519;
        break;
    }
    case DER_OID_ECDSA_WITH_SHA256: {
        *algorithm = X509_ALGORITHM_ECDSA_WITH_SHA256;
        break;
    }
    case DER_OID_ECDSA_PUBLIC_KEY: {
        if(der_decoder_decode_object_identifier(der_decoder, &oid) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode EC public key curve OID");
            return -1;
        }
        if(oid != DER_OID_EC_SECP256R1) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "unsupported EC curve OID: %d", oid);
            return -1;
        }
        *algorithm = X509_ALGORITHM_ECDSA_SECP256R1_SHA256;
        break;
    }
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported algorithm: %d", oid);
        return -1;
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to end algorithm identifier sequence");
        return -1;
    }

    return 0;
}

static int8_t x509_decode_data_with_bit_string_with_alogrithm_identifier(der_decoder_t*    der_decoder,
                                                                         x509_algorithm_t* algorithm,
                                                                         uint8_t**         data,
                                                                         size_t*           data_length) {
    if(!der_decoder || !algorithm || !data ) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to start data with bit string sequence");
        return -1;
    }

    if(x509_decode_algorithm_identifier(der_decoder, algorithm) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode algorithm identifier");
        return -1;
    }

    if(der_decoder_decode_bit_string(der_decoder, data, data_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode bit string data");
        return -1;
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to end data with bit string sequence");
        return -1;
    }

    return 0;
}

static int8_t x509_decode_tbs(der_decoder_t* der_decoder, x509_certificate_t* cert) {
    if (!der_decoder || !cert) {
        return -1;
    }

    if(der_decoder_start_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to start TBS sequence");
        return -1;
    }

    // 1. Version [0] EXPLICIT INTEGER (v3 = 2)
    if(der_decoder_start_explicit_tag(der_decoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 0) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to start version explicit tag");
        return -1;
    }

    int64_t version = 0;
    if(der_decoder_decode_integer(der_decoder, &version) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode version integer");
        return -1;
    }

    if(version != 2) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "unsupported certificate version: %lld", version);
        return -1;
    }

    cert->version = (uint8_t)version;

    if(der_decoder_end_explicit_tag(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to end version explicit tag");
        return -1;
    }

    // 2. Serial Number (Integer)
    if(der_decoder_decode_integer_u160(der_decoder, cert->serial_number) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode serial number");
        return -1;
    }

    // 3. Signature Algorithm Identifier
    if(x509_decode_algorithm_identifier(der_decoder, &cert->signature_algorithm) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode signature algorithm");
        return -1;
    }

    // 4. Issuer
    if(x509_decode_dn(der_decoder, cert->issuer) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode issuer");
        return -1;
    }

    // 5. Validity
    if(x509_decode_validity(der_decoder, &cert->not_before, &cert->not_after) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode validity");
        return -1;
    }

    // 6. Subject
    if(x509_decode_dn(der_decoder, cert->subject) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode subject");
        return -1;
    }

    // 7. SubjectPublicKeyInfo
    if(x509_decode_data_with_bit_string_with_alogrithm_identifier(der_decoder,
                                                                  &cert->public_key_algorithm,
                                                                  &cert->public_key,
                                                                  &cert->public_key_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode subject public key info");
        return -1;
    }

    // 8. Extensions [3] EXPLICIT SEQUENCE
    if(x509_decode_extensions(der_decoder, cert) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode extensions");
        return -1;
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to end TBS sequence");
        return -1;
    }

    return 0;
}

x509_certificate_t* x509_certificate_from_der(const uint8_t* der_data, size_t der_length) {
    if (der_data == NULL || der_length == 0) {
        return NULL;
    }

    der_decoder_t* der_decoder = NULL;

    der_decoder = der_decoder_new(der_data, der_length);
    if (der_decoder == NULL) {
        return NULL;
    }

    boolean_t success = false;

    x509_certificate_t* cert = x509_certificate_new();
    if (cert == NULL) {
        der_decoder_destroy(der_decoder);
        return NULL;
    }

    cert->version = 0;
    memory_memclean(cert->serial_number, sizeof(cert->serial_number));

    if(der_decoder_start_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to start DER sequence");
        goto cleanup;
    }

    // 1. parse tbsCertificate
    size_t tbs_start_pos = 0;
    size_t tbs_end_pos = 0;

    if(der_decoder_get_current_position(der_decoder, &tbs_start_pos) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to get TBS start position");
        goto cleanup;
    }

    if(x509_decode_tbs(der_decoder, cert) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode TBS certificate");
        goto cleanup;
    }

    if(der_decoder_get_current_position(der_decoder, &tbs_end_pos) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to get TBS end position");
        goto cleanup;
    }

    size_t tbs_length = tbs_end_pos - tbs_start_pos;
    cert->tbs_length = tbs_length;
    cert->tbs_data = memory_malloc(tbs_length);
    if (cert->tbs_data == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to allocate memory for TBS data");
        goto cleanup;
    }

    memory_memcopy(der_data + tbs_start_pos, cert->tbs_data, tbs_length);

    // 2. parse signatureAlgorithm
    if(x509_decode_algorithm_identifier(der_decoder, &cert->signature_algorithm) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode signature algorithm");
        goto cleanup;
    }

    // 3. parse signatureValue
    size_t signature_length = 0;
    uint8_t* signature = NULL;

    if(der_decoder_decode_bit_string(der_decoder, &signature, &signature_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to decode signature value");
        goto cleanup;
    }

    if(der_decoder_end_sequence(der_decoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to end DER sequence");
        goto cleanup;
    }

    cert->signature_length = signature_length;
    cert->signature = signature;

    success = true;

cleanup:
    if(!success) {
        x509_certificate_free(cert);
        cert = NULL;
    }

    der_decoder_destroy(der_decoder);

    return cert;
}

x509_certificate_t* x509_certificate_from_pem(const char_t* pem_data) {
    if (pem_data == NULL) {
        return NULL;
    }

    uint8_t* der_data = NULL;
    size_t der_length = 0;

    if(pem_decode("CERTIFICATE", pem_data, strlen(pem_data), &der_data, &der_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to PEM decode certificate");
        return NULL;
    }

    x509_certificate_t* cert = x509_certificate_from_der(der_data, der_length);
    memory_free(der_data);

    return cert;
}

uint8_t* x509_certificate_get_tbs_data(x509_certificate_t* cert, boolean_t rebuild, size_t* out_length) {
    if (cert == NULL || out_length == NULL) {
        return NULL;
    }

    if(rebuild || cert->tbs_data == NULL || cert->tbs_length == 0) {
        if(cert->tbs_data) {
            memory_free(cert->tbs_data);
            cert->tbs_data = NULL;
            cert->tbs_length = 0;
        }

        if (x509_encode_tbs(cert) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "failed to encode TBS data");
            return NULL;
        }
    }

    *out_length = cert->tbs_length;

    uint8_t* tbs_data = memory_malloc(cert->tbs_length);
    if (tbs_data == NULL) {
        return NULL;
    }

    memory_memcopy(cert->tbs_data, tbs_data, cert->tbs_length);

    return tbs_data;
}

uint8_t* x509_certificate_get_public_key_data(x509_certificate_t* cert, size_t* out_length) {
    if (cert == NULL || out_length == NULL) {
        return NULL;
    }

    *out_length = cert->public_key_length;

    uint8_t* public_key_data = memory_malloc(cert->public_key_length);
    if (public_key_data == NULL) {
        return NULL;
    }

    memory_memcopy(cert->public_key, public_key_data, cert->public_key_length);

    return public_key_data;
}

x509_algorithm_t x509_certificate_get_public_key_algorithm(x509_certificate_t* cert) {
    if (cert == NULL) {
        return X509_ALGORITHM_UNKNOWN;
    }

    return cert->public_key_algorithm;
}

int8_t x509_certificate_get_issuer_field(const x509_certificate_t* cert, x509_issuer_subject_field_t field, char_t** out_value) {
    if (cert == NULL || out_value == NULL || field >= X509_ISSUER_SUBJECT_FIELD_COUNT) {
        return -1;
    }

    *out_value = strdup(cert->issuer[field]);
    return 0;
}

int8_t x509_certificate_get_subject_field(const x509_certificate_t* cert, x509_issuer_subject_field_t field, char_t** out_value) {
    if (cert == NULL || out_value == NULL || field >= X509_ISSUER_SUBJECT_FIELD_COUNT) {
        return -1;
    }

    *out_value = strdup(cert->subject[field]);
    return 0;
}

static boolean_t x509_is_signature_algorithm_compatible_with_public_key_algorithm(x509_algorithm_t signature_algorithm, x509_algorithm_t public_key_algorithm) {
    switch (signature_algorithm) {
    case X509_ALGORITHM_ED25519:
        return public_key_algorithm == X509_ALGORITHM_ED25519;
    case X509_ALGORITHM_ECDSA_WITH_SHA256:
        return public_key_algorithm == X509_ALGORITHM_ECDSA_SECP256R1_SHA256;
    default:
        return false; // Unsupported or unknown signature algorithm
    }
}

boolean_t x509_certificate_is_authority_of(const x509_certificate_t* cert, const x509_certificate_t* potential_issuer) {
    if (cert == NULL || potential_issuer == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Certificate or potential issuer is null");
        return false;
    }

    // Check if issuer fields match subject fields
    for (size_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
        const char_t* issuer_value  = cert->issuer[i];
        const char_t* subject_value = potential_issuer->subject[i];

        if ((issuer_value == NULL && subject_value != NULL) ||
            (issuer_value != NULL && subject_value == NULL)) {
            return false; // One is null and the other is not
        }

        if (issuer_value != NULL && subject_value != NULL) {
            if (strcmp(issuer_value, subject_value) != 0) {
                return false; // Values do not match
            }
        }
    }

    // Check SKID/AKID if present
    const x509_extension_t* issuer_skid_ext = &potential_issuer->extensions[X509_EXTENSION_SKID];
    const x509_extension_t* cert_akid_ext = &cert->extensions[X509_EXTENSION_AKID];

    if (issuer_skid_ext->is_valid && cert_akid_ext->is_valid) {
        if (issuer_skid_ext->data.skid.length != cert_akid_ext->data.akid.length ||
            memory_memcompare(issuer_skid_ext->data.skid.data, cert_akid_ext->data.akid.data, issuer_skid_ext->data.skid.length) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "SKID/AKID mismatch: issuer SKID length %llu, cert AKID length %llu", issuer_skid_ext->data.skid.length, cert_akid_ext->data.akid.length);
            return false; // SKID/AKID do not match
        }
    }

    // check signature algorithm compatibility
    if (!x509_is_signature_algorithm_compatible_with_public_key_algorithm(cert->signature_algorithm, potential_issuer->public_key_algorithm)) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Signature algorithm '%d' does not match issuer's public key algorithm '%d'", cert->signature_algorithm, potential_issuer->public_key_algorithm);
        return false; // Signature algorithm does not match issuer's public key algorithm
    }

    // check signature verification
    if(x509_certificate_verify_signature_with_rebuild((x509_certificate_t*)cert,
                                                      potential_issuer->public_key_algorithm,
                                                      potential_issuer->public_key,
                                                      potential_issuer->public_key_length,
                                                      false) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Signature verification failed for potential issuer");
        return false; // Signature verification failed
    }

    return true; // All checks passed, potential_issuer is an authority of cert
}

int8_t x509_certificate_get_subject_der(const x509_certificate_t* cert, uint8_t** out_subject_der, size_t* out_subject_der_length) {
    if (cert == NULL || out_subject_der == NULL || out_subject_der_length == NULL) {
        return -1;
    }

    der_encoder_t* der_encoder = der_encoder_new();
    if (der_encoder == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create DER encoder");
        return -1;
    }

    if(x509_encode_dn(der_encoder, (char_t**)cert->subject) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode subject DN");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(der_encoder_get_der_data(der_encoder, out_subject_der, out_subject_der_length) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get DER data for subject");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    der_encoder_destroy(der_encoder);

    return 0;
}
