/**
 * @file tls13.h
 * @brief TLS 1.3 Protocol Definitions
 *
 * This file defines the structures, enumerations, and function prototypes necessary for implementing
 * the Transport Layer Security (TLS) version 1.3 protocol. It covers various aspects of the protocol,
 * including handshake messages, record layer, cryptographic primitives, and context management.
 *
 * The TLS 1.3 protocol aims to provide enhanced security, improved performance, and a simplified
 * handshake compared to previous versions. This header file serves as the primary interface for
 * interacting with the TLS 1.3 implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___TLS13_H
#define ___TLS13_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <crypto/x509.h>

/**
 * @brief Opaque structure representing the TLS 1.3 context.
 *
 * This structure holds all the state information for a TLS 1.3 connection, including
 * cryptographic keys, session state, random values, supported features, and network
 * communication callbacks. It is opaque to the user and managed internally by the TLS library.
 */
typedef struct tls13_context_t tls13_context_t;

/**
 * @brief Function pointer type for loading server certificates and private keys.
 *
 * This callback function is responsible for loading the server's X.509 certificate chain
 * and its corresponding private key. The implementation should populate the provided
 * pointers with the certificate data and private key material.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @param supported_algorithms Pointer to an array of `x509_algorithm_t` values representing the signature algorithms supported by the client. The implementation can use this information to select an appropriate certificate and key.
 * @param out_ca_cert Pointer to a pointer that will be set to the loaded CA certificate.
 * @param out_server_cert Pointer to a pointer that will be set to the loaded server certificate(s).
 * @param out_private_key Pointer to a pointer that will be set to the loaded server's private key data.
 * @param out_private_key_len Pointer to a size_t that will be set to the length of the private key data.
 * @return 0 on success, a negative value on failure.
 */
typedef int8_t (*tls13_load_server_certificate_and_key_f)(tls13_context_t*     ctx,
                                                          x509_algorithm_t*    supported_algorithms,
                                                          x509_certificate_t** out_ca_cert,
                                                          x509_certificate_t** out_server_cert,
                                                          uint8_t**            out_private_key,
                                                          size_t*              out_private_key_len);

/**
 * @brief Function pointer type for client certificate verification callback.
 *
 * This callback function is invoked during the TLS handshake when the server requests
 * a client certificate. The implementation should verify the provided client certificate
 * chain and return an appropriate status code.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @param certificate_chain Pointer to an array of X.509 certificates representing the client's certificate chain.
 * @param chain_length The number of certificates in the chain.
 * @return 0 if the client certificate is valid, a negative value if it is invalid or verification fails.
 */
typedef int8_t (*tls13_client_certificate_verify_callback_f)(tls13_context_t*     ctx,
                                                             x509_certificate_t** certificate_chain,
                                                             size_t               chain_length);

/**
 * @brief Function pointer type for providing a list of CA Distinguished Names (DNs) to the client.
 *
 * This callback function is called when the server needs to provide the client with a list of acceptable
 * Certificate Authorities (CAs) during the TLS handshake. The implementation should populate the provided
 * pointers with the list of CA DNs and their count.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @param out_ca_dn_list Pointer to a pointer that will be set to an array of byte arrays, where each byte array represents a CA DN in DER format.
 * @param out_ca_dn_list_length Pointer to a size_t that will be set to the length of the CA DN list.
 * @param out_ca_count Pointer to a size_t that will be set to the number of CA DNs in the list.
 * @return 0 on success, a negative value on failure.
 */
typedef int8_t (*tls13_client_certificates_ca_dn_list_callback_f)(tls13_context_t* ctx,
                                                                  uint8_t***       out_ca_dn_list,
                                                                  size_t**         out_ca_dn_list_length,
                                                                  size_t*          out_ca_count);

/**
 * @brief Function pointer type for retrieving PSK encryption keys.
 *
 * This callback function is used to obtain the necessary keys and IVs for PSK (Pre-Shared Key) encryption operations during the TLS handshake. The implementation should provide the appropriate keys based on whether the previous key is being used or not.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @param previos_key A boolean indicating whether to retrieve the previous key (true) or the current key (false).
 * @param out_psk_encryption_key Pointer to a buffer where the PSK encryption key will be stored.
 * @param out_psk_encryption_iv Pointer to a buffer where the PSK encryption IV will be stored.
 * @param out_psk_aed_key Pointer to a buffer where the PSK authentication encryption data key will be stored.
 * @return 0 on success, a negative value on failure.
 */
typedef int8_t (*tls13_get_psk_encryption_keys_callback_f)(tls13_context_t* ctx,
                                                           boolean_t        previos_key,
                                                           uint8_t**        out_psk_encryption_key,
                                                           uint8_t**        out_psk_encryption_iv,
                                                           uint8_t**        out_psk_aed_key);


/**
 * @brief Function pointer type for sending data over the network.
 *
 * This callback function is used by the TLS library to send data to the remote peer.
 *
 * @param network_client_identifier An identifier for the network connection.
 * @param buf Pointer to the data buffer to be sent.
 * @param len The number of bytes to send.
 * @param flags Network-specific flags (e.g., MSG_DONTWAIT).
 * @return The number of bytes sent, or a negative value on error.
 */
typedef int32_t (*tls13_network_send_f)(int64_t network_client_identifier, const uint8_t* buf, int32_t len, int32_t flags);

/**
 * @brief Function pointer type for receiving data from the network.
 *
 * This callback function is used by the TLS library to receive data from the remote peer.
 *
 * @param network_client_identifier An identifier for the network connection.
 * @param buf Pointer to the buffer where received data will be stored.
 * @param len The maximum number of bytes to receive.
 * @param flags Network-specific flags.
 * @return The number of bytes received, or a negative value on error.
 */
typedef int32_t (*tls13_network_recv_f)(int64_t network_client_identifier, uint8_t* buf, int32_t len, int32_t flags);


/**
 * @brief Creates a new TLS 1.3 server context.
 *
 * Initializes a TLS 1.3 context for a server. This function sets up the necessary state
 * for handling TLS connections, including network callbacks, certificate loading functions,
 * and security parameters.
 *
 * @param host_port The hostname and port string (e.g., "example.com:443") the server is listening on. Used for SNI and logging.
 * @param load_server_certificate_and_key A function pointer to load the server's certificate and private key.
 * @param client_certificate_verify_callback A function pointer for verifying client certificates during the handshake.
 * @param client_certificates_ca_dn_list_callback A function pointer for providing a list of acceptable CA DNs to the client.
 * @param get_psk_encryption_keys_callback A function pointer for retrieving PSK encryption keys during the handshake.
 * @param network_send A function pointer for sending data over the network.
 * @param network_recv A function pointer for receiving data from the network.
 * @param network_client_identifier An identifier for the specific network connection.
 * @param require_client_certificate If true, the server will request a client certificate during the handshake.
 * @return A pointer to the newly created tls13_context_t on success, or NULL on failure.
 */
tls13_context_t* tls13_create_server_context(const char_t*                                   host_port,
                                             tls13_load_server_certificate_and_key_f         load_server_certificate_and_key,
                                             tls13_client_certificate_verify_callback_f      client_certificate_verify_callback,
                                             tls13_client_certificates_ca_dn_list_callback_f client_certificates_ca_dn_list_callback,
                                             tls13_get_psk_encryption_keys_callback_f        get_psk_encryption_keys_callback,
                                             tls13_network_send_f                            network_send,
                                             tls13_network_recv_f                            network_recv,
                                             int64_t                                         network_client_identifier,
                                             boolean_t                                       require_client_certificate);
/**
 * @brief Destroys a TLS 1.3 context and frees associated resources.
 *
 * This function cleans up all memory and resources allocated for a TLS 1.3 context.
 * It should be called when the TLS connection is no longer needed.
 *
 * @param tls13_ctx Pointer to the TLS 1.3 context to destroy.
 */
void tls13_destroy_context(tls13_context_t* tls13_ctx);

/**
 * @brief Reads application data from the TLS connection.
 *
 * Attempts to read application data from the decrypted TLS record stream. This function
 * handles buffering of fragmented records and decryption of incoming data.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @param out_data Pointer to the buffer where the decrypted application data will be stored.
 * @param max_len The maximum number of bytes to read into out_data.
 * @return The number of bytes read on success, 0 if the connection is closed gracefully,
 *         or a negative value on error.
 */
int32_t tls13_read(tls13_context_t* ctx, uint8_t* out_data, uint32_t max_len);

/**
 * @brief Writes application data to the TLS connection.
 *
 * Encrypts and sends application data over the TLS connection. This function handles
 * record fragmentation and encryption.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @param data Pointer to the application data buffer to send.
 * @param len The number of bytes to send.
 * @return The number of bytes written on success, or a negative value on error.
 */
int32_t tls13_write(tls13_context_t* ctx, const uint8_t* data, uint32_t len);

/**
 * @brief Sends a TLS Close Notify alert.
 *
 * Gracefully closes the TLS connection by sending a "Close Notify" alert to the peer.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @return 0 on success, a negative value on failure.
 */
int8_t tls13_send_close_notify(tls13_context_t* ctx);

/**
 * @brief Handles the TLS 1.3 handshake process.
 *
 * This function orchestrates the server-side TLS 1.3 handshake, processing client messages
 * (like ClientHello) and sending corresponding server messages (like ServerHello, Certificate, etc.).
 * It should be called after the initial network connection is established and before application data is exchanged.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @return 0 on successful completion of the handshake, a negative value on failure.
 */
int8_t tls13_handle_handshake(tls13_context_t* ctx);

/**
 * @brief Checks if the client offered HTTP/2 (h2) via ALPN.
 *
 * @param ctx Pointer to the TLS 1.3 context.
 * @return true if the client offered h2 via ALPN, false otherwise.
 */
boolean_t tls13_has_alpn_h2(tls13_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* ___TLS13_H */
