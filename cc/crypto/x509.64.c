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
    x509_public_key_algorithm_t public_key_algorithm;
    size_t                      public_key_length;
    uint8_t*                    public_key;

    // Signature Info
    x509_signature_algorithm_t signature_algorithm;
    size_t                     signature_length;
    uint8_t*                   signature;

    // tbs data cache
    uint8_t* tbs_data;
    size_t   tbs_length;

    // assembled certificate cache
    uint8_t* certificate_data;
    size_t   certificate_length;
};

static const uint8_t OID_CN[] = { 0x06, 0x03, 0x55, 0x04, 0x03 };
static const uint8_t OID_ED25519[] = { 0x06, 0x03, 0x2B, 0x65, 0x70 };
static const uint8_t OID_X25519[] = { 0x06, 0x03, 0x2B, 0x65, 0x6E };
static const uint8_t OID_SERVER_AUTH[] = { 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01 };
static const uint8_t OID_CLIENT_AUTH[] = { 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02 };
static const uint8_t OID_EXT_BASIC_CONSTRAINTS[] = { 0x06, 0x03, 0x55, 0x1D, 0x13 };
static const uint8_t OID_EXT_KEY_USAGE[] = { 0x06, 0x03, 0x55, 0x1D, 0x0F };
static const uint8_t OID_EXT_EXTENDED_KEY_USAGE[] = { 0x06, 0x03, 0x55, 0x1D, 0x25 };
static const uint8_t OID_EXT_SAN[] = { 0x06, 0x03, 0x55, 0x1D, 0x11 };
static const uint8_t OID_EXT_SKID[] = { 0x06, 0x03, 0x55, 0x1D, 0x0E };
static const uint8_t OID_EXT_AKID[] = { 0x06, 0x03, 0x55, 0x1D, 0x23 };

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

int8_t x509_certificate_add_public_key(x509_certificate_t*         cert,
                                       x509_public_key_algorithm_t algorithm,
                                       const uint8_t*              public_key,
                                       size_t                      public_key_length) {
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

static size_t der_encode_length(buffer_t* buffer, size_t length) {
    if (length < 0x80) {
        buffer_append_byte(buffer, (uint8_t)length);
        return 1;
    }

    size_t num_bytes = 0;
    size_t len = length;
    while (len > 0) {
        len >>= 8;
        num_bytes++;
    }
    buffer_append_byte(buffer, (uint8_t)(0x80 | num_bytes));
    for (size_t i = num_bytes; i > 0; i--) {
        buffer_append_byte(buffer, (uint8_t)((length >> ((i - 1) * 8)) & 0xFF));
    }
    return 1 + num_bytes;
}

static void der_encode_integer_u128(buffer_t* buffer, uint128_t value) {
    uint8_t bytes[16];
    // Convert u128 to big-endian bytes...
    for (int32_t i = 15; i >= 0; i--) {
        bytes[i] = (uint8_t)(value & 0xFF);
        value >>= 8;
    }

    // Find first non-zero byte
    int start = 0;
    while (start < 15 && bytes[start] == 0) {start++;}

    size_t len = 16 - start;
    buffer_append_byte(buffer, 0x02); // Tag: INTEGER

    // If MSB is 1, prepend 0x00
    if (bytes[start] & 0x80) {
        der_encode_length(buffer, len + 1);
        buffer_append_byte(buffer, 0x00);
    } else {
        der_encode_length(buffer, len);
    }

    buffer_append_bytes(buffer, &bytes[start], len);
}

static buffer_t* x509_encode_name(const char_t* common_name) {
    // 1. Create the innermost content: The AttributeTypeAndValue (Sequence)
    buffer_t* attr_type_val_content = buffer_new();
    buffer_append_bytes(attr_type_val_content, OID_CN, sizeof(OID_CN));

    buffer_append_byte(attr_type_val_content, 0x13); // PrintableString
    der_encode_length(attr_type_val_content, strlen(common_name));
    buffer_append_bytes(attr_type_val_content, (uint8_t*)common_name, strlen(common_name));

    // 2. Wrap it in a SEQUENCE, then into the RDN (Set)
    buffer_t* rdn_set_content = buffer_new();
    buffer_append_byte(rdn_set_content, 0x30); // SEQUENCE Tag
    der_encode_length(rdn_set_content, buffer_get_length(attr_type_val_content));
    buffer_append_buffer(rdn_set_content, attr_type_val_content);
    buffer_destroy(attr_type_val_content);

    // 3. Wrap the RDN into the Name (Sequence)
    buffer_t* name_content = buffer_new();
    buffer_append_byte(name_content, 0x31); // SET Tag
    der_encode_length(name_content, buffer_get_length(rdn_set_content));
    buffer_append_buffer(name_content, rdn_set_content);
    buffer_destroy(rdn_set_content);

    // 4. Final Name Wrapper
    buffer_t* name_final = buffer_new();
    buffer_append_byte(name_final, 0x30); // SEQUENCE Tag
    der_encode_length(name_final, buffer_get_length(name_content));
    buffer_append_buffer(name_final, name_content);
    buffer_destroy(name_content);

    return name_final;
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

static buffer_t* x509_encode_extension_value(x509_extension_t* ext) {
    buffer_t* value = buffer_new();

    switch (ext->type) {
    case X509_EXTENSION_BASIC_CONSTRAINTS: {
        buffer_append_byte(value, 0x30); // SEQUENCE
        buffer_t* seq = buffer_new();

        // isCA BOOLEAN
        buffer_append_byte(seq, 0x01);
        der_encode_length(seq, 1);
        buffer_append_byte(seq, ext->data.basic_constraints.is_ca ? 0xFF : 0x00);

        // pathLenConstraint INTEGER (if applicable)
        if (ext->data.basic_constraints.path_len >= 0) {
            der_encode_integer_u128(seq, (uint128_t)ext->data.basic_constraints.path_len);
        }

        der_encode_length(value, buffer_get_length(seq));
        buffer_append_buffer(value, seq);
        buffer_destroy(seq);
        break;
    }
    case X509_EXTENSION_KEY_USAGE: {
        buffer_append_byte(value, 0x03); // BIT STRING
        buffer_t* bitstr = buffer_new();

        // Unused bits
        buffer_append_byte(bitstr, 0x00);

        // Key usage bits
        buffer_append_byte(bitstr, (uint8_t)ext->data.key_usage);

        der_encode_length(value, buffer_get_length(bitstr));
        buffer_append_buffer(value, bitstr);
        buffer_destroy(bitstr);
        break;
    }
    case X509_EXTENSION_EXTENDED_KEY_USAGE: {
        buffer_append_byte(value, 0x30); // SEQUENCE
        buffer_t* seq = buffer_new();

        // OID for EKU based on type
        const uint8_t* eku_oid = NULL;
        size_t eku_oid_len = 0;
        switch (ext->data.eku) {
        case X509_EXTENDED_KEY_USAGE_SERVER_AUTH: {
            eku_oid = OID_SERVER_AUTH;
            eku_oid_len = sizeof(OID_SERVER_AUTH);
            break;
        }
        case X509_EXTENDED_KEY_USAGE_CLIENT_AUTH: {
            eku_oid = OID_CLIENT_AUTH;
            eku_oid_len = sizeof(OID_CLIENT_AUTH);
            break;
        }
        default:
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported EKU type");
            buffer_destroy(value);
            return NULL;
        }
        buffer_append_bytes(seq, (uint8_t*)eku_oid, eku_oid_len);

        der_encode_length(value, buffer_get_length(seq));
        buffer_append_buffer(value, seq);
        buffer_destroy(seq);
        break;
    }
    case X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME: {
        buffer_append_byte(value, 0x30); // SEQUENCE
        buffer_t* seq = buffer_new();

        x509_subject_alternative_name_t* san = ext->data.san_list;
        while (san) {
            switch (san->type) {
            case X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS: {
                buffer_append_byte(seq, 0x82); // [2] DNSName
                der_encode_length(seq, strlen(san->value));
                buffer_append_bytes(seq, (uint8_t*)san->value, strlen(san->value));
                break;
            }
            case X509_SUBJECT_ALTERNATIVE_NAME_TYPE_IP: {
                buffer_append_byte(seq, 0x87); // [7] iPAddress
                // Assuming IPv4 for simplicity
                uint8_t ip_bytes[4];
                if (x509_parse_ipv4address(san->value, ip_bytes) != 0) {
                    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid IP address format");
                    buffer_destroy(value);
                    buffer_destroy(seq);
                    return NULL;
                }
                der_encode_length(seq, 4);
                buffer_append_bytes(seq, ip_bytes, 4);
                break;
            }
            case X509_SUBJECT_ALTERNATIVE_NAME_TYPE_EMAIL: {
                buffer_append_byte(seq, 0x81); // [1] rfc822Name
                der_encode_length(seq, strlen(san->value));
                buffer_append_bytes(seq, (uint8_t*)san->value, strlen(san->value));
                break;
            }
            default:
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported SAN type");
                buffer_destroy(value);
                buffer_destroy(seq);
                return NULL;
            }
            san = san->next;
        }

        der_encode_length(value, buffer_get_length(seq));
        buffer_append_buffer(value, seq);
        buffer_destroy(seq);
        break;
    }
    case X509_EXTENSION_SKID: {
        buffer_append_byte(value, 0x04); // OCTET STRING
        buffer_t* octet_str = buffer_new();

        buffer_append_bytes(octet_str, ext->data.skid.data, ext->data.skid.length);

        der_encode_length(value, buffer_get_length(octet_str));
        buffer_append_buffer(value, octet_str);
        buffer_destroy(octet_str);
        break;
    }
    case X509_EXTENSION_AKID: {
        buffer_append_byte(value, 0x30); // SEQUENCE
        buffer_t* seq = buffer_new();
        buffer_append_byte(seq, 0x80); // [0] keyIdentifier
        buffer_t* octet_str = buffer_new();
        buffer_append_bytes(octet_str, ext->data.akid.data, ext->data.akid.length);
        der_encode_length(seq, buffer_get_length(octet_str));
        buffer_append_buffer(seq, octet_str);
        buffer_destroy(octet_str);
        der_encode_length(value, buffer_get_length(seq));
        buffer_append_buffer(value, seq);
        buffer_destroy(seq);
        break;
    }
    default:
        // Unsupported extension type
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported extension type: %d", ext->type);
        buffer_destroy(value);
        return NULL;
    }

    return value;
}

static int8_t x509_encode_tbs(x509_certificate_t* cert) {
    if (!cert || !cert->issuer_common_name || !cert->subject_common_name ||
        !cert->public_key || cert->not_before == 0 || cert->not_after == 0) {
        return -1;
    }

    buffer_t* body = buffer_new();

    if(!body) {
        return -1;
    }

    // 1. Version [0] EXPLICIT INTEGER (v3 = 2)
    // Hex: A0 03 02 01 02
    uint8_t version_data[] = { 0xA0, 0x03, 0x02, 0x01, 0x02 };
    if(!buffer_append_bytes(body, version_data, 5)) {
        buffer_destroy(body);
        return -1;
    }

    // 2. Serial Number (Integer)
    der_encode_integer_u128(body, cert->serial_number);

    // 3. Signature Algorithm Identifier (Ed25519)
    // SEQUENCE { OID 1.3.101.112 }
    if(!buffer_append_byte(body, 0x30)) {
        buffer_destroy(body);
        return -1;
    }

    switch (cert->signature_algorithm) {
    case X509_SIGNATURE_ALGORITHM_ED25519: {
        der_encode_length(body, sizeof(OID_ED25519));
        if(buffer_append_bytes(body, OID_ED25519, sizeof(OID_ED25519)) == false) {
            buffer_destroy(body);
            return -1;
        }
        break;
    }
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported signature algorithm: %d", cert->signature_algorithm);
        buffer_destroy(body);
        return -1;
    }

    // 4 & 6. Issuer and Subject (Helper for DN)
    // Encodes: SEQUENCE { SET { SEQUENCE { OID(CN), PrintableString(val) } } }
    buffer_t* issuer_dn = x509_encode_name(cert->issuer_common_name);
    if(!issuer_dn) {
        buffer_destroy(body);
        return -1;
    }

    if(!buffer_append_buffer(body, issuer_dn)) {
        buffer_destroy(issuer_dn);
        buffer_destroy(body);
        return -1;
    }

    buffer_destroy(issuer_dn);

    // 5. Validity
    // SEQUENCE { UTCTime, UTCTime }
    buffer_t* validity = buffer_new();
    if(!validity) {
        buffer_destroy(body);
        return -1;
    }

    char_t time_str[14]; // YYMMDDHHMMSSZ + null

    if(!buffer_append_byte(validity, 0x17)) { // UTCTime
        buffer_destroy(validity);
        buffer_destroy(body);
        return -1;
    }

    time_ns_format_utc(cert->not_before, time_str, sizeof(time_str));
    der_encode_length(validity, 13);
    if(!buffer_append_bytes(validity, (uint8_t*)time_str, 13)) {
        buffer_destroy(validity);
        buffer_destroy(body);
        return -1;
    }

    if(!buffer_append_byte(validity, 0x17)) { // UTCTime
        buffer_destroy(validity);
        buffer_destroy(body);
        return -1;
    }

    time_ns_format_utc(cert->not_after, time_str, sizeof(time_str));
    der_encode_length(validity, 13);
    if(!buffer_append_bytes(validity, (uint8_t*)time_str, 13)) {
        buffer_destroy(validity);
        buffer_destroy(body);
        return -1;
    }

    if(!buffer_append_byte(body, 0x30)) { // Wrap Validity
        buffer_destroy(validity);
        buffer_destroy(body);
        return -1;
    }

    der_encode_length(body, buffer_get_length(validity));
    if(!buffer_append_buffer(body, validity)) {
        buffer_destroy(validity);
        buffer_destroy(body);
        return -1;
    }

    buffer_destroy(validity);

    // Subject DN
    buffer_t* subject_dn = x509_encode_name(cert->subject_common_name);
    if(!subject_dn) {
        buffer_destroy(body);
        return -1;
    }

    if(!buffer_append_buffer(body, subject_dn)) {
        buffer_destroy(subject_dn);
        buffer_destroy(body);
        return -1;
    }

    buffer_destroy(subject_dn);

    // 7. SubjectPublicKeyInfo (Crucial structure for X25519)
    // SEQUENCE { SEQUENCE { OID X25519 }, BIT STRING { KeyBytes } }
    buffer_t* spki = buffer_new();
    if(!spki) {
        buffer_destroy(body);
        return -1;
    }

    // Algorithm Identifier Sequence
    if(!buffer_append_byte(spki, 0x30)) {
        buffer_destroy(spki);
        buffer_destroy(body);
        return -1;
    }

    switch (cert->public_key_algorithm) {
    case X509_PUBLIC_KEY_ALGORITHM_ED25519: {
        der_encode_length(spki, sizeof(OID_ED25519));
        if(buffer_append_bytes(spki, OID_ED25519, sizeof(OID_ED25519)) == false) {
            buffer_destroy(spki);
            buffer_destroy(body);
            return -1;
        }
        break;
    }
    case X509_PUBLIC_KEY_ALGORITHM_X25519: {
        der_encode_length(spki, sizeof(OID_X25519));
        if(buffer_append_bytes(spki, OID_X25519, sizeof(OID_X25519)) == false) {
            buffer_destroy(spki);
            buffer_destroy(body);
            return -1;
        }
        break;
    }
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported public key algorithm: %d", cert->public_key_algorithm);
        buffer_destroy(spki);
        buffer_destroy(body);
        return -1;
    }

    // Public Key BitString
    if(!buffer_append_byte(spki, 0x03)) {
        buffer_destroy(spki);
        buffer_destroy(body);
        return -1;
    }

    der_encode_length(spki, cert->public_key_length + 1);
    if(!buffer_append_byte(spki, 0x00)) { // 0 unused bits
        buffer_destroy(spki);
        buffer_destroy(body);
        return -1;
    }

    if(!buffer_append_bytes(spki, cert->public_key, cert->public_key_length)) {
        buffer_destroy(spki);
        buffer_destroy(body);
        return -1;
    }

    if(!buffer_append_byte(body, 0x30)) { // Wrap SPKI
        buffer_destroy(spki);
        buffer_destroy(body);
        return -1;
    }

    der_encode_length(body, buffer_get_length(spki));
    if(!buffer_append_buffer(body, spki)) {
        buffer_destroy(spki);
        buffer_destroy(body);
        return -1;
    }

    buffer_destroy(spki);

    // 8. Extensions [3] EXPLICIT SEQUENCE
    if (cert->extensions) {
        buffer_t* ext_list = buffer_new();
        if(!ext_list) {
            buffer_destroy(body);
            return -1;
        }

        x509_extension_t* curr = cert->extensions;

        while (curr) {
            buffer_t* ext_seq = buffer_new();

            if(!ext_seq) {
                buffer_destroy(ext_list);
                buffer_destroy(body);
                return -1;
            }

            // OID
            if (curr->type == X509_EXTENSION_BASIC_CONSTRAINTS) {
                if(!buffer_append_bytes(ext_seq, OID_EXT_BASIC_CONSTRAINTS, sizeof(OID_EXT_BASIC_CONSTRAINTS))) {
                    buffer_destroy(ext_seq);
                    buffer_destroy(ext_list);
                    buffer_destroy(body);
                    return -1;
                }
            } else if (curr->type == X509_EXTENSION_KEY_USAGE) {
                if(!buffer_append_bytes(ext_seq, OID_EXT_KEY_USAGE, sizeof(OID_EXT_KEY_USAGE))) {
                    buffer_destroy(ext_seq);
                    buffer_destroy(ext_list);
                    buffer_destroy(body);
                    return -1;
                }
            } else if (curr->type == X509_EXTENSION_EXTENDED_KEY_USAGE) {
                if(buffer_append_bytes(ext_seq, OID_EXT_EXTENDED_KEY_USAGE, sizeof(OID_EXT_EXTENDED_KEY_USAGE)) == false) {
                    buffer_destroy(ext_seq);
                    buffer_destroy(ext_list);
                    buffer_destroy(body);
                    return -1;
                }
            } else if (curr->type == X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME) {
                if(!buffer_append_bytes(ext_seq, OID_EXT_SAN, sizeof(OID_EXT_SAN))) {
                    buffer_destroy(ext_seq);
                    buffer_destroy(ext_list);
                    buffer_destroy(body);
                    return -1;
                }
            } else if( curr->type == X509_EXTENSION_SKID) {
                if(!buffer_append_bytes(ext_seq, OID_EXT_SKID, sizeof(OID_EXT_SKID))) {
                    buffer_destroy(ext_seq);
                    buffer_destroy(ext_list);
                    buffer_destroy(body);
                    return -1;
                }
            } else if( curr->type == X509_EXTENSION_AKID) {
                if(!buffer_append_bytes(ext_seq, OID_EXT_AKID, sizeof(OID_EXT_AKID))) {
                    buffer_destroy(ext_seq);
                    buffer_destroy(ext_list);
                    buffer_destroy(body);
                    return -1;
                }
            } else {
                // Unknown extension type
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unknown extension type: %d", curr->type);
                buffer_destroy(ext_seq);
                buffer_destroy(ext_list);
                buffer_destroy(body);
                return -1;
            }

            // Criticality (Only if true)
            if (curr->is_critical) {
                uint8_t crit[] = { 0x01, 0x01, 0xFF };
                buffer_append_bytes(ext_seq, crit, 3);
            }

            // Extension Value (OCTET STRING containing the DER encoded extension)
            buffer_t* ext_val_raw = x509_encode_extension_value(curr);

            if (ext_val_raw == NULL) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode extension value");
                buffer_destroy(ext_seq);
                buffer_destroy(ext_list);
                buffer_destroy(body);
                return -1;
            }

            if(!buffer_append_byte(ext_seq, 0x04)) { // OCTET STRING
                buffer_destroy(ext_val_raw);
                buffer_destroy(ext_seq);
                buffer_destroy(ext_list);
                buffer_destroy(body);
                return -1;
            }

            der_encode_length(ext_seq, buffer_get_length(ext_val_raw));
            if(!buffer_append_buffer(ext_seq, ext_val_raw)) {
                buffer_destroy(ext_val_raw);
                buffer_destroy(ext_seq);
                buffer_destroy(ext_list);
                buffer_destroy(body);
                return -1;
            }

            buffer_destroy(ext_val_raw);

            // Wrap single extension in SEQUENCE
            if(!buffer_append_byte(ext_list, 0x30)) {
                buffer_destroy(ext_seq);
                buffer_destroy(ext_list);
                buffer_destroy(body);
                return -1;
            }

            der_encode_length(ext_list, buffer_get_length(ext_seq));
            if(buffer_append_buffer(ext_list, ext_seq) == false) {
                buffer_destroy(ext_seq);
                buffer_destroy(ext_list);
                buffer_destroy(body);
                return -1;
            }

            buffer_destroy(ext_seq);

            curr = curr->next;
        }

        // Wrap [3] { SEQUENCE { ...ext_list... } }
        buffer_t* ext_wrapper = buffer_new();
        if(!ext_wrapper) {
            buffer_destroy(ext_list);
            buffer_destroy(body);
            return -1;
        }

        if(!buffer_append_byte(ext_wrapper, 0x30)) {
            buffer_destroy(ext_wrapper);
            buffer_destroy(ext_list);
            buffer_destroy(body);
            return -1;
        }

        der_encode_length(ext_wrapper, buffer_get_length(ext_list));
        if(!buffer_append_buffer(ext_wrapper, ext_list)) {
            buffer_destroy(ext_wrapper);
            buffer_destroy(ext_list);
            buffer_destroy(body);
            return -1;
        }

        buffer_destroy(ext_list);

        if(!buffer_append_byte(body, 0xA3)) { // [3] EXPLICIT
            buffer_destroy(ext_wrapper);
            buffer_destroy(body);
            return -1;
        }

        der_encode_length(body, buffer_get_length(ext_wrapper));
        if(!buffer_append_buffer(body, ext_wrapper)) {
            buffer_destroy(ext_wrapper);
            buffer_destroy(body);
            return -1;
        }

        buffer_destroy(ext_wrapper);
    }

    // Final Outer TBS SEQUENCE
    buffer_t* tbs = buffer_new();
    if(!tbs) {
        buffer_destroy(body);
        return -1;
    }

    if(!buffer_append_byte(tbs, 0x30)) {
        buffer_destroy(tbs);
        buffer_destroy(body);
        return -1;
    }

    der_encode_length(tbs, buffer_get_length(body));
    if(!buffer_append_buffer(tbs, body)) {
        buffer_destroy(tbs);
        buffer_destroy(body);
        return -1;
    }

    buffer_destroy(body);

    cert->tbs_data = buffer_get_all_bytes_and_destroy(tbs, &cert->tbs_length);

    if(!cert->tbs_data) {
        return -1;
    }

    if(cert->tbs_length == 0) {
        return -1;
    }

    return 0;
}

static int8_t x509_certificate_sign_with_ed25519(x509_certificate_t* cert,
                                                 const uint8_t*      private_key,
                                                 size_t              private_key_length) {
    if (cert == NULL || private_key == NULL || private_key_length == 0) {
        return -1;
    }

    cert->signature_algorithm = X509_SIGNATURE_ALGORITHM_ED25519;

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

int8_t x509_certificate_sign(x509_certificate_t*        cert,
                             x509_signature_algorithm_t algorithm,
                             const uint8_t*             private_key,
                             size_t                     private_key_length) {
    if (cert == NULL || private_key == NULL || private_key_length == 0) {
        return -1;
    }

    switch (algorithm) {
    case X509_SIGNATURE_ALGORITHM_ED25519:
        return x509_certificate_sign_with_ed25519(cert, private_key, private_key_length);
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "unsupported signature algorithm: %d", algorithm);
        return -1;
    }

    return 0;
}

int8_t x509_certificate_assemble(x509_certificate_t* cert) {
    if (cert == NULL || cert->tbs_data == NULL || cert->tbs_length == 0 || cert->signature == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "invalid certificate state for assembly");
        return -1;
    }

    buffer_t* cert_seq = buffer_new();

    if (cert_seq == NULL) {
        return -1;
    }

    // 1. TBSCertificate
    if(!buffer_append_bytes(cert_seq, cert->tbs_data, cert->tbs_length)) {
        buffer_destroy(cert_seq);
        return -1;
    }

    // 2. AlgorithmIdentifier (Ed25519)
    // SEQUENCE { OID 1.3.101.112 }
    buffer_t* alg_id = buffer_new();
    if(!alg_id) {
        buffer_destroy(cert_seq);
        return -1;
    }

    switch (cert->signature_algorithm) {
    case X509_SIGNATURE_ALGORITHM_ED25519: {
        if(!buffer_append_bytes(alg_id, OID_ED25519, sizeof(OID_ED25519))) {
            buffer_destroy(alg_id);
            buffer_destroy(cert_seq);
            return -1;
        }
    }
    break;
    default:
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "unsupported signature algorithm in assembly: %d", cert->signature_algorithm);
        buffer_destroy(alg_id);
        buffer_destroy(cert_seq);
        return -1;
    }


    if(!buffer_append_byte(cert_seq, 0x30)) {
        buffer_destroy(alg_id);
        buffer_destroy(cert_seq);
        return -1;
    }

    der_encode_length(cert_seq, buffer_get_length(alg_id));
    if(!buffer_append_buffer(cert_seq, alg_id)) {
        buffer_destroy(alg_id);
        buffer_destroy(cert_seq);
        return -1;
    }

    buffer_destroy(alg_id);

    // 3. Signature Value (BIT STRING)
    // 64 bytes + 1 byte for "0 unused bits"
    if(!buffer_append_byte(cert_seq, 0x03)) {
        buffer_destroy(cert_seq);
        return -1;
    }

    der_encode_length(cert_seq, 65);
    if(!buffer_append_byte(cert_seq, 0x00)) { // 0 unused bits
        buffer_destroy(cert_seq);
        return -1;
    }

    if(!buffer_append_bytes(cert_seq, cert->signature, 64)) {
        buffer_destroy(cert_seq);
        return -1;
    }

    // Final Wrap
    buffer_t* final_output = buffer_new();

    if(!final_output) {
        buffer_destroy(cert_seq);
        return -1;
    }

    if(!buffer_append_byte(final_output, 0x30)) {
        buffer_destroy(cert_seq);
        buffer_destroy(final_output);
        return -1;
    }

    der_encode_length(final_output, buffer_get_length(cert_seq));
    if(!buffer_append_buffer(final_output, cert_seq)) {
        buffer_destroy(cert_seq);
        buffer_destroy(final_output);
        return -1;
    }

    buffer_destroy(cert_seq);

    cert->certificate_data = buffer_get_all_bytes_and_destroy(final_output, &cert->certificate_length);

    if(!cert->certificate_data) {
        return -1;
    }

    if(cert->certificate_length == 0) {
        return -1;
    }

    return 0;
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

static char_t* pem_encode(const char_t* header, const uint8_t* der_data, size_t der_length, size_t* out_pem_length) {
    if (header == NULL || der_data == NULL || der_length == 0 || out_pem_length == NULL) {
        return NULL;
    }

    buffer_t* pem_buffer = buffer_new();
    if (pem_buffer == NULL) {
        return NULL;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"-----BEGIN ", strlen("-----BEGIN "))) {
        buffer_destroy(pem_buffer);
        return NULL;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)header, strlen(header))) {
        buffer_destroy(pem_buffer);
        return NULL;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"-----\n", strlen("-----\n"))) {
        buffer_destroy(pem_buffer);
        return NULL;
    }

    uint8_t* b64_encoded = NULL;
    size_t b64_length = base64_encode(der_data, der_length, true, &b64_encoded);
    if (b64_length == 0 || b64_encoded == NULL) {
        buffer_destroy(pem_buffer);
        return NULL;
    }

    if(!buffer_append_bytes(pem_buffer, b64_encoded, b64_length)) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return NULL;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"\n-----END ", strlen("\n-----END "))) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return NULL;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)header, strlen(header))) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return NULL;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"-----\n", strlen("-----\n"))) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return NULL;
    }

    if(!buffer_append_byte(pem_buffer, '\0')) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return NULL;
    }

    uint8_t* pem_data_bytes = buffer_get_all_bytes_and_destroy(pem_buffer, out_pem_length);
    memory_free(b64_encoded);

    if (pem_data_bytes == NULL || *out_pem_length == 0) {
        return NULL;
    }


    return (char_t*)pem_data_bytes;
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
    char_t* pem_data = pem_encode("CERTIFICATE", cert->certificate_data, cert->certificate_length, &pem_length);
    if (pem_data == NULL) {
        return NULL;
    }

    return pem_data;
}
