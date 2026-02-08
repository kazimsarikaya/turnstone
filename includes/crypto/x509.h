/**
 * @file x509.h
 * @brief X.509 Certificate Definitions
 *
 * This file defines the structures and functions for creating and manipulating X.509 certificates.
 * X.509 is a standard defining the format of public key certificates used in Transport Layer Security (TLS)
 * and other Public Key Infrastructure (PKI) systems.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___X509_H
#define ___X509_H 0

#include <types.h> // For basic types like uint32_t, boolean_t, char_t, uint8_t, size_t, int8_t, uint128_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Forward declaration for the X.509 certificate structure.
 */
typedef struct x509_certificate_t x509_certificate_t;

/**
 * @brief Enumeration for the type of X.509 extension.
 *
 * These types represent standard X.509 extensions that can be added to a certificate.
 */
typedef enum x509_extension_type_t {
    X509_EXTENSION_UNKNOWN, ///< Unknown extension type.
    X509_EXTENSION_BASIC_CONSTRAINTS, ///< Basic Constraints extension (e.g., CA flag, path length).
    X509_EXTENSION_KEY_USAGE, ///< Key Usage extension (defines permitted uses of the key).
    X509_EXTENSION_EXTENDED_KEY_USAGE, ///< Extended Key Usage extension (further specifies permitted uses).
    X509_EXTENSION_SUBJECT_ALTERNATIVE_NAME, ///< Subject Alternative Name extension (e.g., DNS names, IP addresses).
    X509_EXTENSION_SKID, ///< Subject Key Identifier extension.
    X509_EXTENSION_AKID, ///< Authority Key Identifier extension.
    X509_EXTENSION_NETSCAPE_CERT_TYPE, ///< Netscape Certificate Type extension.
    X509_EXTENSION_COUNT ///< Total number of extension types.
} x509_extension_type_t;

/**
 * @brief Enumeration for the Basic Constraints extension.
 *
 * Specifies whether the certificate is a Certificate Authority (CA) or an end-entity certificate.
 */
typedef enum x509_basic_constraints_t {
    X509_BASIC_CONSTRAINTS_UNKNOWN = 0, ///< Unknown constraint.
    X509_BASIC_CONSTRAINTS_CA      = 1, ///< The certificate is a Certificate Authority (CA).
    X509_BASIC_CONSTRAINTS_END_ENTITY = 2, ///< The certificate is an end-entity certificate.
} x509_basic_constraints_t;

/**
 * @brief Enumeration for the Key Usage extension.
 *
 * Defines the permitted uses of the public key contained in the certificate.
 * Multiple flags can be combined using the bitwise OR operator.
 */
typedef enum x509_key_usage_t {
    X509_KEY_USAGE_UNKNOWN           = 0x00, ///< Unknown key usage.
    X509_KEY_USAGE_DIGITAL_SIGNATURE = 0x80, ///< The public key may be used to verify digital signatures.
    X509_KEY_USAGE_NON_REPUDIATION   = 0x40, ///< The public key may be used for non-repudiation.
    X509_KEY_USAGE_KEY_ENCIPHERMENT  = 0x20, ///< The public key may be used to encrypt keys.
    X509_KEY_USAGE_DATA_ENCIPHERMENT = 0x10, ///< The public key may be used to encrypt data.
    X509_KEY_USAGE_KEY_AGREEMENT     = 0x08, ///< The public key may be used in key agreement protocols.
    X509_KEY_USAGE_KEY_CERT_SIGN     = 0x04, ///< The public key may be used to sign certificate requests.
    X509_KEY_USAGE_CRL_SIGN          = 0x02, ///< The public key may be used to sign Certificate Revocation Lists (CRLs).
    X509_KEY_USAGE_ENCIPHER_ONLY     = 0x01, ///< The public key may be used only to encrypt keys.
} x509_key_usage_t;

/**
 * @brief Enumeration for the Netscape Certificate Type extension.
 *
 * Specifies the type of Netscape certificate.
 * Multiple flags can be combined using the bitwise OR operator.
 */
typedef enum x509_netscape_cert_type_t {
    X509_NETSCAPE_CERT_TYPE_UNKNOWN        = 0x00, ///< Unknown Netscape certificate type.
    X509_NETSCAPE_CERT_TYPE_SSL_CLIENT     = 0x01, ///< SSL Client certificate.
    X509_NETSCAPE_CERT_TYPE_SSL_SERVER     = 0x02, ///< SSL Server certificate.
    X509_NETSCAPE_CERT_TYPE_SMIME          = 0x04, ///< S/MIME certificate.
    X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING = 0x08, ///< Object Signing certificate.
    X509_NETSCAPE_CERT_TYPE_RESERVED       = 0x10, ///< Reserved for future use.
    X509_NETSCAPE_CERT_TYPE_SSL_CA         = 0x20, ///< SSL CA certificate.
    X509_NETSCAPE_CERT_TYPE_SMIME_CA       = 0x40, ///< S/MIME CA certificate.
    X509_NETSCAPE_CERT_TYPE_OBJECT_SIGNING_CA = 0x80, ///< Object Signing CA certificate.
} x509_netscape_cert_type_t;

/**
 * @brief Enumeration for the Extended Key Usage extension.
 *
 * Provides a more specific indication of the intended use of the public key.
 * Multiple flags can be combined using the bitwise OR operator.
 */
typedef enum x509_extended_key_usage_t {
    X509_EXTENDED_KEY_USAGE_UNKNOWN           = 0x00, ///< Unknown extended key usage.
    X509_EXTENDED_KEY_USAGE_SERVER_AUTH       = 0x01, ///< The public key is intended for server authentication (e.g., TLS/SSL).
    X509_EXTENDED_KEY_USAGE_CLIENT_AUTH       = 0x02, ///< The public key is intended for client authentication (e.g., TLS/SSL).
    X509_EXTENDED_KEY_USAGE_CODE_SIGNING      = 0x04, ///< The public key is intended for signing code.
    X509_EXTENDED_KEY_USAGE_EMAIL_PROTECTION  = 0x08, ///< The public key is intended for email protection (e.g., S/MIME).
    X509_EXTENDED_KEY_USAGE_TIME_STAMPING     = 0x10, ///< The public key is intended for time stamping.
    X509_EXTENDED_KEY_USAGE_OCSP_SIGNING      = 0x20, ///< The public key is intended for signing OCSP responses.
} x509_extended_key_usage_t;

/**
 * @brief Enumeration for the type of Subject Alternative Name.
 *
 * Specifies the type of identifier used in the Subject Alternative Name extension.
 */
typedef enum x509_subject_alternative_name_type_t {
    X509_SUBJECT_ALTERNATIVE_NAME_TYPE_UNKNOWN, ///< Unknown name type.
    X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS, ///< Domain Name System (DNS) name.
    X509_SUBJECT_ALTERNATIVE_NAME_TYPE_IP, ///< IP address.
    X509_SUBJECT_ALTERNATIVE_NAME_TYPE_EMAIL, ///< Email address.
} x509_subject_alternative_name_type_t;

/**
 * @brief Enumeration for the signature algorithm used to sign the certificate.
 */
typedef enum x509_algorithm_t {
    X509_ALGORITHM_UNKNOWN, ///< Unknown signature algorithm.
    X509_ALGORITHM_ED25519, ///< Ed25519 signature algorithm.
    X509_ALGORITHM_X25519, ///< X25519 key agreement algorithm.
    X509_ALGORITHM_ECDSA_WITH_SHA256, ///< ECDSA with SHA-256 signature algorithm.
    X509_ALGORITHM_ECDSA_SECP256R1, ///< ECDSA with secp256r1 curve signature algorithm.
} x509_algorithm_t;

typedef enum x509_issuer_subject_field_t {
    X509_ISSUER_SUBJECT_FIELD_UNKNOWN,
    X509_ISSUER_SUBJECT_FIELD_ORGANIZATION,
    X509_ISSUER_SUBJECT_FIELD_ORGANIZATIONAL_UNIT,
    X509_ISSUER_SUBJECT_FIELD_COUNTRY,
    X509_ISSUER_SUBJECT_FIELD_COMMON_NAME,
    X509_ISSUER_SUBJECT_FIELD_COUNT,
} x509_issuer_subject_field_t;

/**
 * @brief Creates a new, empty X.509 certificate structure.
 *
 * @return A pointer to the newly created `x509_certificate_t` structure, or NULL if memory allocation fails.
 */
x509_certificate_t* x509_certificate_new(void);

/**
 * @brief Frees all memory associated with an X.509 certificate structure.
 *
 * @param cert Pointer to the `x509_certificate_t` structure to be freed.
 */
void x509_certificate_free(x509_certificate_t* cert);

/**
 * @brief Adds a field to the issuer information of the certificate.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param field The field to add (e.g., organization, common name).
 * @param value The value of the field as a string.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_issuer_field(x509_certificate_t* cert, x509_issuer_subject_field_t field, const char_t* value);

/**
 * @brief Retrieves a field from the issuer information of the certificate.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param field The field to retrieve (e.g., organization, common name).
 * @param out_value Output parameter that will point to the value of the field if found. The caller is responsible for freeing this memory.
 * @return 0 on success, -1 on failure (e.g., invalid input, field not found).
 */
int8_t x509_certificate_get_issuer_field(const x509_certificate_t* cert, x509_issuer_subject_field_t field, char_t** out_value);

/**
 * @brief Adds a field to the subject information of the certificate.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param field The field to add (e.g., organization, common name).
 * @param value The value of the field as a string.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_subject_field(x509_certificate_t* cert, x509_issuer_subject_field_t field, const char_t* value);

/**
 * @brief Retrieves a field from the issuer information of the certificate.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param field The field to retrieve (e.g., organization, common name).
 * @param out_value Output parameter that will point to the value of the field if found. The caller is responsible for freeing this memory.
 * @return 0 on success, -1 on failure (e.g., invalid input, field not found).
 */
int8_t x509_certificate_get_subject_field(const x509_certificate_t* cert, x509_issuer_subject_field_t field, char_t** out_value);

/**
 * @brief Sets the validity period of the certificate.
 *
 * Calculates the `not_before` and `not_after` dates based on the current time and the number of days specified.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param days_valid The number of days the certificate should be valid for.
 * @return 0 on success, -1 on failure (e.g., invalid input).
 */
int8_t x509_certificate_add_duration(x509_certificate_t* cert, uint32_t days_valid);

/**
 * @brief Sets the Basic Constraints extension for the certificate.
 *
 * This function is used to indicate whether the certificate is a Certificate Authority (CA)
 * or an end-entity certificate, and optionally set the path length constraint.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param is_ca Boolean indicating if this is a CA certificate (`true`) or an end-entity certificate (`false`).
 * @param path_len The maximum number of non-self-issued intermediate certificates that may follow this certificate in a valid certification path. A value of -1 indicates no limit.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_set_is_ca(x509_certificate_t* cert, boolean_t is_ca, int32_t path_len);

/**
 * @brief Adds the Key Usage extension to the certificate.
 *
 * Specifies the permitted uses of the public key. Multiple usages can be combined using bitwise OR.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param key_usage A bitmask of `x509_key_usage_t` values specifying the allowed key usages.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_key_usage(x509_certificate_t* cert, x509_key_usage_t key_usage);

/**
 * @brief Adds the Extended Key Usage extension to the certificate.
 *
 * Provides a more specific indication of the intended use of the public key. Multiple usages can be combined using bitwise OR.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param eku The extended key usage to add.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_extended_key_usage(x509_certificate_t* cert, x509_extended_key_usage_t eku);

/**
 * @brief Adds a Subject Alternative Name (SAN) entry to the certificate.
 *
 * This extension allows specifying additional identities for the subject, such as DNS names or IP addresses.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param type The type of the alternative name (e.g., `X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS`).
 * @param value The string representation of the alternative name.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_subject_alternative_name(x509_certificate_t* cert, x509_subject_alternative_name_type_t type, const char_t* value);

/**
 * @brief Adds the Subject Key Identifier (SKID) extension to the certificate.
 *
 * The SKID is a unique identifier for the public key contained in the certificate.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param skid Pointer to the raw SKID bytes.
 * @param skid_length The length of the SKID in bytes.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_subject_key_identifier(x509_certificate_t* cert, const uint8_t* skid, size_t skid_length);

/**
 * @brief Adds the Authority Key Identifier (AKID) extension to the certificate.
 *
 * The AKID is used to identify the public key corresponding to the private key used to sign the certificate.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param akid Pointer to the raw AKID bytes.
 * @param akid_length The length of the AKID in bytes.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_authority_key_identifier(x509_certificate_t* cert, const uint8_t* akid, size_t akid_length);

/**
 * @brief Adds the public key information to the certificate.
 *
 * This includes the algorithm used and the raw public key bytes.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param algorithm The public key algorithm (e.g., `X509_PUBLIC_KEY_ALGORITHM_ED25519`).
 * @param public_key Pointer to the raw public key bytes.
 * @param public_key_length The length of the public key in bytes.
 * @return 0 on success, -1 on failure (e.g., invalid input, memory allocation error).
 */
int8_t x509_certificate_add_public_key(x509_certificate_t* cert,
                                       x509_algorithm_t    algorithm,
                                       const uint8_t*      public_key,
                                       size_t              public_key_length);

/**
 * @brief Signs the certificate using the provided private key.
 *
 * This function first encodes the "To Be Signed" (TBS) part of the certificate and then signs it
 * using the specified algorithm and private key.
 *
 * @param cert Pointer to the `x509_certificate_t` structure. The TBS data will be generated and stored internally.
 * @param algorithm The signature algorithm to use (e.g., `X509_SIGNATURE_ALGORITHM_ED25519`).
 * @param private_key Pointer to the raw private key bytes.
 * @param private_key_length The length of the private key in bytes.
 * @return 0 on success, -1 on failure (e.g., invalid input, TBS encoding failed, signing failed).
 */
int8_t x509_certificate_sign(x509_certificate_t* cert,
                             x509_algorithm_t    algorithm,
                             const uint8_t*      private_key,
                             size_t              private_key_length);

/**
 * @brief Verifies the certificate's signature using the provided public key.
 *
 * This function checks that the signature on the certificate matches the TBS data when verified
 * with the given public key of the issuer.
 * @param cert Pointer to the `x509_certificate_t` structure. Requires TBS data and signature to be present.
 * @param public_key Pointer to the raw public key bytes.
 * @param public_key_length The length of the public key in bytes.
 * @param rebuild Boolean flag indicating whether to rebuild the TBS data after first verification, then re-verify.
 * @return 0 if the signature is valid, -1 if invalid or on failure (e.g., invalid input, verification failed).
 */
int8_t x509_certificate_verify_signature_with_rebuild(x509_certificate_t* cert,
                                                      const uint8_t*      public_key,
                                                      size_t              public_key_length,
                                                      boolean_t           rebuild);

/**
 * @brief Verifies the certificate's signature using the provided public key.
 *
 * This function checks that the signature on the certificate matches the TBS data when verified
 * with the given public key of the issuer.
 * It always rebuild the TBS data for double-checking.
 * @param cert Pointer to the `x509_certificate_t` structure. Requires TBS data and signature to be present.
 * @param public_key Pointer to the raw public key bytes.
 * @param public_key_length The length of the public key in bytes.
 * @return 0 if the signature is valid, -1 if invalid or on failure (e.g., invalid input, verification failed).
 */
#define x509_certificate_verify_signature(cert, public_key, public_key_length) \
        x509_certificate_verify_signature_with_rebuild((cert), (public_key), (public_key_length), true)

/**
 * @brief Assembles the final DER-encoded certificate structure.
 *
 * This function takes the internally generated TBS data and the signature, and combines them
 * into the complete X.509 certificate format. This should typically be called after `x509_certificate_sign`.
 *
 * @param cert Pointer to the `x509_certificate_t` structure. Requires TBS data and signature to be present.
 * @return 0 on success, -1 on failure (e.g., invalid certificate state, assembly failed).
 */
int8_t x509_certificate_assemble(x509_certificate_t* cert);

/**
 * @brief Retrieves the DER-encoded representation of the certificate.
 *
 * If the certificate has not yet been assembled, this function will call `x509_certificate_assemble` internally.
 * The caller is responsible for freeing the returned buffer using `memory_free`.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param out_length Pointer to a `size_t` variable that will receive the length of the DER-encoded data.
 * @return A pointer to a newly allocated buffer containing the DER-encoded certificate, or NULL on failure.
 */
uint8_t* x509_certificate_get_der(x509_certificate_t* cert, size_t* out_length);

/**
 * @brief Retrieves the PEM-encoded representation of the certificate.
 *
 * If the certificate has not yet been assembled, this function will call `x509_certificate_assemble` internally.
 * The caller is responsible for freeing the returned string using `memory_free`.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @return A pointer to a newly allocated null-terminated string containing the PEM-encoded certificate, or NULL on failure.
 */
char_t* x509_certificate_get_pem(x509_certificate_t* cert);

/**
 * @brief Creates an X.509 certificate structure from DER-encoded data.
 *
 * This function parses the provided DER data and populates a new `x509_certificate_t` structure.
 * The caller is responsible for freeing the returned structure using `x509_certificate_free`.
 *
 * @param der_data Pointer to the DER-encoded certificate data.
 * @param der_length The length of the DER-encoded data in bytes.
 * @return A pointer to the newly created `x509_certificate_t` structure, or NULL on failure.
 */
x509_certificate_t* x509_certificate_from_der(const uint8_t* der_data, size_t der_length);

/**
 * @brief Creates an X.509 certificate structure from PEM-encoded data.
 *
 * This function decodes the provided PEM string to extract the DER data,
 * then parses it to populate a new `x509_certificate_t` structure.
 * The caller is responsible for freeing the returned structure using `x509_certificate_free`.
 *
 * @param pem_data Pointer to the PEM-encoded certificate string.
 * @return A pointer to the newly created `x509_certificate_t` structure, or NULL on failure.
 */
x509_certificate_t* x509_certificate_from_pem(const char_t* pem_data);

/**
 * @brief Retrieves the "To Be Signed" (TBS) data of the certificate.
 *
 * This function returns the DER-encoded TBS portion of the certificate.
 * If `rebuild` is set to true, it will re-encode the TBS data before returning it.
 * The caller is responsible for freeing the returned buffer using `memory_free`.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param rebuild Boolean flag indicating whether to rebuild the TBS data before retrieval.
 * @param out_length Pointer to a `size_t` variable that will receive the length of the TBS data.
 * @return A pointer to a newly allocated buffer containing the TBS data, or NULL on failure.
 */
uint8_t* x509_certificate_get_tbs_data(x509_certificate_t* cert, boolean_t rebuild, size_t* out_length);

/**
 * @brief Retrieves the public key data from the certificate.
 *
 * This function returns the raw public key bytes contained in the certificate.
 * The caller is responsible for freeing the returned buffer using `memory_free`.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @param out_length Pointer to a `size_t` variable that will receive the length of the public key data.
 * @return A pointer to a newly allocated buffer containing the public key data, or NULL on failure.
 */
uint8_t* x509_certificate_get_public_key_data(x509_certificate_t* cert, size_t* out_length);

/**
 * @brief Retrieves the public key algorithm used in the certificate.
 *
 * This function returns the algorithm identifier for the public key contained in the certificate.
 *
 * @param cert Pointer to the `x509_certificate_t` structure.
 * @return The `x509_algorithm_t` value representing the public key algorithm, or `X509_ALGORITHM_UNKNOWN` on failure.
 */
x509_algorithm_t x509_certificate_get_public_key_algorithm(x509_certificate_t* cert);


#ifdef __cplusplus
}
#endif

#endif // ___X509_H
