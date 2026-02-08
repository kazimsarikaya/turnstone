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
    DER_OID_ORGANIZATION, ///< Organization Name
    DER_OID_ORGANIZATIONAL_UNIT, ///< Organizational Unit Name
    DER_OID_COUNTRY, ///< Country Name
    DER_OID_CN, ///< Common Name
    DER_OID_ED25519, ///< Ed25519 signature algorithm OID
    DER_OID_X25519, ///< X25519 key exchange algorithm OID
    DER_OID_ECDSA_WITH_SHA256, ///< ECDSA with SHA-256 signature algorithm OID
    DER_OID_ECDSA_PUBLIC_KEY, ///< ECDSA public key OID
    DER_OID_EC_SECP256R1, ///< secp256r1 curve OID
    DER_OID_SERVER_AUTH, ///< Server Authentication EKU
    DER_OID_CLIENT_AUTH, ///< Client Authentication EKU
    DER_OID_CODE_SIGNING, ///< Code Signing EKU
    DER_OID_EMAIL_PROTECTION, ///< Email Protection EKU
    DER_OID_TIME_STAMPING, ///< Time Stamping EKU
    DER_OID_OCSP_SIGNING, ///< OCSP Signing EKU
    DER_OID_EXT_BASIC_CONSTRAINTS, ///< Basic Constraints extension OID
    DER_OID_EXT_KEY_USAGE, ///< Key Usage extension OID
    DER_OID_EXT_EXTENDED_KEY_USAGE, ///< Extended Key Usage extension OID
    DER_OID_EXT_SAN, ///< Subject Alternative Name extension OID
    DER_OID_EXT_SKID, ///< Subject Key Identifier extension OID
    DER_OID_EXT_AKID, ///< Authority Key Identifier extension OID
    DER_OID_EXT_NETSCAPE_CERT_TYPE ///< Netscape Certificate Type extension OID
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
 * Allocates memory for a new DER encoder and initializes its internal state.
 *
 * @return A pointer to the newly created `der_encoder_t`, or `NULL` if memory allocation fails.
 */
der_encoder_t* der_encoder_new();

/**
 * @brief Destroys a DER encoder instance and frees associated resources.
 *
 * This function releases all memory allocated by the DER encoder.
 *
 * @param encoder The DER encoder to destroy. If `NULL`, the function does nothing.
 */
void der_encoder_destroy(der_encoder_t * encoder);

/**
 * @brief Starts encoding a DER sequence.
 *
 * This function should be called before adding elements to a sequence. It pushes a new
 * sequence context onto the encoder's internal stack. The corresponding `der_encoder_end_sequence`
 * must be called later to finalize and append the sequence.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, memory allocation error).
 */
int8_t der_encoder_start_sequence(der_encoder_t * encoder);

/**
 * @brief Ends encoding a DER sequence.
 *
 * This function finalizes the current sequence, calculates its length, and appends it
 * to the parent structure or sets it as the final output if it's the top-level sequence.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, mismatched start/end calls).
 */
int8_t der_encoder_end_sequence(der_encoder_t * encoder);

/**
 * @brief Starts encoding a DER octet string.
 *
 * This function is used when an octet string will contain other DER encoded elements (i.e., it's a constructed octet string).
 * It pushes a new octet string context onto the encoder's internal stack.
 * The corresponding `der_encoder_end_octet_string` must be called later to finalize and append the octet string.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, memory allocation error).
 */
int8_t der_encoder_start_octet_string(der_encoder_t * encoder);

/**
 * @brief Starts encoding a DER bit string.
 *
 * This function is used when a bit string will contain other DER encoded elements (i.e., it's a constructed bit string).
 * It pushes a new bit string context onto the encoder's internal stack.
 * The corresponding `der_encoder_end_bit_string` must be called later to finalize and append the bit string.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, memory allocation error).
 */
int8_t der_encoder_start_bit_string(der_encoder_t * encoder);

/**
 * @brief Ends encoding a DER bit string.
 *
 * This function finalizes the current bit string, calculates its length, and appends it
 * to the parent structure.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, mismatched start/end calls).
 */
int8_t der_encoder_end_bit_string(der_encoder_t * encoder);

/**
 * @brief Ends encoding a DER octet string.
 *
 * This function finalizes the current octet string, calculates its length, and appends it
 * to the parent structure.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, mismatched start/end calls).
 */
int8_t der_encoder_end_octet_string(der_encoder_t * encoder);

/**
 * @brief Starts encoding a DER set.
 *
 * This function should be called before adding elements to a set. It pushes a new
 * set context onto the encoder's internal stack. The corresponding `der_encoder_end_set`
 * must be called later to finalize and append the set.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, memory allocation error).
 */
int8_t der_encoder_start_set(der_encoder_t * encoder);

/**
 * @brief Ends encoding a DER set.
 *
 * This function finalizes the current set, calculates its length, and appends it
 * to the parent structure.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, mismatched start/end calls).
 */
int8_t der_encoder_end_set(der_encoder_t * encoder);

/**
 * @brief Starts encoding an explicit tag.
 *
 * This is used for context-specific or application-specific tagging of DER elements.
 * It pushes a new explicit tag context onto the encoder's internal stack.
 * The `inner_tag_class` and `inner_tag_number` define the tag to be applied.
 * The corresponding `der_encoder_end_explicit_tag` must be called later to finalize and append the tagged element.
 *
 * @param encoder The DER encoder instance.
 * @param inner_tag_class The tag class for the explicit tag (e.g., `DER_TAG_CLASS_CONTEXT_SPECIFIC`).
 * @param inner_tag_number The tag number for the explicit tag.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, memory allocation error).
 */
int8_t der_encoder_start_explicit_tag(der_encoder_t * encoder, der_tag_class_t inner_tag_class, uint8_t inner_tag_number);

/**
 * @brief Ends encoding an explicit tag.
 *
 * This function finalizes the current explicit tag, calculates its length, and appends it
 * to the parent structure.
 *
 * @param encoder The DER encoder instance.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, mismatched start/end calls).
 */
int8_t der_encoder_end_explicit_tag(der_encoder_t * encoder);

/**
 * @brief Adds a DER tag to the current buffer.
 *
 * This function manually adds a tag byte to the current DER encoding buffer.
 * It is typically used internally by other encoding functions or for advanced manual construction of DER elements.
 *
 * @param encoder The DER encoder instance.
 * @param tag_class The class of the tag (e.g., `DER_TAG_CLASS_UNIVERSAL`).
 * @param tag_type The type of the tag (e.g., `DER_TAG_TYPE_PRIMITIVE` or `DER_TAG_TYPE_CONSTRUCTED`).
 * @param tag_number The number of the tag.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_add_tag(der_encoder_t * encoder, der_tag_class_t tag_class, der_tag_type_t tag_type, uint8_t tag_number);

/**
 * @brief Encodes and appends a DER integer.
 *
 * Encodes the given 64-bit signed integer into DER format and appends it to the current buffer.
 *
 * @param encoder The DER encoder instance.
 * @param value The integer value to encode.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_integer(der_encoder_t * encoder, int64_t value);

/**
 * @brief Encodes and appends a DER octet string.
 *
 * Encodes the given byte array as a DER octet string and appends it to the current buffer.
 *
 * @param encoder The DER encoder instance.
 * @param data Pointer to the data to be encoded.
 * @param data_len Length of the data.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_octet_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len);

/**
 * @brief Encodes and appends a DER object identifier.
 *
 * Encodes a predefined object identifier (OID) from the `der_object_identifier_t` enumeration
 * into DER format and appends it to the current buffer.
 *
 * @param encoder The DER encoder instance.
 * @param oid The predefined object identifier from `der_object_identifier_t`.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, unknown OID, buffer error).
 */
int8_t der_encoder_encode_object_identifier(der_encoder_t * encoder, der_object_identifier_t oid);

/**
 * @brief Encodes and appends a DER bit string.
 *
 * Encodes the given byte array as a DER bit string. Note that DER bit strings include a byte
 * indicating the number of unused bits in the last byte of the actual data. This function
 * assumes all bits in the provided data are used and sets the unused bits count to 0.
 *
 * @param encoder The DER encoder instance.
 * @param data Pointer to the bit string data.
 * @param data_len Length of the data (excluding the unused bits byte).
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_bit_string(der_encoder_t * encoder, const uint8_t * data, size_t data_len);

/**
 * @brief Encodes and appends a DER printable string.
 *
 * Encodes the given string as a DER utf8 (not printable) string and appends it to the current buffer.
 * Printable strings typically contain characters like A-Z, a-z, 0-9, and some punctuation.
 *
 * @param encoder The DER encoder instance.
 * @param str Pointer to the null-terminated string.
 * @param str_len Length of the string.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_printable_string(der_encoder_t * encoder, const char_t * str, size_t str_len);

/**
 * @brief Encodes and appends a DER boolean.
 *
 * Encodes the given boolean value (true or false) into DER format and appends it to the current buffer.
 * DER encoding for boolean uses `0xFF` for true and `0x00` for false.
 *
 * @param encoder The DER encoder instance.
 * @param value The boolean value (true or false).
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_boolean(der_encoder_t * encoder, boolean_t value);

/**
 * @brief Encodes and appends a context-specific string.
 *
 * Encodes the given byte array as a DER primitive type with a context-specific tag.
 *
 * @param encoder The DER encoder instance.
 * @param tag_number The context-specific tag number (0-30).
 * @param data Pointer to the string data.
 * @param data_len Length of the string data.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_context_specific_string(der_encoder_t * encoder, uint8_t tag_number, const uint8_t * data, size_t data_len);

/**
 * @brief Encodes and appends a DER UTC time.
 *
 * Encodes the given time value (seconds since epoch) into DER UTCTime format (YYMMDDHHMMSSZ)
 * and appends it to the current buffer.
 *
 * @param encoder The DER encoder instance.
 * @param time_value The time value (seconds since epoch).
 * @return 0 on success, -1 on failure (e.g., invalid encoder, time formatting error, buffer error).
 */
int8_t der_encoder_encode_utc_time(der_encoder_t * encoder, time_t time_value);

/**
 * @brief Encodes and appends a DER integer using a 128-bit unsigned integer.
 *
 * Encodes the given 128-bit unsigned integer into DER format and appends it to the current buffer.
 * This is useful for very large integers that exceed the capacity of `int64_t`.
 *
 * @param encoder The DER encoder instance.
 * @param value The unsigned 128-bit integer value to encode.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_integer_u128(der_encoder_t * encoder, uint128_t value);

/**
 * @brief Encodes and appends a DER integer using a 160-bit unsigned integer.
 *
 * Encodes the given 160-bit unsigned integer (20 bytes) into DER format and appends it to the current buffer.
 * This is useful for very large integers that exceed the capacity of `uint128_t`.
 *
 * @param encoder The DER encoder instance.
 * @param value A 20-byte array representing the unsigned 160-bit integer in big-endian format.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_integer_u160(der_encoder_t * encoder, const uint8_t value[20]);

/**
 * @brief Encodes and appends a DER integer using a 256-bit unsigned integer.
 *
 * Encodes the given 256-bit unsigned integer (32 bytes) into DER format and appends it to the current buffer.
 * This is useful for very large integers that exceed the capacity of `uint128_t`.
 *
 * @param encoder The DER encoder instance.
 * @param value A 32-byte array representing the unsigned 256-bit integer in big-endian format.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_integer_u256(der_encoder_t * encoder, const uint8_t value[32]);

/**
 * @brief Appends raw bytes directly to the DER buffer.
 *
 * This function allows appending pre-encoded DER elements or raw data directly into the encoder's buffer.
 * Use with caution, as it bypasses standard DER encoding rules for the appended data.
 *
 * @param encoder The DER encoder instance.
 * @param data Pointer to the raw data.
 * @param data_len Length of the raw data.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, buffer error).
 */
int8_t der_encoder_encode_raw_bytes(der_encoder_t * encoder, const uint8_t * data, size_t data_len);

/**
 * @brief Retrieves the final DER encoded data.
 *
 * This function finalizes the encoding process, retrieves the complete DER encoded byte sequence,
 * and consumes the encoder instance. The caller is responsible for freeing the returned data buffer.
 *
 * @param encoder The DER encoder instance. This instance will be invalidated after this call.
 * @param out_data Pointer to a pointer that will receive the allocated buffer containing the DER data.
 * @param out_len Pointer to a `size_t` variable that will receive the length of the DER data.
 * @return 0 on success, -1 on failure (e.g., invalid encoder, incomplete encoding, memory allocation error).
 */
int8_t der_encoder_get_der_data(der_encoder_t * encoder, uint8_t ** out_data, size_t * out_len);


/**
 * @brief Opaque structure representing a DER decoder.
 *
 * This structure holds the internal state of the DER decoder, including the data buffer,
 * current position, and a stack for tracking nested structures.
 */
typedef struct der_decoder_t der_decoder_t;

/**
 * @brief Creates a new DER decoder instance.
 *
 * Initializes a DER decoder with the provided data buffer.
 *
 * @param data Pointer to the DER-encoded data.
 * @param data_len Length of the DER-encoded data.
 * @return A pointer to the newly created `der_decoder_t`, or `NULL` if initialization fails (e.g., invalid input, memory allocation error).
 */
der_decoder_t* der_decoder_new(const uint8_t* data, size_t data_len);

/**
 * @brief Destroys a DER decoder instance and frees associated resources.
 *
 * @param decoder The DER decoder to destroy. If `NULL`, the function does nothing.
 */
void der_decoder_destroy(der_decoder_t* decoder);

/**
 * @brief Checks if the decoder has reached the end of the data buffer.
 *
 * @param decoder The DER decoder instance.
 * @return `true` if the decoder's current position is at or beyond the end of the data, `false` otherwise.
 */
boolean_t der_decoder_is_at_end(der_decoder_t* decoder);

/**
 * @brief Starts decoding a DER sequence.
 *
 * Attempts to parse the next element in the DER data as a sequence. If successful, it pushes
 * the sequence's boundaries onto the decoder's stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not a sequence, unexpected end of data, invalid structure).
 */
int8_t der_decoder_start_sequence(der_decoder_t* decoder);

/**
 * @brief Ends decoding a DER sequence.
 *
 * Checks if the decoder has consumed all elements within the current sequence context.
 * If the end of the sequence is reached correctly, it pops the sequence's boundaries from the stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not at the end of the sequence, stack underflow).
 */
int8_t der_decoder_end_sequence(der_decoder_t* decoder);

/**
 * @brief Starts decoding a DER octet string.
 *
 * Attempts to parse the next element as an octet string. If successful, it pushes
 * the octet string's boundaries onto the decoder's stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not an octet string, unexpected end of data).
 */
int8_t der_decoder_start_octet_string(der_decoder_t* decoder);

/**
 * @brief Ends decoding a DER octet string.
 *
 * Checks if the decoder has consumed all data within the current octet string context.
 * If the end is reached correctly, it pops the octet string's boundaries from the stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not at the end of the octet string, stack underflow).
 */
int8_t der_decoder_end_octet_string(der_decoder_t* decoder);

/**
 * @brief Starts decoding a DER bit string.
 *
 * Attempts to parse the next element as a bit string. If successful, it pushes
 * the bit string's boundaries onto the decoder's stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not a bit string, unexpected end of data).
 */
int8_t der_decoder_start_bit_string(der_decoder_t* decoder);

/**
 * @brief Ends decoding a DER bit string.
 *
 * Checks if the decoder has consumed all data within the current bit string context.
 * If the end is reached correctly, it pops the bit string's boundaries from the stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not at the end of the bit string, stack underflow).
 */
int8_t der_decoder_end_bit_string(der_decoder_t* decoder);

/**
 * @brief Starts decoding a DER set.
 *
 * Attempts to parse the next element as a set. If successful, it pushes
 * the set's boundaries onto the decoder's stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not a set, unexpected end of data).
 */
int8_t der_decoder_start_set(der_decoder_t* decoder);

/**
 * @brief Ends decoding a DER set.
 *
 * Checks if the decoder has consumed all elements within the current set context.
 * If the end of the set is reached correctly, it pops the set's boundaries from the stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not at the end of the set, stack underflow).
 */
int8_t der_decoder_end_set(der_decoder_t* decoder);

/**
 * @brief Starts decoding an explicit tag.
 *
 * Attempts to parse the next element as an explicit tag with the specified class and number.
 * If successful, it pushes the tag's boundaries onto the decoder's stack.
 *
 * @param decoder The DER decoder instance.
 * @param inner_tag_class The expected tag class.
 * @param inner_tag_number The expected tag number.
 * @return 0 on success, -1 on failure (e.g., tag mismatch, unexpected end of data).
 */
int8_t der_decoder_start_explicit_tag(der_decoder_t* decoder, der_tag_class_t inner_tag_class, uint8_t inner_tag_number);

/**
 * @brief Ends decoding an explicit tag.
 *
 * Checks if the decoder has consumed all data within the current explicit tag context.
 * If the end is reached correctly, it pops the tag's boundaries from the stack.
 *
 * @param decoder The DER decoder instance.
 * @return 0 on success, -1 on failure (e.g., not at the end of the tag, stack underflow).
 */
int8_t der_decoder_end_explicit_tag(der_decoder_t* decoder);

/**
 * @brief Decodes a DER integer.
 *
 * Attempts to parse the next element as a DER integer and stores the value in `out_value`.
 * Currently supports up to 64-bit signed integers.
 *
 * @param decoder The DER decoder instance.
 * @param out_value Pointer to a `int64_t` where the decoded integer will be stored.
 * @return 0 on success, -1 on failure (e.g., not an integer, invalid format, value out of range).
 */
int8_t der_decoder_decode_integer(der_decoder_t* decoder, int64_t* out_value);

/**
 * @brief Decodes a DER object identifier.
 *
 * Attempts to parse the next element as a DER object identifier (OID) and stores the
 * corresponding enum value in `out_oid`. Only known OIDs are supported.
 *
 * @param decoder The DER decoder instance.
 * @param out_oid Pointer to a `der_object_identifier_t` where the decoded OID enum will be stored.
 * @return 0 on success, -1 on failure (e.g., not an OID, unknown OID, invalid format).
 */
int8_t der_decoder_decode_object_identifier(der_decoder_t* decoder, der_object_identifier_t* out_oid);

/**
 * @brief Decodes a DER octet string.
 *
 * Attempts to parse the next element as a DER octet string. If successful, it allocates memory
 * for the string data and stores a pointer to it in `out_data` and its length in `out_data_len`.
 * The caller is responsible for freeing the allocated memory using `memory_free`.
 *
 * @param decoder The DER decoder instance.
 * @param out_data Pointer to a pointer that will receive the allocated buffer containing the octet string data.
 * @param out_data_len Pointer to a `size_t` that will receive the length of the octet string data.
 * @return 0 on success, -1 on failure (e.g., not an octet string, memory allocation error).
 */
int8_t der_decoder_decode_octet_string(der_decoder_t* decoder, uint8_t** out_data, size_t* out_data_len);

/**
 * @brief Decodes a DER bit string.
 *
 * Attempts to parse the next element as a DER bit string. If successful, it allocates memory
 * for the bit string data (excluding the unused bits byte) and stores a pointer to it in `out_data`
 * and its length in `out_data_len`. The caller is responsible for freeing the allocated memory.
 *
 * @param decoder The DER decoder instance.
 * @param out_data Pointer to a pointer that will receive the allocated buffer containing the bit string data.
 * @param out_data_len Pointer to a `size_t` that will receive the length of the bit string data.
 * @return 0 on success, -1 on failure (e.g., not a bit string, memory allocation error).
 */
int8_t der_decoder_decode_bit_string(der_decoder_t* decoder, uint8_t** out_data, size_t* out_data_len);

/**
 * @brief Decodes a DER printable string.
 *
 * Attempts to parse the next element as a DER printable/utf8 string. If successful, it allocates memory
 * for the string, null-terminates it, and stores a pointer to it in `out_str` and its length in `out_str_len`.
 * The caller is responsible for freeing the allocated memory.
 *
 * @param decoder The DER decoder instance.
 * @param out_str Pointer to a pointer that will receive the allocated null-terminated string.
 * @param out_str_len Pointer to a `size_t` that will receive the length of the string (excluding null terminator).
 * @return 0 on success, -1 on failure (e.g., not a printable string, memory allocation error).
 */
int8_t der_decoder_decode_printable_string(der_decoder_t* decoder, char_t** out_str, size_t* out_str_len);

/**
 * @brief Decodes a DER boolean.
 *
 * Attempts to parse the next element as a DER boolean and stores the value in `out_value`.
 *
 * @param decoder The DER decoder instance.
 * @param out_value Pointer to a `boolean_t` where the decoded boolean will be stored.
 * @return 0 on success, -1 on failure (e.g., not a boolean, invalid format).
 */
int8_t der_decoder_decode_boolean(der_decoder_t* decoder, boolean_t* out_value);

/**
 * @brief Decodes a DER integer using a 128-bit unsigned integer.
 *
 * Attempts to parse the next element as a DER integer and stores the value in `out_value`.
 * Supports up to 128-bit unsigned integers, handling potential sign extension bytes.
 *
 * @param decoder The DER decoder instance.
 * @param out_value Pointer to a `uint128_t` where the decoded integer will be stored.
 * @return 0 on success, -1 on failure (e.g., not an integer, invalid format, value out of range for `uint128_t`).
 */
int8_t der_decoder_decode_integer_u128(der_decoder_t* decoder, uint128_t* out_value);

/**
 * @brief Decodes a DER integer using a 160-bit unsigned integer.
 *
 * Attempts to parse the next element as a DER integer and stores the value in `out_value`.
 * Supports up to 160-bit unsigned integers (20 bytes), handling potential sign extension bytes.
 *
 * @param decoder The DER decoder instance.
 * @param out_value A 20-byte array where the decoded 160-bit integer will be stored in big-endian format.
 * @return 0 on success, -1 on failure (e.g., not an integer, invalid format, value out of range for 160 bits).
 */
int8_t der_decoder_decode_integer_u160(der_decoder_t* decoder, uint8_t out_value[20]);

/**
 * @brief Decodes a DER integer using a 256-bit unsigned integer.
 *
 * Attempts to parse the next element as a DER integer and stores the value in `out_value`.
 * Supports up to 256-bit unsigned integers (32 bytes), handling potential sign extension bytes.
 *
 * @param decoder The DER decoder instance.
 * @param out_value A 32-byte array where the decoded 256-bit integer will be stored in big-endian format.
 * @return 0 on success, -1 on failure (e.g., not an integer, invalid format, value out of range for 256 bits).
 */
int8_t der_decoder_decode_integer_u256(der_decoder_t* decoder, uint8_t out_value[32]);

/**
 * @brief Decodes a context-specific string.
 *
 * Attempts to parse the next element as a primitive context-specific type with the expected tag number.
 * If successful, it allocates memory for the data and stores a pointer to it in `out_data` and its length in `out_data_len`.
 * The caller is responsible for freeing the allocated memory.
 *
 * @param decoder The DER decoder instance.
 * @param expected_tag_number The expected context-specific tag number.
 * @param out_data Pointer to a pointer that will receive the allocated buffer containing the data.
 * @param out_data_len Pointer to a `size_t` that will receive the length of the data.
 * @return 0 on success, -1 on failure (e.g., tag mismatch, not a primitive type, memory allocation error).
 */
int8_t der_decoder_decode_context_specific_string(der_decoder_t* decoder, uint8_t expected_tag_number, uint8_t** out_data, size_t* out_data_len);

/**
 * @brief Decodes a DER UTC time.
 *
 * Attempts to parse the next element as a DER UTCTime string and converts it to a `time_t` value.
 *
 * @param decoder The DER decoder instance.
 * @param out_time_value Pointer to a `time_t` where the decoded time value will be stored.
 * @return 0 on success, -1 on failure (e.g., not a UTC time, invalid format, time parsing error).
 */
int8_t der_decoder_decode_utc_time(der_decoder_t* decoder, time_t* out_time_value);

/**
 * @brief Checks if the next element has a specific DER tag.
 *
 * This function peeks at the next element without consuming it, checking if its tag matches the expected class, type, and number.
 *
 * @param decoder The DER decoder instance.
 * @param expected_tag_class The expected tag class.
 * @param expected_tag_type The expected tag type (primitive or constructed).
 * @param expected_tag_number The expected tag number.
 * @return `true` if the next element has the specified tag, `false` otherwise or on error.
 */
boolean_t der_decoder_has_tag(der_decoder_t* decoder, der_tag_class_t expected_tag_class, der_tag_type_t expected_tag_type, uint8_t expected_tag_number);

/**
 * @brief Macro to check if the next element is a DER boolean tag.
 *
 * A convenience macro for calling `der_decoder_has_tag` with the appropriate parameters for a universal primitive boolean.
 *
 * @param decoder The DER decoder instance.
 * @return `true` if the next element is a boolean, `false` otherwise.
 */
#define der_decoder_has_boolean_tag(decoder) der_decoder_has_tag(decoder, DER_TAG_CLASS_UNIVERSAL, DER_TAG_TYPE_PRIMITIVE, DER_TAG_NUMBER_BOOLEAN)

/**
 * @brief Checks if the next element has a specific explicit tag.
 *
 * This function peeks at the next element without consuming it, checking if its tag matches the expected
 * constructed tag with the specified class and number.
 *
 * @param decoder The DER decoder instance.
 * @param expected_tag_class The expected tag class.
 * @param expected_tag_number The expected tag number.
 * @return `true` if the next element has the specified explicit tag, `false` otherwise or on error.
 */
boolean_t der_decoder_has_explicit_tag(der_decoder_t* decoder, der_tag_class_t expected_tag_class, uint8_t expected_tag_number);

/**
 * @brief Checks if the next element has a specific context-specific tag.
 *
 * This function peeks at the next element without consuming it, checking if its tag matches the expected
 * primitive context-specific tag with the specified number.
 *
 * @param decoder The DER decoder instance.
 * @param expected_tag_number The expected context-specific tag number.
 * @return `true` if the next element has the specified context-specific tag, `false` otherwise or on error.
 */
boolean_t der_decoder_has_context_specific_tag(der_decoder_t* decoder, uint8_t expected_tag_number);

/**
 * @brief Checks if the decoder has finished processing the current container (sequence, set, etc.).
 *
 * This is useful for iterating through elements within a container.
 *
 * @param decoder The DER decoder instance.
 * @return `true` if the current position is at or beyond the end of the current container, `false` otherwise.
 */
boolean_t der_decoder_has_container_ended(der_decoder_t* decoder);

/**
 * @brief Retrieves the current position (offset) of the decoder within the data buffer.
 *
 * @param decoder The DER decoder instance.
 * @param out_position Pointer to a `size_t` that will receive the current position.
 * @return 0 on success, -1 on failure (e.g., invalid decoder).
 */
int8_t der_decoder_get_current_position(der_decoder_t* decoder, size_t* out_position);


#ifdef __cplusplus
}
#endif

#endif // ___DER_H
