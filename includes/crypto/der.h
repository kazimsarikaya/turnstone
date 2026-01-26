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

/**
 * @brief Enumeration for DER tag classes.
 *
 * DER (Distinguished Encoding Rules) uses tag classes to categorize data types.
 */
typedef enum der_tag_class_t {
    DER_TAG_CLASS_UNIVERSAL        = 0x00, ///< Universal class, applies to common ASN.1 types.
    DER_TAG_CLASS_APPLICATION      = 0x40, ///< Application class, specific to ASN.1 applications.
    DER_TAG_CLASS_CONTEXT_SPECIFIC = 0x80, ///< Context-specific class, used for specific structures.
    DER_TAG_CLASS_PRIVATE          = 0xC0 ///< Private class, for proprietary use.
} der_tag_class_t;

/**
 * @brief Enumeration for DER tag types.
 *
 * DER tag types indicate whether the data is a primitive value or a constructed sequence/set.
 */
typedef enum der_tag_type_t {
    DER_TAG_TYPE_PRIMITIVE   = 0x00, ///< Primitive type, data is encoded directly.
    DER_TAG_TYPE_CONSTRUCTED = 0x20 ///< Constructed type, data is a sequence or set of other DER elements.
} der_tag_type_t;

/**
 * @brief Enumeration for common DER universal tag numbers.
 *
 * These represent standard ASN.1 data types.
 */
typedef enum der_universal_tag_number_t {
    DER_TAG_NUMBER_EOC             = 0x00, ///< End of Content
    DER_TAG_NUMBER_BOOLEAN         = 0x01, ///< Boolean
    DER_TAG_NUMBER_INTEGER         = 0x02, ///< Integer
    DER_TAG_NUMBER_BIT_STRING      = 0x03, ///< Bit String
    DER_TAG_NUMBER_OCTET_STRING    = 0x04, ///< Octet String
    DER_TAG_NUMBER_NULL            = 0x05, ///< NULL
    DER_TAG_NUMBER_OBJECT_ID       = 0x06, ///< Object Identifier
    DER_TAG_NUMBER_OBJECT_DESC     = 0x07, ///< Object Description
    DER_TAG_NUMBER_EXTERNAL        = 0x08, ///< External
    DER_TAG_NUMBER_REAL            = 0x09, ///< Real
    DER_TAG_NUMBER_ENUMERATED      = 0x0A, ///< Enumerated
    DER_TAG_NUMBER_UTF8_STRING     = 0x0C, ///< UTF8 String
    DER_TAG_NUMBER_SEQUENCE        = 0x10, ///< Sequence
    DER_TAG_NUMBER_SET             = 0x11, ///< Set
    DER_TAG_NUMBER_PRINTABLE_STRING= 0x13, ///< Printable String
    DER_TAG_NUMBER_T61_STRING      = 0x14, ///< T61 String
    DER_TAG_NUMBER_IA5_STRING      = 0x16, ///< IA5 String
    DER_TAG_NUMBER_UTC_TIME        = 0x17, ///< UTC Time
    DER_TAG_NUMBER_GENERALIZED_TIME= 0x18, ///< Generalized Time
    DER_TAG_NUMBER_UNIVERSAL_STRING= 0x1C, ///< Universal String
} der_universal_tag_number_t;

/**
 * @brief Enumeration for predefined Object Identifiers (OIDs).
 *
 * This enumeration lists common OIDs used in cryptographic contexts.
 */
typedef enum der_object_identifier_t {
    DER_OID_UNDEFINED = 0,
    DER_OID_CN, ///< Common Name
    DER_OID_ED25519, ///< Ed25519 signature algorithm OID
    DER_OID_X25519, ///< X25519 key exchange algorithm OID
    DER_OID_SERVER_AUTH, ///< Server Authentication EKU
    DER_OID_CLIENT_AUTH, ///< Client Authentication EKU
    DER_OID_EXT_BASIC_CONSTRAINTS, ///< Basic Constraints extension OID
    DER_OID_EXT_KEY_USAGE, ///< Key Usage extension OID
    DER_OID_EXT_EXTENDED_KEY_USAGE, ///< Extended Key Usage extension OID
    DER_OID_EXT_SAN, ///< Subject Alternative Name extension OID
    DER_OID_EXT_SKID, ///< Subject Key Identifier extension OID
    DER_OID_EXT_AKID, ///< Authority Key Identifier extension OID
} der_object_identifier_t;

/**
 * @brief Opaque structure representing a DER encoder.
 *
 * This structure holds the internal state of the DER encoder.
 */
typedef struct der_encoder_t der_encoder_t;


/**
 * @brief Creates a new DER encoder instance.
 *
 * @return A pointer to the newly created der_encoder_t, or NULL if allocation fails.
 */
der_encoder_t* der_encoder_new();

/**
 * @brief Destroys a DER encoder instance and frees associated resources.
 *
 * @param encoder The DER encoder to destroy.
 */
void der_encoder_destroy(der_encoder_t * encoder);

/**
 * @brief Starts encoding a DER sequence.
 *
 * This function should be called before adding elements to a sequence.
 * The corresponding `der_encoder_end_sequence` must be called later.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_start_sequence(der_encoder_t * encoder);

/**
 * @brief Ends encoding a DER sequence.
 *
 * This function finalizes the current sequence and appends it to the parent structure
 * or sets it as the final output if it's the top-level sequence.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_end_sequence(der_encoder_t * encoder);

/**
 * @brief Starts encoding a DER octet string.
 *
 * This function is used when an octet string will contain other DER encoded elements.
 * The corresponding `der_encoder_end_octet_string` must be called later.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_start_octet_string(der_encoder_t * encoder);

/**
 * @brief Ends encoding a DER octet string.
 *
 * This function finalizes the current octet string and appends it to the parent structure.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_end_octet_string(der_encoder_t * encoder);

/**
 * @brief Starts encoding a DER set.
 *
 * This function should be called before adding elements to a set.
 * The corresponding `der_encoder_end_set` must be called later.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_start_set(der_encoder_t * encoder);

/**
 * @brief Ends encoding a DER set.
 *
 * This function finalizes the current set and appends it to the parent structure.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_end_set(der_encoder_t * encoder);

/**
 * @brief Starts encoding an explicit tag.
 *
 * This is used for context-specific or application-specific tagging of DER elements.
 * The `inner_tag_class` and `inner_tag_number` define the tag to be applied.
 * The corresponding `der_encoder_end_explicit_tag` must be called later.
 *
 * @param encoder The DER encoder instance.
 * @param inner_tag_class The tag class for the explicit tag.
 * @param inner_tag_number The tag number for the explicit tag.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_start_explicit_tag(der_encoder_t * encoder, der_tag_class_t inner_tag_class, uint8_t inner_tag_number);

/**
 * @brief Ends encoding an explicit tag.
 *
 * This function finalizes the explicit tag and appends it to the parent structure.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_end_explicit_tag(der_encoder_t * encoder);

/**
 * @brief Adds a DER tag to the current buffer.
 *
 * This function is typically used internally by other encoding functions or for
 * manually constructing DER elements.
 *
 * @param encoder The DER encoder instance.
 * @param tag_class The class of the tag.
 * @param tag_type The type of the tag (primitive or constructed).
 * @param tag_number The number of the tag.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_add_tag(der_encoder_t * encoder, der_tag_class_t tag_class, der_tag_type_t tag_type, uint8_t tag_number);

/**
 * @brief Encodes and appends a DER integer.
 *
 * @param encoder The DER encoder instance.
 * @param value The integer value to encode.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_integer(der_encoder_t * encoder, int64_t value);

/**
 * @brief Encodes and appends a DER octet string.
 *
 * @param encoder The DER encoder instance.
 * @param data Pointer to the data to be encoded.
 * @param data_len Length of the data.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_octet_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len);

/**
 * @brief Encodes and appends a DER object identifier.
 *
 * @param encoder The DER encoder instance.
 * @param oid The predefined object identifier from `der_object_identifier_t`.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_object_identifier(der_encoder_t * encoder, der_object_identifier_t oid);

/**
 * @brief Encodes and appends a DER bit string.
 *
 * @param encoder The DER encoder instance.
 * @param data Pointer to the bit string data.
 * @param data_len Length of the data (excluding the unused bits byte).
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_bit_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len);

/**
 * @brief Encodes and appends a DER printable string.
 *
 * @param encoder The DER encoder instance.
 * @param str Pointer to the null-terminated string.
 * @param str_len Length of the string.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_printable_string(der_encoder_t * encoder, const char_t * str, size_t str_len);

/**
 * @brief Encodes and appends a DER boolean.
 *
 * @param encoder The DER encoder instance.
 * @param value The boolean value (true or false).
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_boolean(der_encoder_t * encoder, boolean_t value);

/**
 * @brief Encodes and appends a context-specific string.
 *
 * @param encoder The DER encoder instance.
 * @param tag_number The context-specific tag number.
 * @param data Pointer to the string data.
 * @param data_len Length of the string data.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_context_specific_string(der_encoder_t * encoder, uint8_t tag_number, const uint8_t * data, size_t data_len);

/**
 * @brief Encodes and appends a DER UTC time.
 *
 * The time is formatted according to the UTCTime standard (YYMMDDHHMMSSZ).
 *
 * @param encoder The DER encoder instance.
 * @param time_value The time value (seconds since epoch).
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_utc_time(der_encoder_t * encoder, time_t time_value);

/**
 * @brief Encodes and appends a DER integer using a 128-bit unsigned integer.
 *
 * @param encoder The DER encoder instance.
 * @param value The unsigned 128-bit integer value to encode.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_integer_u128(der_encoder_t * encoder, uint128_t value);

/**
 * @brief Appends raw bytes directly to the DER buffer.
 *
 * This is useful for appending pre-encoded DER elements or raw data.
 *
 * @param encoder The DER encoder instance.
 * @param data Pointer to the raw data.
 * @param data_len Length of the raw data.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_encode_raw_bytes(der_encoder_t * encoder, const uint8_t * data, size_t data_len);

/**
 * @brief Retrieves the final DER encoded data.
 *
 * This function consumes the encoder and returns the encoded data. The caller is
 * responsible for freeing the returned data.
 *
 * @param encoder The DER encoder instance.
 * @param out_data Pointer to a pointer that will receive the DER data.
 * @param out_len Pointer to a size_t that will receive the length of the DER data.
 * @return 0 on success, -1 on failure.
 */
int8_t der_encoder_get_der_data(der_encoder_t * encoder, uint8_t ** out_data, size_t * out_len);

#ifdef __cplusplus
}
#endif

#endif // ___DER_H
