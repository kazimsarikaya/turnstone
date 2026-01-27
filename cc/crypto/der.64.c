/**
 * @file der.64.c
 * @brief DER (Distinguished Encoding Rules) Implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crypto/der.h>
#include <buffer.h>
#include <strings.h>

MODULE("turnstone.lib.crypto");

/* --- Constants --- */
static const uint8_t OID_CN[] = { 0x55, 0x04, 0x03 };
static const uint8_t OID_ED25519[] = { 0x2B, 0x65, 0x70 };
static const uint8_t OID_X25519[] = { 0x2B, 0x65, 0x6E };
static const uint8_t OID_SERVER_AUTH[] = { 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01 };
static const uint8_t OID_CLIENT_AUTH[] = { 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02 };
static const uint8_t OID_EXT_BASIC_CONSTRAINTS[] = { 0x55, 0x1D, 0x13 };
static const uint8_t OID_EXT_KEY_USAGE[] = { 0x55, 0x1D, 0x0F };
static const uint8_t OID_EXT_EXTENDED_KEY_USAGE[] = { 0x55, 0x1D, 0x25 };
static const uint8_t OID_EXT_SAN[] = { 0x55, 0x1D, 0x11 };
static const uint8_t OID_EXT_SKID[] = { 0x55, 0x1D, 0x0E };
static const uint8_t OID_EXT_AKID[] = { 0x55, 0x1D, 0x23 };

/* --- Structures --- */
typedef struct der_encoder_buffer_chain_t der_encoder_buffer_chain_t;

struct der_encoder_buffer_chain_t {
    buffer_t*                   current_buffer;
    der_encoder_buffer_chain_t* previous;
};

struct der_encoder_t {
    der_encoder_buffer_chain_t* buffer_chain;
};

/* --- Internal Helper Declarations --- */

/**
 * @brief Encodes the length field according to DER rules.
 */
static int8_t _der_encode_length(buffer_t* buffer, size_t length) {
    if (length < 0x80) {
        if(!buffer_append_byte(buffer, (uint8_t)length)) {
            return -1;
        }
    } else {
        size_t num_bytes = 0;
        size_t len = length;
        while (len > 0) {
            num_bytes++;
            len >>= 8;
        }
        if(!buffer_append_byte(buffer, 0x80 | (uint8_t)num_bytes)) {
            return -1;
        }
        for (int i = num_bytes - 1; i >= 0; i--) {
            if(!buffer_append_byte(buffer, (uint8_t)(length >> (i * 8)))) {
                return -1;
            }
        }
    }
    return 0;
}

/**
 * @brief Starts a new constructed type (Sequence, Set, etc).
 * Allocates a new chain link, new buffer, and writes the initial tag.
 */
static int8_t _der_start_chain(der_encoder_t* encoder, uint8_t tag_byte) {
    if (!encoder) {
        return -1;
    }

    der_encoder_buffer_chain_t* new_chain = memory_malloc(sizeof(der_encoder_buffer_chain_t));
    if (!new_chain) {
        return -1;
    }

    new_chain->current_buffer = buffer_new();
    if (!new_chain->current_buffer) {
        memory_free(new_chain);
        return -1;
    }

    if(!buffer_append_byte(new_chain->current_buffer, tag_byte)) {
        buffer_destroy(new_chain->current_buffer);
        memory_free(new_chain);
        return -1;
    }

    new_chain->previous = encoder->buffer_chain;
    encoder->buffer_chain = new_chain;
    return 0;
}

/**
 * @brief Ends a constructed type.
 * Pops the current chain, wraps content in Tag+Len, appends to parent.
 */
static int8_t _der_end_chain(der_encoder_t* encoder, uint8_t tag_byte) {
    if (!encoder || !encoder->buffer_chain) {
        return -1;
    }

    der_encoder_buffer_chain_t* current_chain = encoder->buffer_chain;
    encoder->buffer_chain = current_chain->previous;

    // The buffer currently contains [TagByte][Data...].
    // We need to calculate length of [Data...], and create [TagByte][Length][Data...]
    size_t content_length = buffer_get_length(current_chain->current_buffer) - 1;

    buffer_t* final_buffer = buffer_new();
    if (!final_buffer) {
        goto error_cleanup;
    }

    // 1. Append Tag
    if(!buffer_append_byte(final_buffer, tag_byte)) {
        goto error_cleanup;
    }

    // 2. Append Length
    if(_der_encode_length(final_buffer, content_length) != 0) {
        goto error_cleanup;
    }

    // 3. Append Content (Skipping the original tag byte at index 0)
    if(!buffer_append_bytes(final_buffer,
                            buffer_get_raw_bytes(current_chain->current_buffer) + 1,
                            content_length)) {
        goto error_cleanup;
    }

    buffer_destroy(current_chain->current_buffer);
    memory_free(current_chain);

    // Merge into parent or set as root
    if (encoder->buffer_chain) {
        if(!buffer_append_buffer(encoder->buffer_chain->current_buffer, final_buffer)) {
            buffer_destroy(final_buffer);
            return -1;
        }
        buffer_destroy(final_buffer);
    } else {
        // Create root chain
        encoder->buffer_chain = memory_malloc(sizeof(der_encoder_buffer_chain_t));
        if (!encoder->buffer_chain) {
            buffer_destroy(final_buffer);
            return -1;
        }
        encoder->buffer_chain->current_buffer = final_buffer;
        encoder->buffer_chain->previous = NULL;
    }

    return 0;

error_cleanup:
    if(final_buffer) {
        buffer_destroy(final_buffer);
    }
    buffer_destroy(current_chain->current_buffer);
    memory_free(current_chain);
    return -1;
}

/**
 * @brief Helper to write [Tag][Length][Data] to the current buffer.
 */
static int8_t _der_write_primitive(der_encoder_t* encoder, uint8_t tag_byte, const uint8_t* data, size_t len) {
    if (!encoder || !encoder->buffer_chain) {
        return -1;
    }

    buffer_t* buffer = encoder->buffer_chain->current_buffer;

    if(!buffer_append_byte(buffer, tag_byte)) {
        return -1;
    }
    if(_der_encode_length(buffer, len) != 0) {
        return -1;
    }
    if(data && len > 0) {
        if(!buffer_append_bytes(buffer, data, len)) {
            return -1;
        }
    }
    return 0;
}

/* --- Public API Implementation --- */

der_encoder_t* der_encoder_new(void) {
    der_encoder_t* encoder = memory_malloc(sizeof(der_encoder_t));
    if (!encoder) {
        return NULL;
    }
    encoder->buffer_chain = NULL;
    return encoder;
}

void der_encoder_destroy(der_encoder_t* encoder) {
    if (!encoder) {
        return;
    }
    der_encoder_buffer_chain_t* chain = encoder->buffer_chain;
    while (chain) {
        der_encoder_buffer_chain_t* prev = chain->previous;
        buffer_destroy(chain->current_buffer);
        memory_free(chain);
        chain = prev;
    }
    memory_free(encoder);
}

/* --- Constructed Types --- */

int8_t der_encoder_start_sequence(der_encoder_t * encoder) {
    return _der_start_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_CONSTRUCTED | DER_TAG_NUMBER_SEQUENCE);
}

int8_t der_encoder_end_sequence(der_encoder_t * encoder) {
    return _der_end_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_CONSTRUCTED | DER_TAG_NUMBER_SEQUENCE);
}

int8_t der_encoder_start_set(der_encoder_t * encoder) {
    return _der_start_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_CONSTRUCTED | DER_TAG_NUMBER_SET);
}

int8_t der_encoder_end_set(der_encoder_t * encoder) {
    return _der_end_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_CONSTRUCTED | DER_TAG_NUMBER_SET);
}

int8_t der_encoder_start_octet_string(der_encoder_t * encoder) {
    return _der_start_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OCTET_STRING);
}

int8_t der_encoder_end_octet_string(der_encoder_t * encoder) {
    return _der_end_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OCTET_STRING);
}

int8_t der_encoder_start_explicit_tag(der_encoder_t * encoder, der_tag_class_t inner_tag_class, uint8_t inner_tag_number) {
    uint8_t tag_byte = (uint8_t)(DER_TAG_TYPE_CONSTRUCTED | (inner_tag_class & 0xC0) | (inner_tag_number & 0x1F));
    return _der_start_chain(encoder, tag_byte);
}

int8_t der_encoder_end_explicit_tag(der_encoder_t * encoder) {
    if (!encoder || !encoder->buffer_chain) {
        return -1;
    }
    // For explicit tags, we retrieve the tag byte that was stored at the start of the buffer
    uint8_t tag_byte = buffer_get_raw_bytes(encoder->buffer_chain->current_buffer)[0];
    return _der_end_chain(encoder, tag_byte);
}

/* --- Primitive Types --- */

int8_t der_encoder_add_tag(der_encoder_t * encoder, der_tag_class_t tag_class, der_tag_type_t tag_type, uint8_t tag_number) {
    if (!encoder || !encoder->buffer_chain) {
        return -1;
    }
    uint8_t tag_byte = (uint8_t)(tag_class | tag_type | (tag_number & 0x1F));
    return buffer_append_byte(encoder->buffer_chain->current_buffer, tag_byte) ? 0 : -1;
}

int8_t der_encoder_encode_integer(der_encoder_t * encoder, int64_t value) {
    uint8_t int_bytes[9];
    size_t int_len = 0;

    if (value == 0) {
        int_bytes[0] = 0x00;
        int_len = 1;
    } else {
        bool is_negative = value < 0;
        uint64_t abs_value = is_negative ? -value : value;

        while (abs_value > 0) {
            int_bytes[int_len++] = (uint8_t)(abs_value & 0xFF);
            abs_value >>= 8;
        }

        if (!is_negative && (int_bytes[int_len - 1] & 0x80)) {
            int_bytes[int_len++] = 0x00;
        }

        // Reverse
        for (size_t i = 0; i < int_len / 2; i++) {
            uint8_t temp = int_bytes[i];
            int_bytes[i] = int_bytes[int_len - 1 - i];
            int_bytes[int_len - 1 - i] = temp;
        }
    }

    return _der_write_primitive(encoder,
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_INTEGER,
                                int_bytes, int_len);
}

int8_t der_encoder_encode_integer_u128(der_encoder_t * encoder, uint128_t value) {
    uint8_t int_bytes[16];
    size_t int_len = 0;

    if (value == 0) {
        int_bytes[0] = 0x00;
        int_len = 1;
    } else {
        while (value > 0) {
            int_bytes[int_len++] = (uint8_t)(value & 0xFF);
            value >>= 8;
        }

        if (int_bytes[int_len - 1] & 0x80) {
            int_bytes[int_len++] = 0x00;
        }

        for (size_t i = 0; i < int_len / 2; i++) {
            uint8_t temp = int_bytes[i];
            int_bytes[i] = int_bytes[int_len - 1 - i];
            int_bytes[int_len - 1 - i] = temp;
        }
    }

    return _der_write_primitive(encoder,
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_INTEGER,
                                int_bytes, int_len);
}

int8_t der_encoder_encode_octet_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len) {
    if (!data && data_len > 0) {
        return -1;
    }
    return _der_write_primitive(encoder,
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OCTET_STRING,
                                data, data_len);
}

int8_t der_encoder_encode_object_identifier(der_encoder_t * encoder, der_object_identifier_t oid) {
    const uint8_t* oid_data = NULL;
    size_t oid_len = 0;

    switch (oid) {
    case DER_OID_CN: oid_data = OID_CN; oid_len = sizeof(OID_CN); break;
    case DER_OID_ED25519: oid_data = OID_ED25519; oid_len = sizeof(OID_ED25519); break;
    case DER_OID_X25519: oid_data = OID_X25519; oid_len = sizeof(OID_X25519); break;
    case DER_OID_SERVER_AUTH: oid_data = OID_SERVER_AUTH; oid_len = sizeof(OID_SERVER_AUTH); break;
    case DER_OID_CLIENT_AUTH: oid_data = OID_CLIENT_AUTH; oid_len = sizeof(OID_CLIENT_AUTH); break;
    case DER_OID_EXT_BASIC_CONSTRAINTS: oid_data = OID_EXT_BASIC_CONSTRAINTS; oid_len = sizeof(OID_EXT_BASIC_CONSTRAINTS); break;
    case DER_OID_EXT_KEY_USAGE: oid_data = OID_EXT_KEY_USAGE; oid_len = sizeof(OID_EXT_KEY_USAGE); break;
    case DER_OID_EXT_EXTENDED_KEY_USAGE: oid_data = OID_EXT_EXTENDED_KEY_USAGE; oid_len = sizeof(OID_EXT_EXTENDED_KEY_USAGE); break;
    case DER_OID_EXT_SAN: oid_data = OID_EXT_SAN; oid_len = sizeof(OID_EXT_SAN); break;
    case DER_OID_EXT_SKID: oid_data = OID_EXT_SKID; oid_len = sizeof(OID_EXT_SKID); break;
    case DER_OID_EXT_AKID: oid_data = OID_EXT_AKID; oid_len = sizeof(OID_EXT_AKID); break;
    default: return -1;
    }

    return _der_write_primitive(encoder,
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OBJECT_ID,
                                oid_data, oid_len);
}

int8_t der_encoder_encode_bit_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len) {
    if (!encoder || !encoder->buffer_chain || !data) {
        return -1;
    }

    buffer_t* buffer = encoder->buffer_chain->current_buffer;

    // Manually handling Bit String as it has the extra "Unused Bits" byte
    if(!buffer_append_byte(buffer, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BIT_STRING)) {
        return -1;
    }
    if(_der_encode_length(buffer, data_len + 1) != 0) {
        return -1;
    }
    if(!buffer_append_byte(buffer, 0x00)) {
        return -1; // Unused bits
    }
    if(!buffer_append_bytes(buffer, data, data_len)) {
        return -1;
    }

    return 0;
}

int8_t der_encoder_encode_printable_string(der_encoder_t * encoder, const char_t * str, size_t str_len) {
    if (!str && str_len > 0) {
        return -1;
    }
    return _der_write_primitive(encoder,
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_PRINTABLE_STRING,
                                (const uint8_t*)str, str_len);
}

int8_t der_encoder_encode_boolean(der_encoder_t * encoder, boolean_t value) {
    uint8_t bool_byte = value ? 0xFF : 0x00;
    return _der_write_primitive(encoder,
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BOOLEAN,
                                &bool_byte, 1);
}

int8_t der_encoder_encode_context_specific_string(der_encoder_t * encoder, uint8_t tag_number, const uint8_t * data, size_t data_len) {
    uint8_t tag_byte = DER_TAG_CLASS_CONTEXT_SPECIFIC | DER_TAG_TYPE_PRIMITIVE | (tag_number & 0x1F);
    return _der_write_primitive(encoder, tag_byte, data, data_len);
}

int8_t der_encoder_encode_raw_bytes(der_encoder_t * encoder, const uint8_t * data, size_t data_len) {
    if (!encoder || !encoder->buffer_chain || (!data && data_len > 0)) {
        return -1;
    }
    return buffer_append_bytes(encoder->buffer_chain->current_buffer, data, data_len) ? 0 : -1;
}

int8_t der_encoder_encode_utc_time(der_encoder_t * encoder, time_t time_value) {
    char_t time_str[16] = {0};
    time_ns_format_utc(time_value, time_str, sizeof(time_str));

    return _der_write_primitive(encoder,
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_UTC_TIME,
                                (const uint8_t*)time_str, strlen(time_str));
}

int8_t der_encoder_get_der_data(der_encoder_t * encoder, uint8_t ** out_data, size_t * out_len) {
    if (!encoder || !out_data || !out_len) {
        return -1;
    }
    if (!encoder->buffer_chain || encoder->buffer_chain->previous) {
        return -1; // Incomplete

    }
    buffer_t* buffer = encoder->buffer_chain->current_buffer;
    *out_len = buffer_get_length(buffer);
    *out_data = buffer_get_all_bytes_and_destroy(buffer, NULL);

    memory_free(encoder->buffer_chain);
    encoder->buffer_chain = NULL;
    return 0;
}


struct der_decoder_t {
    const uint8_t* data;
    size_t         data_length;
    size_t         position;
};


der_decoder_t* der_decoder_new(const uint8_t* data, size_t data_length) {
    if (!data || data_length == 0) {
        return NULL;
    }

    der_decoder_t* decoder = memory_malloc(sizeof(der_decoder_t));
    if (!decoder) {
        return NULL;
    }

    decoder->data = data;
    decoder->data_length = data_length;
    decoder->position = 0;

    return decoder;
}

void der_decoder_destroy(der_decoder_t* decoder) {
    if (decoder) {
        memory_free(decoder);
    }
}

boolean_t der_decoder_is_at_end(der_decoder_t* decoder) {
    if (!decoder) {
        return true;
    }
    return decoder->position >= decoder->data_length ? true : false;
}

static int8_t _der_decoder_parse_length(der_decoder_t* decoder, size_t* out_length) {
    if (decoder->position >= decoder->data_length) {
        return -1;
    }

    uint8_t length_byte = decoder->data[decoder->position++];
    size_t length = 0;
    if (length_byte & 0x80) {
        size_t num_length_bytes = length_byte & 0x7F;
        if (num_length_bytes == 0 || num_length_bytes > sizeof(size_t) || decoder->position + num_length_bytes > decoder->data_length) {
            return -1;
        }
        for (size_t i = 0; i < num_length_bytes; i++) {
            length = (length << 8) | decoder->data[decoder->position++];
        }
    } else {
        length = length_byte;
    }

    *out_length = length;
    return 0;
}

int8_t der_decoder_start_sequence(der_decoder_t* decoder) {
    if (!decoder || decoder->position >= decoder->data_length) {
        return -1;
    }

    uint8_t tag = decoder->data[decoder->position++];
    if (tag != (DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_CONSTRUCTED | DER_TAG_NUMBER_SEQUENCE)) {
        return -1;
    }

    // parse length
    size_t length = 0;
    if (_der_decoder_parse_length(decoder, &length) != 0) {
        return -1;
    }

    if (decoder->position + length > decoder->data_length) {
        return -1;
    }

    return 0;
}

int8_t der_decoder_start_octet_string(der_decoder_t* decoder) {
    if (!decoder || decoder->position >= decoder->data_length) {
        return -1;
    }

    uint8_t tag = decoder->data[decoder->position++];
    if (tag != (DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OCTET_STRING)) {
        return -1;
    }

    // parse length
    size_t length = 0;
    if (_der_decoder_parse_length(decoder, &length) != 0) {
        return -1;
    }

    if (decoder->position + length > decoder->data_length) {
        return -1;
    }

    return 0;
}

int8_t der_decoder_read_integer(der_decoder_t* decoder, int64_t* out_value) {
    if (!decoder || decoder->position >= decoder->data_length || !out_value) {
        return -1;
    }

    uint8_t tag = decoder->data[decoder->position++];
    if (tag != (DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_INTEGER)) {
        return -1;
    }

    // parse length
    size_t length = 0;
    if (_der_decoder_parse_length(decoder, &length) != 0) {
        return -1;
    }

    if (decoder->position + length > decoder->data_length || length == 0 || length > 8) {
        return -1;
    }

    int64_t value = 0;
    for (size_t i = 0; i < length; i++) {
        value = (value << 8) | decoder->data[decoder->position++];
    }

    *out_value = value;
    return 0;
}

int8_t der_decoder_read_object_identifier(der_decoder_t* decoder, der_object_identifier_t* out_oid) {
    if (!decoder || decoder->position >= decoder->data_length || !out_oid) {
        return -1;
    }

    uint8_t tag = decoder->data[decoder->position++];
    if (tag != (DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OBJECT_ID)) {
        return -1;
    }

    // parse length
    size_t length = 0;
    if (_der_decoder_parse_length(decoder, &length) != 0) {
        return -1;
    }

    if (decoder->position + length > decoder->data_length) {
        return -1;
    }

    const uint8_t* oid_data = &decoder->data[decoder->position];
    decoder->position += length;

    // Match against known OIDs
    if (length == sizeof(OID_CN) && memory_memcompare(oid_data, OID_CN, sizeof(OID_CN)) == 0) {
        *out_oid = DER_OID_CN;
    } else if (length == sizeof(OID_ED25519) && memory_memcompare(oid_data, OID_ED25519, sizeof(OID_ED25519)) == 0) {
        *out_oid = DER_OID_ED25519;
    } else if (length == sizeof(OID_X25519) && memory_memcompare(oid_data, OID_X25519, sizeof(OID_X25519)) == 0) {
        *out_oid = DER_OID_X25519;
    } else if (length == sizeof(OID_SERVER_AUTH) && memory_memcompare(oid_data, OID_SERVER_AUTH, sizeof(OID_SERVER_AUTH)) == 0) {
        *out_oid = DER_OID_SERVER_AUTH;
    } else if (length == sizeof(OID_CLIENT_AUTH) && memory_memcompare(oid_data, OID_CLIENT_AUTH, sizeof(OID_CLIENT_AUTH)) == 0) {
        *out_oid = DER_OID_CLIENT_AUTH;
    } else if (length == sizeof(OID_EXT_BASIC_CONSTRAINTS) && memory_memcompare(oid_data, OID_EXT_BASIC_CONSTRAINTS, sizeof(OID_EXT_BASIC_CONSTRAINTS)) == 0) {
        *out_oid = DER_OID_EXT_BASIC_CONSTRAINTS;
    } else if (length == sizeof(OID_EXT_KEY_USAGE) && memory_memcompare(oid_data, OID_EXT_KEY_USAGE, sizeof(OID_EXT_KEY_USAGE)) == 0) {
        *out_oid = DER_OID_EXT_KEY_USAGE;
    } else if (length == sizeof(OID_EXT_EXTENDED_KEY_USAGE) && memory_memcompare(oid_data, OID_EXT_EXTENDED_KEY_USAGE, sizeof(OID_EXT_EXTENDED_KEY_USAGE)) == 0) {
        *out_oid = DER_OID_EXT_EXTENDED_KEY_USAGE;
    } else if (length == sizeof(OID_EXT_SAN) && memory_memcompare(oid_data, OID_EXT_SAN, sizeof(OID_EXT_SAN)) == 0) {
        *out_oid = DER_OID_EXT_SAN;
    } else if (length == sizeof(OID_EXT_SKID) && memory_memcompare(oid_data, OID_EXT_SKID, sizeof(OID_EXT_SKID)) == 0) {
        *out_oid = DER_OID_EXT_SKID;
    } else if (length == sizeof(OID_EXT_AKID) && memory_memcompare(oid_data, OID_EXT_AKID, sizeof(OID_EXT_AKID)) == 0) {
        *out_oid = DER_OID_EXT_AKID;
    } else {
        return -1; // Unknown OID
    }

    return 0;
}

int8_t der_decoder_read_octet_string(der_decoder_t* decoder, uint8_t** out_data, size_t* out_data_len) {
    if (!decoder || decoder->position >= decoder->data_length || !out_data || !out_data_len) {
        return -1;
    }

    uint8_t tag = decoder->data[decoder->position++];
    if (tag != (DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OCTET_STRING)) {
        return -1;
    }

    // parse length
    size_t length = 0;
    if (_der_decoder_parse_length(decoder, &length) != 0) {
        return -1;
    }

    if (decoder->position + length > decoder->data_length) {
        return -1;
    }

    uint8_t* data = memory_malloc(length);
    if (!data) {
        return -1;
    }

    memory_memcopy(&decoder->data[decoder->position], data, length);
    decoder->position += length;

    *out_data = data;
    *out_data_len = length;

    return 0;
}

int8_t der_decoder_read_bit_string(der_decoder_t* decoder, uint8_t** out_data, size_t* out_data_len) {
    if (!decoder || decoder->position >= decoder->data_length || !out_data || !out_data_len) {
        return -1;
    }

    uint8_t tag = decoder->data[decoder->position++];
    if (tag != (DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BIT_STRING)) {
        return -1;
    }

    // parse length
    size_t length = 0;
    if (_der_decoder_parse_length(decoder, &length) != 0) {
        return -1;
    }

    if (decoder->position + length > decoder->data_length || length == 0) {
        return -1;
    }

    // First byte is "Unused Bits"
    decoder->position++;
    length--;

    uint8_t* data = memory_malloc(length);
    if (!data) {
        return -1;
    }

    memory_memcopy(&decoder->data[decoder->position], data, length);
    decoder->position += length;

    *out_data = data;
    *out_data_len = length;

    return 0;
}
