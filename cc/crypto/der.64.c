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
#include <logging.h>

MODULE("turnstone.lib.crypto");

/* --- Constants --- */
static const uint8_t OID_CN[] = { 0x55, 0x04, 0x03 };
static const uint8_t OID_ORGANIZATION[] = { 0x55, 0x04, 0x0A };
static const uint8_t OID_ORGANIZATIONAL_UNIT[] = { 0x55, 0x04, 0x0B };
static const uint8_t OID_COUNTRY[] = { 0x55, 0x04, 0x06 };
static const uint8_t OID_ED25519[] = { 0x2B, 0x65, 0x70 };
static const uint8_t OID_X25519[] = { 0x2B, 0x65, 0x6E };
static const uint8_t OID_ECDSA_WITH_SHA256[] = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02 };
static const uint8_t OID_ECDSA_PUBLIC_KEY[] = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01 };
static const uint8_t OID_EC_SECP256R1[] = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07 };
static const uint8_t OID_SERVER_AUTH[] = { 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01 };
static const uint8_t OID_CLIENT_AUTH[] = { 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02 };
static const uint8_t OID_EXT_BASIC_CONSTRAINTS[] = { 0x55, 0x1D, 0x13 };
static const uint8_t OID_EXT_KEY_USAGE[] = { 0x55, 0x1D, 0x0F };
static const uint8_t OID_EXT_EXTENDED_KEY_USAGE[] = { 0x55, 0x1D, 0x25 };
static const uint8_t OID_EXT_SAN[] = { 0x55, 0x1D, 0x11 };
static const uint8_t OID_EXT_SKID[] = { 0x55, 0x1D, 0x0E };
static const uint8_t OID_EXT_AKID[] = { 0x55, 0x1D, 0x23 };
static const uint8_t OID_EXT_NETSCAPE_CERT_TYPE[] = { 0x60, 0x86, 0x48, 0x01, 0x86, 0xF8, 0x42, 0x01, 0x01 };

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

int8_t der_encoder_start_bit_string(der_encoder_t * encoder) {
    return _der_start_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BIT_STRING);
}

int8_t der_encoder_end_bit_string(der_encoder_t * encoder) {
    return _der_end_chain(encoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BIT_STRING);
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
    uint8_t int_bytes[17];
    size_t int_len = 0;

    if (value == 0) {
        int_bytes[0] = 0x00;
        int_len = 1;
    } else {
        while (value > 0) {
            int_bytes[int_len++] = (uint8_t)(value & 0xFF);
            value >>= 8;
        }

        // If MSB is 1, we must add a 0x00 byte to keep it positive
        if (int_bytes[int_len - 1] & 0x80) {
            int_bytes[int_len++] = 0x00;
        }

        // Reverse to Big Endian
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

int8_t der_encoder_encode_integer_u160(der_encoder_t * encoder, uint8_t value[20]) {
    uint8_t int_bytes[21];
    size_t int_len = 0;

    // Find first non-zero byte
    // If all zero, we encode as single 0x00 byte
    size_t start_index = 0;
    while (start_index < 20 && value[start_index] == 0) {
        start_index++;
    }

    if (start_index == 20) {
        int_bytes[0] = 0x00;
        int_len = 1;
    } else {
        // Copy relevant bytes
        for (size_t i = start_index; i < 20; i++) {
            int_bytes[int_len++] = value[i];
        }

        // If MSB is 1, we must add a 0x00 byte to keep it positive
        if (int_bytes[0] & 0x80) {
            // Shift right to make space for 0x00
            for (size_t i = int_len; i > 0; i--) {
                int_bytes[i] = int_bytes[i - 1];
            }
            int_bytes[0] = 0x00;
            int_len++;
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
    case DER_OID_ORGANIZATION: oid_data = OID_ORGANIZATION; oid_len = sizeof(OID_ORGANIZATION); break;
    case DER_OID_ORGANIZATIONAL_UNIT: oid_data = OID_ORGANIZATIONAL_UNIT; oid_len = sizeof(OID_ORGANIZATIONAL_UNIT); break;
    case DER_OID_COUNTRY: oid_data = OID_COUNTRY; oid_len = sizeof(OID_COUNTRY); break;
    case DER_OID_ED25519: oid_data = OID_ED25519; oid_len = sizeof(OID_ED25519); break;
    case DER_OID_X25519: oid_data = OID_X25519; oid_len = sizeof(OID_X25519); break;
    case DER_OID_ECDSA_WITH_SHA256: oid_data = OID_ECDSA_WITH_SHA256; oid_len = sizeof(OID_ECDSA_WITH_SHA256); break;
    case DER_OID_ECDSA_PUBLIC_KEY: oid_data = OID_ECDSA_PUBLIC_KEY; oid_len = sizeof(OID_ECDSA_PUBLIC_KEY); break;
    case DER_OID_EC_SECP256R1: oid_data = OID_EC_SECP256R1; oid_len = sizeof(OID_EC_SECP256R1); break;
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
                                DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_UTF8_STRING,
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

// Most X.509 certs are < 8 levels deep. 16 is plenty of safety margin.
#define DER_MAX_NESTING_DEPTH 16

struct der_decoder_t {
    const uint8_t* data;
    size_t         data_length;
    size_t         position;

    // Stack to track expected end positions
    size_t start_stack[DER_MAX_NESTING_DEPTH];
    size_t end_stack[DER_MAX_NESTING_DEPTH];
    int    stack_top; // Index of the next free slot (starts at 0)
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

static int8_t _der_decoder_push_container(der_decoder_t* decoder, uint8_t expected_tag) {
    if (!decoder || decoder->position >= decoder->data_length) {
        return -1;
    }

    // 1. Stack Overflow Check
    if (decoder->stack_top >= DER_MAX_NESTING_DEPTH) {
        // Error: Structure too deep (prevents stack overflow attacks)
        return -1;
    }

    // 2. Tag Check
    uint8_t tag = decoder->data[decoder->position];
    if (tag != expected_tag) {
        return -1;
    }

    decoder->start_stack[decoder->stack_top] = decoder->position;

    decoder->position++;

    // 3. Length Parse
    size_t length = 0;
    if (_der_decoder_parse_length(decoder, &length) != 0) {
        return -1;
    }

    // 4. Boundary Check
    if (decoder->position + length > decoder->data_length) {
        return -1;
    }

    // 5. Nested Boundary Check (Crucial!)
    // If we are already inside a container, the new container must fit inside it.
    if (decoder->stack_top > 0) {
        size_t current_limit = decoder->end_stack[decoder->stack_top - 1];
        if (decoder->position + length > current_limit) {
            return -1; // Child container extends beyond parent!
        }
    }

    // 6. Push to Stack
    decoder->end_stack[decoder->stack_top++] = decoder->position + length;

    return 0;
}

static int8_t _der_decoder_pop_container(der_decoder_t* decoder) {
    if (!decoder || decoder->stack_top == 0) {
        return -1;
    }

    // Get expected end from top of stack
    size_t expected_end = decoder->end_stack[decoder->stack_top - 1];

    // Strict Check: Did we consume exactly the right amount?
    if (decoder->position != expected_end) {
        return -1;
    }

    // Pop stack
    decoder->stack_top--;
    return 0;
}

boolean_t der_decoder_has_container_ended(der_decoder_t* decoder) {
    if (!decoder || decoder->stack_top == 0) {
        return true;
    }

    size_t expected_end = decoder->end_stack[decoder->stack_top - 1];
    return decoder->position >= expected_end ? true : false;
}

int8_t der_decoder_start_sequence(der_decoder_t* decoder) {
    return _der_decoder_push_container(decoder,
                                       DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_CONSTRUCTED | DER_TAG_NUMBER_SEQUENCE);
}

int8_t der_decoder_end_sequence(der_decoder_t* decoder) {
    return _der_decoder_pop_container(decoder);
}

int8_t der_decoder_start_octet_string(der_decoder_t* decoder) {
    // Note: We expect PRIMITIVE here because it's DER,
    // even though we treat it as a container logically.
    return _der_decoder_push_container(decoder,
                                       DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OCTET_STRING);
}

int8_t der_decoder_end_octet_string(der_decoder_t* decoder) {
    return _der_decoder_pop_container(decoder);
}

int8_t der_decoder_start_bit_string(der_decoder_t* decoder) {
    // Note: We expect PRIMITIVE here because it's DER,
    // even though we treat it as a container logically.
    return _der_decoder_push_container(decoder,
                                       DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BIT_STRING);
}

int8_t der_decoder_end_bit_string(der_decoder_t* decoder) {
    return _der_decoder_pop_container(decoder);
}

int8_t der_decoder_start_set(der_decoder_t* decoder) {
    return _der_decoder_push_container(decoder,
                                       DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_CONSTRUCTED | DER_TAG_NUMBER_SET);
}

int8_t der_decoder_end_set(der_decoder_t* decoder) {
    return _der_decoder_pop_container(decoder);
}

int8_t der_decoder_start_explicit_tag(der_decoder_t* decoder, der_tag_class_t inner_tag_class, uint8_t inner_tag_number) {
    uint8_t tag_byte = (uint8_t)(DER_TAG_TYPE_CONSTRUCTED | (inner_tag_class & 0xC0) | (inner_tag_number & 0x1F));
    return _der_decoder_push_container(decoder, tag_byte);
}

int8_t der_decoder_end_explicit_tag(der_decoder_t* decoder) {
    return _der_decoder_pop_container(decoder);
}

/**
 * @brief Validates Tag, Decodes Length, and checks strict boundaries.
 * * @param decoder The decoder instance.
 * @param expected_tag The exact tag byte expected (e.g. INTEGER | PRIMITIVE | UNIVERSAL).
 * @param out_len Pointer to store the decoded length.
 * @return 0 on success, -1 on mismatch or error.
 */
static int8_t _der_match_tlv(der_decoder_t* decoder, uint8_t expected_tag, size_t* out_len) {
    if (!decoder || decoder->position >= decoder->data_length || !out_len) {
        return -1;
    }

    // 1. Stack Safety Check (prevent reading past parent container)
    if (decoder->stack_top > 0) {
        if (decoder->position >= decoder->end_stack[decoder->stack_top - 1]) {
            return -1;
        }
    }

    // 2. Check Tag
    uint8_t tag = decoder->data[decoder->position];
    if (tag != expected_tag) {
        return -1;
    }
    decoder->position++; // Move past Tag

    // 3. Parse Length
    if (_der_decoder_parse_length(decoder, out_len) != 0) {
        return -1;
    }

    // 4. Global Boundary Check
    if (decoder->position + *out_len > decoder->data_length) {
        return -1;
    }

    // 5. Nested Boundary Check (The "Strict" Check)
    if (decoder->stack_top > 0) {
        if (decoder->position + *out_len > decoder->end_stack[decoder->stack_top - 1]) {
            return -1; // Child extends beyond parent
        }
    }

    return 0;
}

int8_t der_decoder_decode_integer(der_decoder_t* decoder, int64_t* out_value) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_INTEGER, &length) != 0) {
        return -1;
    }

    // Integer specific checks
    if (length == 0 || length > 8) {
        return -1; // We only support up to 64-bit integers
    }

    int64_t value = 0;
    // Read bytes (Big Endian)
    for (size_t i = 0; i < length; i++) {
        value = (value << 8) | decoder->data[decoder->position++];
    }

    *out_value = value;
    return 0;
}

int8_t der_decoder_decode_octet_string(der_decoder_t* decoder, uint8_t** out_data, size_t* out_data_len) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OCTET_STRING, &length) != 0) {
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

int8_t der_decoder_decode_bit_string(der_decoder_t* decoder, uint8_t** out_data, size_t* out_data_len) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BIT_STRING, &length) != 0) {
        return -1;
    }

    if (length == 0) {
        return -1; // BIT STRING must contain at least the "unused bits" byte

    }
    // Skip the "Unused Bits" byte (first byte)
    // Note: In strict DER, we should verify this is 0 for key data, but loose parsing ignores it.
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

typedef struct {
    der_object_identifier_t oid_enum;
    const uint8_t*          oid_bytes;
    size_t                  oid_len;
} oid_entry_t;

// Table of supported OIDs
static const oid_entry_t OID_TABLE[] = {
    { DER_OID_CN,                     OID_CN,                     sizeof(OID_CN) },
    { DER_OID_ORGANIZATION,           OID_ORGANIZATION,           sizeof(OID_ORGANIZATION) },
    { DER_OID_ORGANIZATIONAL_UNIT,    OID_ORGANIZATIONAL_UNIT,    sizeof(OID_ORGANIZATIONAL_UNIT) },
    { DER_OID_COUNTRY,                OID_COUNTRY,                sizeof(OID_COUNTRY) },
    { DER_OID_ED25519,                OID_ED25519,                sizeof(OID_ED25519) },
    { DER_OID_X25519,                 OID_X25519,                 sizeof(OID_X25519) },
    { DER_OID_ECDSA_WITH_SHA256,      OID_ECDSA_WITH_SHA256,      sizeof(OID_ECDSA_WITH_SHA256) },
    { DER_OID_ECDSA_PUBLIC_KEY,       OID_ECDSA_PUBLIC_KEY,       sizeof(OID_ECDSA_PUBLIC_KEY) },
    { DER_OID_EC_SECP256R1,           OID_EC_SECP256R1,           sizeof(OID_EC_SECP256R1) },
    { DER_OID_SERVER_AUTH,            OID_SERVER_AUTH,            sizeof(OID_SERVER_AUTH) },
    { DER_OID_CLIENT_AUTH,            OID_CLIENT_AUTH,            sizeof(OID_CLIENT_AUTH) },
    { DER_OID_EXT_BASIC_CONSTRAINTS,  OID_EXT_BASIC_CONSTRAINTS,  sizeof(OID_EXT_BASIC_CONSTRAINTS) },
    { DER_OID_EXT_KEY_USAGE,          OID_EXT_KEY_USAGE,          sizeof(OID_EXT_KEY_USAGE) },
    { DER_OID_EXT_EXTENDED_KEY_USAGE, OID_EXT_EXTENDED_KEY_USAGE, sizeof(OID_EXT_EXTENDED_KEY_USAGE) },
    { DER_OID_EXT_SAN,                OID_EXT_SAN,                sizeof(OID_EXT_SAN) },
    { DER_OID_EXT_SKID,               OID_EXT_SKID,               sizeof(OID_EXT_SKID) },
    { DER_OID_EXT_AKID,               OID_EXT_AKID,               sizeof(OID_EXT_AKID) },
    { DER_OID_EXT_NETSCAPE_CERT_TYPE, OID_EXT_NETSCAPE_CERT_TYPE, sizeof(OID_EXT_NETSCAPE_CERT_TYPE) },
};
#define OID_TABLE_COUNT (sizeof(OID_TABLE) / sizeof(oid_entry_t))

int8_t der_decoder_decode_object_identifier(der_decoder_t* decoder, der_object_identifier_t* out_oid) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_OBJECT_ID, &length) != 0) {
        return -1;
    }

    const uint8_t* raw_oid = &decoder->data[decoder->position];
    decoder->position += length; // Advance cursor now

    // Linear scan is efficient enough for small tables (< 20 items)
    for (size_t i = 0; i < OID_TABLE_COUNT; i++) {
        if (length == OID_TABLE[i].oid_len &&
            memory_memcompare(raw_oid, OID_TABLE[i].oid_bytes, length) == 0) {
            *out_oid = OID_TABLE[i].oid_enum;
            return 0;
        }
    }

    return -1; // Unknown OID
}

int8_t der_decoder_decode_printable_string(der_decoder_t* decoder, char_t** out_str, size_t* out_str_len) {
    size_t length = 0;
    // first try UTF8 STRING tag
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_UTF8_STRING, &length) != 0) {
        // failback PRINTABLE STRING tag
        if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_PRINTABLE_STRING, &length) != 0) {
            return -1;
        }
    }

    char_t* str = memory_malloc(length + 1);
    if (!str) {
        return -1;
    }

    memory_memcopy(&decoder->data[decoder->position], str, length);
    str[length] = '\0'; // Null-terminate
    decoder->position += length;

    *out_str = str;
    *out_str_len = length;
    return 0;
}

int8_t der_decoder_decode_boolean(der_decoder_t* decoder, boolean_t* out_value) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_BOOLEAN, &length) != 0) {
        return -1;
    }

    if (length != 1) {
        return -1; // Boolean must be exactly 1 byte
    }

    uint8_t bool_byte = decoder->data[decoder->position++];
    *out_value = (bool_byte != 0) ? true : false;
    return 0;
}

int8_t der_decoder_decode_integer_u128(der_decoder_t* decoder, uint128_t* out_value) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_INTEGER, &length) != 0) {
        return -1;
    }

    if (length > 17 || length == 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "DER Decoder: Unsupported integer length %llu\n", length);
        return -1;
    }

    // Peek at the first byte
    uint8_t first_byte = decoder->data[decoder->position];

    // If we have 17 bytes, the first byte MUST be 0x00
    if (length == 17) {
        if (first_byte != 0x00) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "DER Decoder: Integer overflow (17 bytes but first is not 0x00)\n");
            return -1;
        }
        // Skip the padding byte
        decoder->position++;
        length--;
    }
    // If we have 16 bytes or fewer, check for negative numbers (which we can't store in u128)
    else if (first_byte & 0x80) {
        // Technically a negative number in ASN.1, but we are parsing into u128.
        // Depending on your OS policy, you might want to return error or cast it.
        // For a strictly unsigned parser, this is usually an error.
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "DER Decoder: Unexpected negative integer\n");
        return -1;
    }

    uint128_t value = 0;
    // Read bytes (Big Endian)
    for (size_t i = 0; i < length; i++) {
        value = (value << 8) | decoder->data[decoder->position++];
    }

    *out_value = value;
    return 0;
}

int8_t der_decoder_decode_integer_u160(der_decoder_t* decoder, uint8_t out_value[20]) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_INTEGER, &length) != 0) {
        return -1;
    }

    if (length > 21 || length == 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "DER Decoder: Unsupported integer length %llu\n", length);
        return -1;
    }

    // Peek at the first byte
    uint8_t first_byte = decoder->data[decoder->position];

    // If we have 21 bytes, the first byte MUST be 0x00
    if (length == 21) {
        if (first_byte != 0x00) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "DER Decoder: Integer overflow (21 bytes but first is not 0x00)\n");
            return -1;
        }
        // Skip the padding byte
        decoder->position++;
        length--;
    }
    // If we have 16 bytes or fewer, check for negative numbers (which we can't store in u128)
    else if (first_byte & 0x80) {
        // Technically a negative number in ASN.1, but we are parsing into u128.
        // Depending on your OS policy, you might want to return error or cast it.
        // For a strictly unsigned parser, this is usually an error.
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "DER Decoder: Unexpected negative integer\n");
        return -1;
    }

    // Read bytes (Big Endian)
    for (size_t i = 0; i < length; i++) {
        out_value[20 - length + i] = decoder->data[decoder->position++];
    }

    return 0;
}

int8_t der_decoder_decode_context_specific_string(der_decoder_t* decoder, uint8_t expected_tag_number, uint8_t** out_data, size_t* out_data_len) {
    uint8_t expected_tag = DER_TAG_CLASS_CONTEXT_SPECIFIC | DER_TAG_TYPE_PRIMITIVE | (expected_tag_number & 0x1F);
    size_t length = 0;
    if (_der_match_tlv(decoder, expected_tag, &length) != 0) {
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

boolean_t der_decoder_has_context_specific_tag(der_decoder_t* decoder, uint8_t expected_tag_number) {
    if (!decoder || decoder->position >= decoder->data_length) {
        return false;
    }

    uint8_t expected_tag = DER_TAG_CLASS_CONTEXT_SPECIFIC | DER_TAG_TYPE_PRIMITIVE | (expected_tag_number & 0x1F);
    uint8_t tag = decoder->data[decoder->position];

    return (tag == expected_tag) ? true : false;
}

int8_t der_decoder_decode_utc_time(der_decoder_t* decoder, time_t* out_time_value) {
    size_t length = 0;
    if (_der_match_tlv(decoder, DER_TAG_CLASS_UNIVERSAL | DER_TAG_TYPE_PRIMITIVE | DER_TAG_NUMBER_UTC_TIME, &length) != 0) {
        return -1;
    }

    if (length >= 16) {
        return -1; // Too long for UTC time format
    }

    char_t time_str[16] = {0};
    memory_memcopy(&decoder->data[decoder->position], time_str, length);
    time_str[length] = '\0'; // Null-terminate
    decoder->position += length;

    time_t parsed_time = time_ns_parse_utc(time_str);
    if (parsed_time == (time_t)(-1)) {
        return -1; // Failed to parse
    }

    *out_time_value = parsed_time;
    return 0;
}

boolean_t der_decoder_has_tag(der_decoder_t* decoder, der_tag_class_t expected_tag_class, der_tag_type_t expected_tag_type, uint8_t expected_tag_number) {
    if (!decoder || decoder->position >= decoder->data_length) {
        return false;
    }

    uint8_t expected_tag = (uint8_t)(expected_tag_class | expected_tag_type | (expected_tag_number & 0x1F));
    uint8_t tag = decoder->data[decoder->position];

    return (tag == expected_tag) ? true : false;
}

boolean_t der_decoder_has_explicit_tag(der_decoder_t* decoder, der_tag_class_t expected_tag_class, uint8_t expected_tag_number) {
    if (!decoder || decoder->position >= decoder->data_length) {
        return false;
    }

    uint8_t expected_tag = (uint8_t)(DER_TAG_TYPE_CONSTRUCTED | (expected_tag_class & 0xC0) | (expected_tag_number & 0x1F));
    uint8_t tag = decoder->data[decoder->position];

    return (tag == expected_tag) ? true : false;
}

int8_t der_decoder_get_current_position(der_decoder_t* decoder, size_t* out_position) {
    if (!decoder || !out_position) {
        return -1;
    }
    *out_position = decoder->position;
    return 0;
}
