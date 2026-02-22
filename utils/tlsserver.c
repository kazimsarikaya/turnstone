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

static x509_certificate_t* ca_certificate = NULL;

static int8_t tls13_load_ca_certificate(void) {
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

    ca_certificate = x509_certificate_from_pem((char_t*)ca_cert_data);
    memory_free(ca_cert_data);

    if(!ca_certificate) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to parse CA certificate from PEM");
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

    uint8_t* ca_private_key = NULL;
    size_t ca_private_key_len = 0;
    uint8_t* ca_public_key = NULL;
    size_t ca_public_key_len = 0;

    x509_algorithm_t ca_key_algorithm = x509_certificate_get_public_key_algorithm(ca_certificate);
    if (ca_key_algorithm == X509_ALGORITHM_ED25519) {
        ca_private_key_len = ED25519_PRIVATE_KEY_RAW_LEN;
        ca_public_key_len  = ED25519_PUBLIC_KEY_RAW_LEN;

        ca_private_key = memory_malloc(ca_private_key_len);

        if(ca_private_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
            memory_free(ca_key_data);
            return -1;
        }

        ca_public_key = memory_malloc(ca_public_key_len);

        if(ca_public_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA public key");
            memory_free(ca_private_key);
            memory_free(ca_key_data);
            return -1;
        }

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

        if(x509_certificate_verify_signature(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to verify CA certificate signature with rebuild, trying without rebuild");
            if(x509_certificate_verify_signature_with_rebuild(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len, false) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to verify CA certificate signature");
                memory_free(ca_public_key);
                return -1;
            }
        }

        memory_free(ca_public_key);
    } else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
        ca_private_key_len = ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN;
        ca_public_key_len  = ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1;

        ca_private_key = memory_malloc(ca_private_key_len);

        if(ca_private_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
            memory_free(ca_key_data);
            return -1;
        }

        ca_public_key = memory_malloc(ca_public_key_len);

        if(ca_public_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA public key");
            memory_free(ca_private_key);
            memory_free(ca_key_data);
            return -1;
        }

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

        if(x509_certificate_verify_signature(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to verify CA certificate signature with rebuild, trying without rebuild");
            if(x509_certificate_verify_signature_with_rebuild(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len, false) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to verify CA certificate signature");
                memory_free(ca_public_key);
                return -1;
            }
        }

        memory_free(ca_public_key);
    } else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP384R1_SHA384) {
        ca_private_key_len = ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN;
        ca_public_key_len  = ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN + 1;

        ca_private_key = memory_malloc(ca_private_key_len);

        if(ca_private_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
            memory_free(ca_key_data);
            return -1;
        }

        ca_public_key = memory_malloc(ca_public_key_len);

        if(ca_public_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA public key");
            memory_free(ca_private_key);
            memory_free(ca_key_data);
            return -1;
        }

        if(pem_read_secp384r1_private_key((char_t*)ca_key_data, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read CA private key from PEM");
            memory_free(ca_key_data);
            return -1;
        }
        memory_free(ca_key_data);

        if(ellipticcurve_secp384r1_derive_pubkey(ca_public_key + 1, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive CA public key from private key");
            return -1;
        }

        ca_public_key[0] = 0x04; // uncompressed point prefix

        if(x509_certificate_verify_signature(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to verify CA certificate signature with rebuild, trying without rebuild");
            if(x509_certificate_verify_signature_with_rebuild(ca_certificate, ca_key_algorithm, ca_public_key, ca_public_key_len, false) != 0) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to verify CA certificate signature");
                memory_free(ca_public_key);
                return -1;
            }
        }

        memory_free(ca_public_key);
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported CA public key algorithm: %d", ca_key_algorithm);
        return -1;
    }

    memory_free(ca_private_key);

    return 0;
}

static int8_t tls13_load_ca_certificate_and_key(boolean_t force_regenerate, ellipticcurve_curve_type_t curve_type) {
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
        return tls13_load_ca_certificate();
    }

    // generate new CA certificate and KEY    x509_certificate_t* cert = x509_certificate_new();
    x509_certificate_t* cert = x509_certificate_new();
    if (cert == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create new X509 certificate");
        return -1;
    }

    if (x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "TurnstoneOS CA") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject common name");
        x509_certificate_free(cert);
        return -1;
    }

    if(x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_ORGANIZATION, "TurnstoneOS") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject organization");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_COUNTRY, "TR") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject country");
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

    if (x509_certificate_add_key_usage(cert,
                                       X509_KEY_USAGE_DIGITAL_SIGNATURE |
                                       X509_KEY_USAGE_KEY_CERT_SIGN |
                                       X509_KEY_USAGE_CRL_SIGN) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add key usage");
        x509_certificate_free(cert);
        return -1;
    }

    if(curve_type == ELLIPTICCURVE_CURVE_TYPE_NONE) {
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
    } else if(curve_type == ELLIPTICCURVE_CURVE_TYPE_SECP256R1) {
        uint8_t private_key[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN];
        uint8_t public_key[1 + ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];

        if(ellipticcurve_secp256r1_generate_keypair(private_key, public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate SECP256R1 keypair");
            x509_certificate_free(cert);
            return -1;
        }

        public_key[0] = 0x04; // Uncompressed point prefix

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ECDSA_SECP256R1_SHA256, public_key, sizeof(public_key)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to certificate");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_sign(cert, NULL, X509_ALGORITHM_ECDSA_SECP256R1_SHA256,
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
    } else {
        uint8_t private_key[ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN];
        uint8_t public_key[1 + ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN];

        if(ellipticcurve_secp384r1_generate_keypair(private_key, public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate SECP384R1 keypair");
            x509_certificate_free(cert);
            return -1;
        }

        public_key[0] = 0x04; // Uncompressed point prefix

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ECDSA_SECP384R1_SHA384, public_key, sizeof(public_key)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to certificate");
            x509_certificate_free(cert);
            return -1;
        }

        if (x509_certificate_sign(cert, NULL, X509_ALGORITHM_ECDSA_SECP384R1_SHA384,
                                  private_key, sizeof(private_key)) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign certificate");
            x509_certificate_free(cert);
            return -1;
        }

        char_t* final_key_data = NULL;
        if(pem_write_secp384r1_private_key(private_key, &final_key_data) != 0) {
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

    ca_certificate = cert; // cache CA certificate in memory for future use

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

static int8_t tls13_load_server_certificate_and_key(tls13_session_t*     tls13_session,
                                                    x509_algorithm_t*    supported_algorithms,
                                                    const char_t*        sni_data,
                                                    x509_certificate_t** out_ca_cert,
                                                    x509_certificate_t** out_server_cert,
                                                    uint8_t**            out_private_key,
                                                    size_t*              out_private_key_len) {
    UNUSED(tls13_session);

    time_t start = time_ns(NULL);

    FILE* f;

    if(!ca_certificate) { // cache CA certificate in memory after first load to avoid file I/O on every handshake
        if(tls13_load_ca_certificate() != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load CA certificate");
            return -1;
        }
    }

    x509_algorithm_t ca_cert_algorithm = x509_certificate_get_public_key_algorithm(ca_certificate);

    for(int32_t i = 0; supported_algorithms[i] != X509_ALGORITHM_UNKNOWN; i++) {
        if(supported_algorithms[i] == ca_cert_algorithm) {
            break;
        }
        if(supported_algorithms[i] == X509_ALGORITHM_UNKNOWN) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "CA certificate public key algorithm is not supported by client");
            x509_certificate_free(ca_certificate);
            return -1;
        }
    }

    *out_ca_cert = ca_certificate;

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

    uint8_t* ca_private_key = NULL;
    size_t ca_private_key_len = 0;

    x509_algorithm_t ca_key_algorithm = x509_certificate_get_public_key_algorithm(ca_certificate);
    if (ca_key_algorithm == X509_ALGORITHM_ED25519) {
        ca_private_key_len = ED25519_PRIVATE_KEY_RAW_LEN;

        ca_private_key = memory_malloc(ca_private_key_len);

        if(ca_private_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
            memory_free(ca_key_data);
            return -1;
        }

        if(pem_read_ed25519_private_key((char_t*)ca_key_data, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read CA private key from PEM");
            memory_free(ca_key_data);
            return -1;
        }
        memory_free(ca_key_data);
    } else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
        ca_private_key_len = ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN;

        ca_private_key = memory_malloc(ca_private_key_len);

        if(ca_private_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
            memory_free(ca_key_data);
            return -1;
        }

        if(pem_read_secp256r1_private_key((char_t*)ca_key_data, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read CA private key from PEM");
            memory_free(ca_key_data);
            return -1;
        }
        memory_free(ca_key_data);
    } else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP384R1_SHA384) {
        ca_private_key_len = ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN;

        ca_private_key = memory_malloc(ca_private_key_len);

        if(ca_private_key == NULL) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA private key");
            memory_free(ca_key_data);
            return -1;
        }

        if(pem_read_secp384r1_private_key((char_t*)ca_key_data, ca_private_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read CA private key from PEM");
            memory_free(ca_key_data);
            return -1;
        }
        memory_free(ca_key_data);
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported CA public key algorithm: %d", ca_key_algorithm);
        return -1;
    }

    time_t cert_start = time_ns(NULL);

    // now generate server certificate signed by CA
    x509_certificate_t* cert = x509_certificate_new();
    if (cert == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create new X509 certificate");
        return -1;
    }

    if (x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "Test Server") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject common name");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_duration(cert, 10) != 0) {
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

    if(strcmp(sni_data, "localhost") == 0) {
        if (x509_certificate_add_subject_alternative_name(cert, X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS, sni_data) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject alternative name from SNI");
            x509_certificate_free(cert);
            return -1;
        }
    }

    if(x509_certificate_add_subject_alternative_name(cert, X509_SUBJECT_ALTERNATIVE_NAME_TYPE_IP, "127.0.0.1") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add subject alternative name IP");
        x509_certificate_free(cert);
        return -1;
    }

    uint8_t* server_private_key = NULL;
    size_t server_private_key_len = 0;

    time_t keygen_start;
    time_t keygen_end;
    time_t sign_start;
    time_t sign_end;

    if(ca_key_algorithm == X509_ALGORITHM_ED25519) {
        server_private_key_len = ED25519_PRIVATE_KEY_RAW_LEN;
        server_private_key = memory_malloc(ED25519_PRIVATE_KEY_RAW_LEN);
        if(!server_private_key) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for server private key");
            x509_certificate_free(cert);
            return -1;
        }

        uint8_t server_public_key[32];
        keygen_start = time_ns(NULL);
        if(ed25519_generate_keypair(server_private_key, server_public_key) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server X25519 keypair");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
        keygen_end = time_ns(NULL);

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ED25519, server_public_key, ED25519_PUBLIC_KEY_RAW_LEN) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }

        sign_start = time_ns(NULL);
        if (x509_certificate_sign(cert, ca_certificate, ca_key_algorithm,
                                  ca_private_key, ca_private_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
        sign_end = time_ns(NULL);
    } else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP256R1_SHA256) {
        server_private_key_len = ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN;
        server_private_key = memory_malloc(ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN);

        if(!server_private_key) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for server private key");
            x509_certificate_free(cert);
            return -1;
        }

        uint8_t server_public_key[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1]; // +1 for uncompressed point prefix

        keygen_start = time_ns(NULL);
        if(ellipticcurve_secp256r1_generate_keypair(server_private_key, server_public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server ECDSA SECP256R1 keypair");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
        keygen_end = time_ns(NULL);

        server_public_key[0] = 0x04; // uncompressed point prefix

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ECDSA_SECP256R1_SHA256, server_public_key, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }

        sign_start = time_ns(NULL);
        if (x509_certificate_sign(cert, ca_certificate, ca_key_algorithm,
                                  ca_private_key, ca_private_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
        sign_end = time_ns(NULL);
    } else if(ca_key_algorithm == X509_ALGORITHM_ECDSA_SECP384R1_SHA384) {
        server_private_key_len = ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN;
        server_private_key = memory_malloc(ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN);

        if(!server_private_key) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for server private key");
            x509_certificate_free(cert);
            return -1;
        }

        uint8_t server_public_key[ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN + 1]; // +1 for uncompressed point prefix

        keygen_start = time_ns(NULL);
        if(ellipticcurve_secp384r1_generate_keypair(server_private_key, server_public_key + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to generate server ECDSA SECP384R1 keypair");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
        keygen_end = time_ns(NULL);

        server_public_key[0] = 0x04; // uncompressed point prefix

        if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ECDSA_SECP384R1_SHA384, server_public_key, ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN + 1) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to add public key to server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }

        sign_start = time_ns(NULL);
        if (x509_certificate_sign(cert, ca_certificate, ca_key_algorithm,
                                  ca_private_key, ca_private_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to sign server certificate");
            x509_certificate_free(cert);
            memory_free(server_private_key);
            return -1;
        }
        sign_end = time_ns(NULL);
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unsupported CA public key algorithm: %d", ca_key_algorithm);
        x509_certificate_free(cert);
        return -1;
    }

    memory_free(ca_private_key);

    *out_server_cert = cert;
    *out_private_key = server_private_key;
    *out_private_key_len = server_private_key_len;

    time_t end = time_ns(NULL);

    uint64_t duration_ms = (end - start) / 1000000;
    uint64_t cert_duration_ms = (end - cert_start) / 1000000;
    uint64_t keygen_duration_ms = (keygen_end - keygen_start) / 1000000;
    uint64_t sign_duration_ms = (sign_end - sign_start) / 1000000;

    PRINTLOG(CRYPTOLIB, LOG_INFO, "Server certificate and key loaded successfully in %llu ms (cert generation: %llu ms, keygen: %llu ms, signing: %llu ms)", duration_ms, cert_duration_ms, keygen_duration_ms, sign_duration_ms);

    return 0;
}

static int8_t tls13_client_certificate_verify(tls13_session_t*     ctx,
                                              x509_certificate_t** certificate_chain,
                                              size_t               chain_length) {
    UNUSED(ctx);

    if(!ca_certificate) { // cache CA certificate in memory after first load to avoid file I/O on every handshake
        if(tls13_load_ca_certificate() != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load CA certificate for client certificate verification");
            return -1;
        }
    }

    // Verify the very top cert anchors to our CA
    if(!x509_certificate_is_authority_of(certificate_chain[chain_length - 1], ca_certificate)) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Top of chain does not anchor to CA");
        return -1;
    }

    // Now verify the rest of the chain downward
    for(int32_t i = chain_length - 1; i > 0; i--) {
        if(!x509_certificate_is_authority_of(certificate_chain[i - 1], certificate_chain[i])) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Broken link at index %d", i);
            return -1;
        }
    }

    return 0;
}

static int8_t tls13_client_certificates_ca_dn_list(tls13_session_t* ctx,
                                                   uint8_t***       out_ca_dn_list,
                                                   size_t**         out_ca_dn_list_length,
                                                   size_t*          out_ca_count){
    UNUSED(ctx);

    if(!ca_certificate) { // cache CA certificate in memory after first load to avoid file I/O on every handshake
        if(tls13_load_ca_certificate() != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to load CA certificate for client CA DN list");
            return -1;
        }
    }

    uint8_t** ca_dn_list = memory_malloc(sizeof(uint8_t*));
    size_t* ca_dn_list_length = memory_malloc(sizeof(size_t));
    if(!ca_dn_list || !ca_dn_list_length) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for CA DN list");
        memory_free(ca_dn_list);
        memory_free(ca_dn_list_length);
        return -1;
    }

    if(x509_certificate_get_subject_der(ca_certificate, &ca_dn_list[0], &ca_dn_list_length[0]) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get CA certificate subject in DER format");
        memory_free(ca_dn_list);
        memory_free(ca_dn_list_length);
        return -1;
    }

    *out_ca_dn_list = ca_dn_list;
    *out_ca_dn_list_length = ca_dn_list_length;
    *out_ca_count = 1;

    return 0;
}

typedef struct tls13_psk_encryption_parameters_t {
    uint8_t key[AES256_KEY_SIZE];
    uint8_t iv[12];
    uint8_t aed_key[16];
} tls13_psk_encryption_parameters_t;

static int8_t tls13_get_psk_encryption_keys(tls13_session_t* ctx,
                                            boolean_t        previous_key,
                                            uint8_t**        out_psk_encryption_key,
                                            uint8_t**        out_psk_encryption_iv,
                                            uint8_t**        out_psk_aed_key) {
    UNUSED(ctx);

    static tls13_psk_encryption_parameters_t psk_params[2]; // double buffer for current and previous keys
    static boolean_t keys_initialized = false;

    if(!keys_initialized) {
        FILE* f = fopen("build/psk_key.bin", "rb");
        if(f) {
            size_t read_size = fread(psk_params[0].key, 1, sizeof(psk_params[0].key), f);
            if(read_size != sizeof(psk_params[0].key)) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read PSK encryption key from file");
                fclose(f);
                return -1;
            }
            read_size = fread(psk_params[0].iv, 1, sizeof(psk_params[0].iv), f);
            if(read_size != sizeof(psk_params[0].iv)) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read PSK encryption IV from file");
                fclose(f);
                return -1;
            }
            read_size = fread(psk_params[0].aed_key, 1, sizeof(psk_params[0].aed_key), f);
            if(read_size != sizeof(psk_params[0].aed_key)) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to read PSK encryption AED key from file");
                fclose(f);
                return -1;
            }
            fclose(f);
        } else {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "PSK key file not found, generating random keys");
            get_random_bytes(psk_params[0].key, sizeof(psk_params[0].key));
            get_random_bytes(psk_params[0].iv, sizeof(psk_params[0].iv));
            get_random_bytes(psk_params[0].aed_key, sizeof(psk_params[0].aed_key));
        }


        get_random_bytes(psk_params[1].key, sizeof(psk_params[1].key));
        get_random_bytes(psk_params[1].iv, sizeof(psk_params[1].iv));
        get_random_bytes(psk_params[1].aed_key, sizeof(psk_params[1].aed_key));

        f = fopen("build/psk_key.bin", "wb");
        if(f) {
            fwrite(psk_params[1].key, 1, sizeof(psk_params[1].key), f);
            fwrite(psk_params[1].iv, 1, sizeof(psk_params[1].iv), f);
            fwrite(psk_params[1].aed_key, 1, sizeof(psk_params[1].aed_key), f);
            fclose(f);
        } else {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Failed to save PSK keys to file");
        }

        keys_initialized = true;
    }

    int32_t index = previous_key ? 0 : 1;

    *out_psk_encryption_key = psk_params[index].key;
    *out_psk_encryption_iv  = psk_params[index].iv;
    *out_psk_aed_key = psk_params[index].aed_key;

    return 0;
}

static int32_t recv_all(int64_t sockfd, uint8_t* buffer, int32_t length, int32_t flags) {
    int32_t total_received = 0;
    boolean_t once = flags & 0x80000000; // custom flag to indicate recv should be called only once
    while (total_received < length) {
        int32_t bytes_received = recv(sockfd, buffer + total_received, length - total_received, flags);
        if(bytes_received == 0) {
            PRINTLOG(CRYPTOLIB, LOG_WARNING, "Connection closed by peer. bytes_received: %d", bytes_received);
            return total_received; // connection closed, return what we have
        }
        if (bytes_received < 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "recv failed or connection closed. error code: %lli. bytes_received: %d", errno, bytes_received);
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

static int8_t index_handler(http_request_t* request, http_response_t* response) {
    UNUSED(request);
    const char* body = "<html><body><h1>Welcome to the TLS 1.3 Test Server</h1><p>This is a simple page served over HTTPS.</p></body></html>";
    http_response_set_status_code(response, HTTP_STATUS_CODE_OK);
    http_response_set_content_type(response, HTTP_CONTENT_TYPE_TEXT_HTML);
    http_response_write_string(response, body);
    return 0;
}

static int8_t hello_world_handler(http_request_t* request, http_response_t* response) {
    UNUSED(request);
    const char* body = "<html><body><h1>Hello, World!</h1><p>This page is served over HTTPS with TLS 1.3.</p></body></html>";
    http_response_set_status_code(response, HTTP_STATUS_CODE_OK);
    http_response_set_content_type(response, HTTP_CONTENT_TYPE_TEXT_HTML);
    http_response_write_string(response, body);
    return 0;
}

int32_t main(int32_t argc, char_t** argv) {
    signal(SIGPIPE, SIG_IGN);

    ellipticcurve_curve_type_t curve_type = ELLIPTICCURVE_CURVE_TYPE_NONE;
    boolean_t force_ca_regenerate = false;
    boolean_t require_client_certificate = false;

    // parse command line arguments
    for(int32_t i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--use-secp256r1") == 0) {
            curve_type = ELLIPTICCURVE_CURVE_TYPE_SECP256R1;
        } else if(strcmp(argv[i], "--use-secp384r1") == 0) {
            curve_type = ELLIPTICCURVE_CURVE_TYPE_SECP384R1;
        } else if(strcmp(argv[i], "--force-ca-regenerate") == 0) {
            force_ca_regenerate = true;
        } else if(strcmp(argv[i], "--require-client-cert") == 0) {
            require_client_certificate = true;
        }
    }

    gcm_initialize();

    PRINTLOG(CRYPTOLIB, LOG_INFO, "TLS server test application");

    if(tls13_load_ca_certificate_and_key(force_ca_regenerate, curve_type) != 0) {
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
        x509_certificate_free(ca_certificate);
        return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "setsockopt SO_REUSEADDR failed");
        close(server_fd);
        x509_certificate_free(ca_certificate);
        return 1;
    }

    memory_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "bind failed. error code: %lli", errno);
        close(server_fd);
        x509_certificate_free(ca_certificate);
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "listen failed");
        close(server_fd);
        x509_certificate_free(ca_certificate);
        return 1;
    }

    http_application_context_t* http_application_ctx = NULL;
    http_application_ctx = http_create_application_context("localhost:10443");

    if(http_application_ctx == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create HTTP application context");
        close(server_fd);
        x509_certificate_free(ca_certificate);
        return 1;
    }

    http_add_handler(http_application_ctx, HTTP_METHOD_GET, "/", index_handler);
    http_add_handler(http_application_ctx, HTTP_METHOD_GET, "/hello", hello_world_handler);

    tls13_config_t* tls13_config = tls13_create_config(tls13_load_server_certificate_and_key,
                                                       tls13_client_certificate_verify,
                                                       tls13_client_certificates_ca_dn_list,
                                                       tls13_get_psk_encryption_keys,
                                                       require_client_certificate
                                                       );

    if(tls13_config == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create TLS 1.3 configuration");
        close(server_fd);
        x509_certificate_free(ca_certificate);
        http_destroy_application_context(http_application_ctx);
        return 1;
    }

    if(tls13_config_set_network_callbacks(tls13_config, send_all, recv_all) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set TLS 1.3 network callbacks");
        tls13_destroy_config(tls13_config);
        close(server_fd);
        x509_certificate_free(ca_certificate);
        http_destroy_application_context(http_application_ctx);
        return 1;
    }

    tls13_application_context_t* tls13_app_ctx = http_get_tls13_application_context(http_application_ctx);

    if(tls13_config_set_application_callbacks(tls13_config,
                                              tls13_app_ctx,
                                              http_plaintext_redirect_handler,
                                              http_application_handler) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set TLS 1.3 application data callback");
        tls13_destroy_config(tls13_config);
        close(server_fd);
        x509_certificate_free(ca_certificate);
        http_destroy_application_context(http_application_ctx);
        return 1;
    }

    PRINTLOG(CRYPTOLIB, LOG_INFO, "Server listening on port %d (SO_REUSEADDR enabled)", PORT);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Waiting for connections...");

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

        if(tls13_handle_connection(tls13_config, client_fd) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Error handling TLS connection with %s:%d", client_ip, ntohs(client_addr.sin_port));
        } else {
            PRINTLOG(CRYPTOLIB, LOG_INFO, "Connection with %s:%d handled successfully", client_ip, ntohs(client_addr.sin_port));
        }

        close(client_fd);
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Connection closed");
    }

    tls13_destroy_config(tls13_config);
    http_destroy_application_context(http_application_ctx);

    close(server_fd);
    x509_certificate_free(ca_certificate);

    return 0;
}
