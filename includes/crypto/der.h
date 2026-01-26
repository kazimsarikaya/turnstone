/**
 * @file der.h
 * @brief DER (Distinguished Encoding Rules) Header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___DER_H
#define ___DER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <time.h>


typedef enum der_tag_class_t {
    DER_TAG_CLASS_UNIVERSAL        = 0x00,
    DER_TAG_CLASS_APPLICATION      = 0x40,
    DER_TAG_CLASS_CONTEXT_SPECIFIC = 0x80,
    DER_TAG_CLASS_PRIVATE          = 0xC0
} der_tag_class_t;

typedef enum der_tag_type_t {
    DER_TAG_TYPE_PRIMITIVE   = 0x00,
    DER_TAG_TYPE_CONSTRUCTED = 0x20
} der_tag_type_t;

typedef enum der_universal_tag_number_t {
    DER_TAG_NUMBER_EOC             = 0x00,
    DER_TAG_NUMBER_BOOLEAN         = 0x01,
    DER_TAG_NUMBER_INTEGER         = 0x02,
    DER_TAG_NUMBER_BIT_STRING      = 0x03,
    DER_TAG_NUMBER_OCTET_STRING    = 0x04,
    DER_TAG_NUMBER_NULL            = 0x05,
    DER_TAG_NUMBER_OBJECT_ID       = 0x06,
    DER_TAG_NUMBER_OBJECT_DESC     = 0x07,
    DER_TAG_NUMBER_EXTERNAL        = 0x08,
    DER_TAG_NUMBER_REAL            = 0x09,
    DER_TAG_NUMBER_ENUMERATED      = 0x0A,
    DER_TAG_NUMBER_UTF8_STRING     = 0x0C,
    DER_TAG_NUMBER_SEQUENCE        = 0x10,
    DER_TAG_NUMBER_SET             = 0x11,
    DER_TAG_NUMBER_PRINTABLE_STRING= 0x13,
    DER_TAG_NUMBER_T61_STRING      = 0x14,
    DER_TAG_NUMBER_IA5_STRING      = 0x16,
    DER_TAG_NUMBER_UTC_TIME        = 0x17,
    DER_TAG_NUMBER_GENERALIZED_TIME= 0x18,
    DER_TAG_NUMBER_UNIVERSAL_STRING= 0x1C,
} der_universal_tag_number_t;

typedef enum der_object_identifier_t {
    DER_OID_UNDEFINED = 0,
    DER_OID_CN,
    DER_OID_ED25519,
    DER_OID_X25519,
    DER_OID_SERVER_AUTH,
    DER_OID_CLIENT_AUTH,
    DER_OID_EXT_BASIC_CONSTRAINTS,
    DER_OID_EXT_KEY_USAGE,
    DER_OID_EXT_EXTENDED_KEY_USAGE,
    DER_OID_EXT_SAN,
    DER_OID_EXT_SKID,
    DER_OID_EXT_AKID,
} der_object_identifier_t;

typedef struct der_encoder_t der_encoder_t;


der_encoder_t* der_encoder_new();
void           der_encoder_destroy(der_encoder_t * encoder);

int8_t der_encoder_start_sequence(der_encoder_t * encoder);
int8_t der_encoder_end_sequence(der_encoder_t * encoder);

int8_t der_encoder_start_octet_string(der_encoder_t * encoder);
int8_t der_encoder_end_octet_string(der_encoder_t * encoder);

int8_t der_encoder_start_set(der_encoder_t * encoder);
int8_t der_encoder_end_set(der_encoder_t * encoder);

int8_t der_encoder_start_explicit_tag(der_encoder_t * encoder, der_tag_class_t inner_tag_class, uint8_t inner_tag_number);
int8_t der_encoder_end_explicit_tag(der_encoder_t * encoder);

int8_t der_encoder_add_tag(der_encoder_t * encoder, der_tag_class_t tag_class, der_tag_type_t tag_type, uint8_t tag_number);

int8_t der_encoder_encode_integer(der_encoder_t * encoder, int64_t value);
int8_t der_encoder_encode_octet_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len);
int8_t der_encoder_encode_object_identifier(der_encoder_t * encoder, der_object_identifier_t oid);
int8_t der_encoder_encode_bit_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len);
int8_t der_encoder_encode_printable_string(der_encoder_t * encoder, const char_t * str, size_t str_len);
int8_t der_encoder_encode_boolean(der_encoder_t * encoder, boolean_t value);
int8_t der_encoder_encode_context_specific_string(der_encoder_t * encoder, uint8_t tag_number, const uint8_t * data, size_t data_len);
int8_t der_encoder_encode_utc_time(der_encoder_t * encoder, time_t time_value);
int8_t der_encoder_encode_integer_u128(der_encoder_t * encoder, uint128_t value);
int8_t der_encoder_encode_raw_bytes(der_encoder_t * encoder, const uint8_t * data, size_t data_len);

int8_t der_encoder_get_der_data(der_encoder_t * encoder, uint8_t ** out_data, size_t * out_len);

#ifdef __cplusplus
}
#endif

#endif // ___DER_H
