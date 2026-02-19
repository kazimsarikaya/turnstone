/**
 * @file tlsserver.c
 * @brief tls server test application.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000
#include "setup.h"
#include <bigint.h>
#include <crypto/gcm.h>
#include <crypto/sha2.h>
#include <crypto/x25519.h>
#include <crypto/der.h>
#include <crypto/pem.h>
#include <crypto/x509.h>
#include <crypto/tls13.h>
#include <crypto/ellipticcurve.h>
#include <crypto/mlkem768.h>
#include <crypto/keccak.h>
#include <errno.h>
#include <base64.h>
#include <pipeline.h>
#include <cpu/sync.h>
#include <network/http.h>

#define PORT 10443

static int8_t tls13_load_ca_certificate_and_key(boolean_t force_regenerate, boolean_t use_secp256r1) {
    // first check build/ca.pem and build/ca.key exists
    // if exists load them else generate new CA certificate and key
    boolean_t ca_exists = false;
    FILE* f = fopen("build/ca.pem", "rb");
    if(f) {
        ca_exists = true;
        fclose(f);
    }

    f = fopen("build/ca.key", "rb");

    if(f) {
        ca_exists = ca_exists && true;
        fclose(f);
    } else {
        ca_exists = false;
    }

    if(ca_exists && !force_regenerate) {
        return 0;
    }

    // generate new CA certificate and KEY    x509_certificate_t* cert = x509_certificate_new();
    x509_certificate_t* cert = x509_certificate_new();
    if (cert == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create new X509 certificate");
        return -1;
    }

    if (x509_certificate_add_issuer_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "TurnstoneOS CA") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add issuer common name");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "TurnstoneOS CA") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject common name");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_duration(cert, 365) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add certificate duration");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_set_is_ca(cert, true, -1) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set certificate as CA");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_key_usage(cert, X509_KEY_USAGE_KEY_CERT_SIGN | X509_KEY_USAGE_CRL_SIGN) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add key usage");
        x509_certificate_free(cert);
        return -1;
    }

    if(!use_secp256r1) {
        uint8_t private_key[32];
        uint8_t public_key[32];

        if(ed25519_generate_keypair(private_key, public_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate X25519 keypair");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ED25519, public_key, 32) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to certificate");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_sign(cert, NULL, X509_ALGORITHM_ED25519,
                                  private_key, sizeof(private_key)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign certificate");
            x509_certificate_free(cert);
            return -1;
        }

        char_t* final_key_data = NULL;
        if(pem_write_ed25519_private_key(private_key, &final_key_data) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to write private key to PEM format");
            return -1;
        }

        memory_memclean(private_key, sizeof(private_key));

        f = fopen("build/ca.key", "wb");
        if (f == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open ca.key for writing");
            memory_free(final_key_data);
            return -1;
        }

        fwrite(final_key_data, 1, strlen(final_key_data), f);
        fclose(f);

        memory_free(final_key_data);
    } else {
        uint8_t private_key[32];
        uint8_t public_key[65];

        if(ellipticcurve_secp256r1_generate_keypair(private_key, public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate SECP256R1 keypair");
            x509_certificate_free(cert);
            return -1;
        }

        public_key[0] = 0x04; // Uncompressed point prefix

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ECDSA_SECP256R1, public_key, 65) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to certificate");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_sign(cert, NULL, X509_ALGORITHM_ECDSA_SECP256R1,
                                  private_key, sizeof(private_key)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign certificate");
            x509_certificate_free(cert);
            return -1;
        }

        char_t* final_key_data = NULL;
        if(pem_write_secp256r1_private_key(private_key, &final_key_data) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to write private key to PEM format");
            return -1;
        }

        memory_memclean(private_key, sizeof(private_key));

        f = fopen("build/ca.key", "wb");
        if (f == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open ca.key for writing");
            memory_free(final_key_data);
            return -1;
        }

        fwrite(final_key_data, 1, strlen(final_key_data), f);
        fclose(f);

        memory_free(final_key_data);
    }

    char_t* final_cert_data = x509_certificate_get_pem(cert);
    if (final_cert_data == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get final certificate data");
        x509_certificate_free(cert);
        return -1;
    }

    x509_certificate_free(cert);

    f = fopen("build/ca.pem", "wb");
    if (f == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open certificate.der for writing");
        memory_free(final_cert_data);
        return -1;
    }

    fwrite(final_cert_data, 1, strlen(final_cert_data), f);
    fclose(f);

    memory_free(final_cert_data);

    print_success("CA certificate generated successfully: ca.pem ca.key");

    return 0;
}

static int8_t tls13_load_server_certificate_and_key(tls13_context_t*     tls13_ctx,
                                                    x509_certificate_t** out_server_cert,
                                                    uint8_t**            out_private_key,
                                                    size_t*              out_private_key_len) {
    FILE* f;

    f = fopen("build/ca.pem", "rb");
    if(!f) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open CA certificate file");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long ca_cert_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* ca_cert_data = memory_malloc(ca_cert_size);
    if(!ca_cert_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA certificate");
        fclose(f);
        return -1;
    }
    fread(ca_cert_data, 1, ca_cert_size, f);
    fclose(f);

    x509_certificate_t* ca_certificate = x509_certificate_from_pem((char_t*)ca_cert_data);
    memory_free(ca_cert_data);

    if(!ca_certificate) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to parse CA certificate from PEM");
        return -1;
    }

    if(tls13_set_ca_certificate(tls13_ctx, ca_certificate) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set CA certificate in TLS context");
        x509_certificate_free(ca_certificate);
        return -1;
    }

    f = fopen("build/ca.key", "rb");
    if(!f) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to open CA private key file");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long ca_key_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* ca_key_data = memory_malloc(ca_key_size);
    if(!ca_key_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
        fclose(f);
        return -1;
    }
    fread(ca_key_data, 1, ca_key_size, f);
    fclose(f);

    uint8_t ca_private_key[32]; // both ed25519 and secp256r1 use 32 byte private keys
    size_t ca_private_key_len = 0;
    uint8_t ca_public_key[65]; // max public key size for ed25519 is 32 bytes, for secp256r1 is 65 bytes (uncompressed)
    size_t ca_public_key_len = 0;

    x509_algorithm_t ca_key_algorithm = x509_certificate_get_public_key_algorithm(ca_certificate);
    if (ca_key_algorithm == X509_ALGORITHM_ED25519) {
        ca_private_key_len = ED25519_PRIVATE_KEY_RAW_LEN;

        if(pem_read_ed25519_private_key((char_t*)ca_key_data, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read CA private key from PEM");
            memory_free(ca_key_data);
            return -1;
        }
        memory_free(ca_key_data);

        if(ed25519_derive_pubkey(ca_public_key, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive CA public key from private key");
            return -1;
        }

        ca_public_key_len = ED25519_PUBLIC_KEY_RAW_LEN;

        if(x509_certificate_verify_signature(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to verify CA certificate signature with rebuild, trying without rebuild");
            if(x509_certificate_verify_signature_with_rebuild(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len, false) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to verify CA certificate signature");
                return -1;
            }
        }
    }else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP256R1) {
        ca_private_key_len = ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN;
        if(pem_read_secp256r1_private_key((char_t*)ca_key_data, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read CA private key from PEM");
            memory_free(ca_key_data);
            return -1;
        }
        memory_free(ca_key_data);

        if(ellipticcurve_secp256r1_derive_pubkey(ca_public_key + 1, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive CA public key from private key");
            return -1;
        }

        ca_public_key[0] = 0x04; // uncompressed point prefix

        ca_public_key_len = ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1;

        if(x509_certificate_verify_signature(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to verify CA certificate signature with rebuild, trying without rebuild");
            if(x509_certificate_verify_signature_with_rebuild(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len, false) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to verify CA certificate signature");
                return -1;
            }
        }
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported CA public key algorithm: %d", ca_key_algorithm);
        return -1;
    }

    char_t* ca_subjectfields[X509_ISSUER_SUBJECT_FIELD_COUNT] = {0};

    for(int32_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
        if(x509_certificate_get_subject_field(ca_certificate, i, &ca_subjectfields[i]) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get CA certificate subject field: %d", i);
            return -1;
        }
    }

    // now generate server certificate signed by CA
    x509_certificate_t* cert = x509_certificate_new();
    if (cert == NULL) {
        for(int32_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
            memory_free(ca_subjectfields[i]);
        }

        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create new X509 certificate");
        return -1;
    }

    if (x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "Test Server") != 0) {
        for(int32_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
            memory_free(ca_subjectfields[i]);
        }


        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject common name");
        x509_certificate_free(cert);
        return -1;
    }

    for(int32_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
        if(ca_subjectfields[i]) {
            if (x509_certificate_add_issuer_field(cert, i, ca_subjectfields[i]) != 0) {
                for(int32_t j = 0; j < X509_ISSUER_SUBJECT_FIELD_COUNT; j++) {
                    memory_free(ca_subjectfields[j]);
                }
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add issuer field: %d", i);
                x509_certificate_free(cert);
                return -1;
            }
        }
    }

    for(int32_t i = 0; i < X509_ISSUER_SUBJECT_FIELD_COUNT; i++) {
        if(ca_subjectfields[i]) {
            memory_free(ca_subjectfields[i]);
        }
    }

    if (x509_certificate_add_duration(cert, 365) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add certificate duration");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_set_is_ca(cert, false, -1) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set certificate as non-CA");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_key_usage(cert, X509_KEY_USAGE_DIGITAL_SIGNATURE) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add key usage");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_extended_key_usage(cert, X509_EXTENDED_KEY_USAGE_SERVER_AUTH) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add extended key usage");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_alternative_name(cert, X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS, "localhost") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject alternative name");
        x509_certificate_free(cert);
        return -1;
    }

    if(x509_certificate_add_subject_alternative_name(cert, X509_SUBJECT_ALTERNATIVE_NAME_TYPE_IP, "127.0.0.1") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject alternative name IP");
        x509_certificate_free(cert);
        return -1;
    }

    uint8_t* server_private_key = NULL;
    size_t server_private_key_len = 0;

    if(ca_key_algorithm == X509_ALGORITHM_ED25519) {
        server_private_key_len = ED25519_PRIVATE_KEY_RAW_LEN;
        server_private_key = memory_malloc(ED25519_PRIVATE_KEY_RAW_LEN);
        if(!server_private_key) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for server private key");
            x509_certificate_free(cert);
            return -1;
        }

        uint8_t server_public_key[32];

        if(ed25519_generate_keypair(server_private_key, server_public_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server X25519 keypair");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ED25519, server_public_key, ED25519_PUBLIC_KEY_RAW_LEN) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }

        if (x509_certificate_sign(cert, ca_certificate, ca_key_algorithm,
                                  ca_private_key, ca_private_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
    } else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP256R1) {
        server_private_key_len = ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN;
        server_private_key = memory_malloc(ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN);

        if(!server_private_key) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for server private key");
            x509_certificate_free(cert);
            return -1;
        }

        uint8_t server_public_key[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1]; // +1 for uncompressed point prefix

        if(ellipticcurve_secp256r1_generate_keypair(server_private_key, server_public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server ECDSA SECP256R1 keypair");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }

        server_public_key[0] = 0x04; // uncompressed point prefix

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ECDSA_SECP256R1, server_public_key, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }

        if (x509_certificate_sign(cert, ca_certificate, ca_key_algorithm,
                                  ca_private_key, ca_private_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported CA public key algorithm: %d", ca_key_algorithm);
        x509_certificate_free(cert);
        return -1;
    }

    memory_memclean(ca_private_key, ca_private_key_len);

    *out_server_cert = cert;
    *out_private_key = server_private_key;
    *out_private_key_len = server_private_key_len;

    PRINTLOG(CRYPTOLIB, LOG_INFO, "Server certificate and key loaded successfully");

    return 0;
}

static int32_t recv_all(int64_t sockfd, uint8_t* buffer, int32_t length, int32_t flags) {
    int32_t total_received = 0;
    boolean_t once = flags & 0x80000000; // custom flag to indicate recv should be called only once
    while (total_received < length) {
        int32_t bytes_received = recv(sockfd, buffer + total_received, length - total_received, flags);
        if (bytes_received <= 0) {
            return -1; // error or connection closed
        }
        total_received += bytes_received;
        if(once) {
            break;
        }
    }
    return total_received;
}

static int32_t send_all(int64_t sockfd, const uint8_t* buffer, int32_t length, int32_t flags) {
    int32_t total_sent = 0;
    while (total_sent < length) {
        int32_t bytes_sent = send(sockfd, buffer + total_sent, length - total_sent, flags);
        if (bytes_sent <= 0) {
            return -1; // error
        }
        total_sent += bytes_sent;
    }
    return total_sent;
}

int32_t main(int32_t argc, char_t** argv) {
    signal(SIGPIPE, SIG_IGN);

    boolean_t use_secp256r1 = false;
    boolean_t force_ca_regenerate = false;
    boolean_t require_client_certificate = false;

    // parse command line arguments
    for(int32_t i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--use-secp256r1") == 0) {
            use_secp256r1 = true;
        } else if(strcmp(argv[i], "--force-ca-regenerate") == 0) {
            force_ca_regenerate = true;
        } else if(strcmp(argv[i], "--require-client-cert") == 0) {
            require_client_certificate = true;
        }
    }

    gcm_initialize();

    PRINTLOG(CRYPTOLIB, LOG_INFO, "TLS server test application");

    if(tls13_load_ca_certificate_and_key(force_ca_regenerate, use_secp256r1) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load CA certificate and key");
        return 1;
    }

    int32_t server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int32_t opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "socket creation failed");
        return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "setsockopt SO_REUSEADDR failed");
        close(server_fd);
        return 1;
    }

    memory_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "bind failed. error code: %lli", errno);
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "listen failed");
        close(server_fd);
        return 1;
    }

    PRINTLOG(CRYPTOLIB, LOG_INFO, "Server listening on port %d (SO_REUSEADDR enabled)", PORT);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Waiting for connections...");

    uint8_t psk_encryption_key[AES256_KEY_SIZE];
    uint8_t psk_encryption_iv[12];
    uint8_t psk_aed_key[16];
    get_random_bytes(psk_encryption_key, sizeof(psk_encryption_key));
    get_random_bytes(psk_encryption_iv, sizeof(psk_encryption_iv));
    get_random_bytes(psk_aed_key, sizeof(psk_aed_key));

    int32_t request_count = 0;

    while (true && request_count < 10) {
        request_count++;
        // Accept incoming connection
        client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_len);
        if (client_fd == -1) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        PRINTLOG(CRYPTOLIB, LOG_INFO, "New connection from %s:%d", client_ip, ntohs(client_addr.sin_port));

        tls13_context_t* tls13_ctx = tls13_create_server_context(
            "localhost:10443",
            tls13_load_server_certificate_and_key,
            send_all,
            recv_all,
            client_fd,
            require_client_certificate,
            psk_encryption_key,
            psk_encryption_iv,
            psk_aed_key
            );

        if(!tls13_ctx) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed");
            close(client_fd);
            continue;
        }

        if(tls13_handle_handshake(tls13_ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "TLS handshake failed");
            tls13_destroy_context(tls13_ctx);
            close(client_fd);
            continue;
        }

        if(tls13_has_alpn_h2(tls13_ctx)) {
            if(http2_handle_connection(tls13_ctx) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/2 connection handling failed");
            }
        } else {
            if(http11_handle_connection(tls13_ctx) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/1.1 connection handling failed");
            }
        }

        if(tls13_send_close_notify(tls13_ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to send Close Notify");
        }

        tls13_destroy_context(tls13_ctx);

        close(client_fd);
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Connection closed");
    }

    close(server_fd);

    return 0;
}
