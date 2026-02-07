/**
 * @file test_x509.c
 * @brief X25519 Tests
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x8000000
#include "setup.h"
#include <base64.h>
#include <bigint.h>
#include <strings.h>
#include <crypto/x25519.h>
#include <time.h>
#include <buffer.h>
#include <crypto/x509.h>
#include <crypto/sha2.h>
#include <crypto/pem.h>
#include <crypto/der.h>

int32_t main(void);

int32_t main(void) {
    int8_t res;

    time_t start_time = time_ns(NULL);

    x509_certificate_t* cert = x509_certificate_new();
    if (cert == NULL) {
        print_error("Failed to create new X509 certificate\n");
        return -1;
    }

    if (x509_certificate_add_issuer_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "Test CA") != 0) {
        print_error("Failed to add issuer common name\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "Test CA") != 0) {
        print_error("Failed to add subject common name\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_duration(cert, 365) != 0) {
        print_error("Failed to add certificate duration\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_set_is_ca(cert, true, -1) != 0) {
        print_error("Failed to set certificate as CA\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_key_usage(cert, X509_KEY_USAGE_KEY_CERT_SIGN | X509_KEY_USAGE_CRL_SIGN) != 0) {
        print_error("Failed to add key usage\n");
        x509_certificate_free(cert);
        return -1;
    }

    uint8_t private_key[32];
    uint8_t public_key[32];

    if(ed25519_generate_keypair(private_key, public_key) != 0) {
        print_error("Failed to generate X25519 keypair\n");
        x509_certificate_free(cert);
        return -1;
    }

    uint8_t* skid = sha256_hash(public_key, 32);
    if(skid == NULL) {
        print_error("Failed to generate SKID\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_key_identifier(cert, skid, 32) != 0) {
        print_error("Failed to add subject key identifier\n");
        memory_free(skid);
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_authority_key_identifier(cert, skid, 32) != 0) {
        print_error("Failed to add authority key identifier\n");
        memory_free(skid);
        x509_certificate_free(cert);
        return -1;
    }

    memory_free(skid);

    if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ED25519, public_key, 32) != 0) {
        print_error("Failed to add public key to certificate\n");
        x509_certificate_free(cert);
        return -1;
    }

    time_t start_sign_time = time_ns(NULL);

    if (x509_certificate_sign(cert, X509_ALGORITHM_ED25519,
                              private_key, sizeof(private_key)) != 0) {
        print_error("Failed to sign certificate\n");
        x509_certificate_free(cert);
        return -1;
    }

    time_t end_sign_time = time_ns(NULL);
    uint64_t diff_sign_time = end_sign_time - start_sign_time;
    printf("X509 CA certificate signing time: %llu ns %llu ms\n", diff_sign_time, diff_sign_time / 1000000);

    time_t start_pem_time = time_ns(NULL);

    char_t* final_cert_data = x509_certificate_get_pem(cert);
    if (final_cert_data == NULL) {
        print_error("Failed to get final certificate data\n");
        x509_certificate_free(cert);
        return -1;
    }

    time_t end_pem_time = time_ns(NULL);
    uint64_t diff_pem_time = end_pem_time - start_pem_time;
    printf("X509 CA certificate PEM conversion time: %llu ns %llu ms\n", diff_pem_time, diff_pem_time / 1000000);

    x509_certificate_free(cert);

    FILE* f;

    f = fopen("build/ca.pem", "wb");
    if (f == NULL) {
        print_error("Failed to open certificate.der for writing\n");
        memory_free(final_cert_data);
        return -1;
    }

    fwrite(final_cert_data, 1, strlen(final_cert_data), f);
    fclose(f);

    memory_free(final_cert_data);

    char_t* final_key_data = NULL;
    if(pem_write_ed25519_private_key(private_key, &final_key_data) != 0) {
        print_error("Failed to write private key to PEM format\n");
        return -1;
    }

    f = fopen("build/ca.key", "wb");
    if (f == NULL) {
        print_error("Failed to open ca.key for writing\n");
        memory_free(final_key_data);
        return -1;
    }

    fwrite(final_key_data, 1, strlen(final_key_data), f);
    fclose(f);

    memory_free(final_key_data);

    time_t end_time = time_ns(NULL);
    uint64_t diff_time = end_time - start_time;
    printf("X509 CA generation time: %llu ns %llu ms\n", diff_time, diff_time / 1000000);

    print_success("CA certificate generated successfully: ca.pem ca.key\n");

    start_time = time_ns(NULL);

    cert = x509_certificate_new();
    if (cert == NULL) {
        print_error("Failed to create new X509 certificate\n");
        return -1;
    }

    if (x509_certificate_add_issuer_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "Test CA") != 0) {
        print_error("Failed to add issuer common name\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_field(cert, X509_ISSUER_SUBJECT_FIELD_COMMON_NAME, "Test Server") != 0) {
        print_error("Failed to add subject common name\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_duration(cert, 365) != 0) {
        print_error("Failed to add certificate duration\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_set_is_ca(cert, false, -1) != 0) {
        print_error("Failed to set certificate as non-CA\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_key_usage(cert, X509_KEY_USAGE_DIGITAL_SIGNATURE | X509_KEY_USAGE_KEY_ENCIPHERMENT) != 0) {
        print_error("Failed to add key usage\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_extended_key_usage(cert, X509_EXTENDED_KEY_USAGE_SERVER_AUTH) != 0) {
        print_error("Failed to add extended key usage\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_alternative_name(cert, X509_SUBJECT_ALTERNATIVE_NAME_TYPE_DNS, "localhost") != 0) {
        print_error("Failed to add subject alternative name\n");
        x509_certificate_free(cert);
        return -1;
    }

    uint8_t server_private_key[32];
    uint8_t server_public_key[32];

    if(ed25519_generate_keypair(server_private_key, server_public_key) != 0) {
        print_error("Failed to generate server X25519 keypair\n");
        x509_certificate_free(cert);
        return -1;
    }

    skid = sha256_hash(server_public_key, 32);
    if(skid == NULL) {
        print_error("Failed to generate server SKID\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_subject_key_identifier(cert, skid, 32) != 0) {
        print_error("Failed to add server subject key identifier\n");
        memory_free(skid);
        x509_certificate_free(cert);
        return -1;
    }

    memory_free(skid);

    uint8_t* akid = sha256_hash(public_key, 32);
    if(akid == NULL) {
        print_error("Failed to generate server AKID\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_add_authority_key_identifier(cert, akid, 32) != 0) {
        print_error("Failed to add server authority key identifier\n");
        memory_free(akid);
        x509_certificate_free(cert);
        return -1;
    }

    memory_free(akid);

    if (x509_certificate_add_public_key(cert, X509_ALGORITHM_ED25519, server_public_key, 32) != 0) {
        print_error("Failed to add public key to server certificate\n");
        x509_certificate_free(cert);
        return -1;
    }

    if (x509_certificate_sign(cert, X509_ALGORITHM_ED25519,
                              private_key, sizeof(private_key)) != 0) {
        print_error("Failed to sign server certificate\n");
        x509_certificate_free(cert);
        return -1;
    }

    char_t* final_server_cert_data = x509_certificate_get_pem(cert);
    if (final_server_cert_data == NULL) {
        print_error("Failed to get final server certificate data\n");
        x509_certificate_free(cert);
        return -1;
    }

    x509_certificate_free(cert);

    f = fopen("build/server_certificate.pem", "wb");
    if (f == NULL) {
        print_error("Failed to open server_certificate.der for writing\n");
        x509_certificate_free(cert);
        return -1;
    }

    fwrite(final_server_cert_data, 1, strlen(final_server_cert_data), f);
    fclose(f);

    char_t* final_server_key_data = NULL;
    if(pem_write_ed25519_private_key(server_private_key, &final_server_key_data) != 0) {
        print_error("Failed to write server private key to PEM format\n");
        return -1;
    }

    f = fopen("build/server_certificate.key", "wb");
    if (f == NULL) {
        print_error("Failed to open server_certificate.key for writing\n");
        memory_free(final_server_key_data);
        return -1;
    }

    fwrite(final_server_key_data, 1, strlen(final_server_key_data), f);
    fclose(f);

    memory_free(final_server_key_data);

    end_time = time_ns(NULL);
    diff_time = end_time - start_time;
    printf("X509 Server certificate generation time: %llu ns %llu ms\n", diff_time, diff_time / 1000000);

    start_time = time_ns(NULL);

    cert = x509_certificate_from_pem(final_server_cert_data);
    if (cert == NULL) {
        print_error("Failed to parse server certificate from PEM\n");
        memory_free(final_server_cert_data);
        return -1;
    }

    end_time = time_ns(NULL);
    diff_time = end_time - start_time;
    printf("X509 Server certificate PEM parsing time: %llu ns %llu ms\n", diff_time, diff_time / 1000000);

    memory_free(final_server_cert_data);

    start_time = time_ns(NULL);

    if( x509_certificate_verify_signature(cert, public_key, 32) != 0) {
        print_error("Failed to verify server certificate signature\n");
        x509_certificate_free(cert);
        return -1;
    }

    end_time = time_ns(NULL);
    diff_time = end_time - start_time;
    printf("X509 Server certificate verification time: %llu ns %llu ms\n", diff_time, diff_time / 1000000);

    x509_certificate_free(cert);

    print_success("Server certificate generated successfully: server_certificate.pem server_certificate.key\n");

    res = 0;
    return res;
}
