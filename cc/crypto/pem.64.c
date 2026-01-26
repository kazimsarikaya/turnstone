/**
 * @file pem.64.c
 * @brief PEM (Privacy-Enhanced Mail) format handling.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <crypto/pem.h>
#include <base64.h>
#include <memory.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.lib.crypto");

#define PEM_START_MARKER "-----BEGIN "
#define PEM_START_MARKER_LEN (sizeof(PEM_START_MARKER) - 1)
#define PEM_END_MARKER   "-----END "
#define PEM_END_MARKER_LEN (sizeof(PEM_END_MARKER) - 1)
#define PEM_MARKER_SUFFIX "-----\n"
#define PEM_MARKER_SUFFIX_LEN (sizeof(PEM_MARKER_SUFFIX) - 1)

int8_t pem_encode(const char_t* header,
                  const uint8_t* der_data, size_t der_length,
                  char_t** out_pem, size_t* out_pem_length) {
    if (header == NULL || der_data == NULL || der_length == 0 || out_pem == NULL) {
        return -1;
    }

    buffer_t* pem_buffer = buffer_new();
    if (pem_buffer == NULL) {
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)PEM_START_MARKER, PEM_START_MARKER_LEN)) {
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)header, strlen(header))) {
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)PEM_MARKER_SUFFIX, PEM_MARKER_SUFFIX_LEN)) {
        buffer_destroy(pem_buffer);
        return -1;
    }

    uint8_t* b64_encoded = NULL;
    size_t b64_length = base64_encode_rfc7468(der_data, der_length, &b64_encoded); // third parameter true to add newlines
    if (b64_length == 0 || b64_encoded == NULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to base64 encode DER data. b64_length=%llu, b64_encoded is null %i", b64_length, b64_encoded == NULL);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, b64_encoded, b64_length)) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)PEM_END_MARKER, PEM_END_MARKER_LEN)) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)header, strlen(header))) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)PEM_MARKER_SUFFIX, PEM_MARKER_SUFFIX_LEN)) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_byte(pem_buffer, '\0')) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    uint8_t* pem_data_bytes = buffer_get_all_bytes_and_destroy(pem_buffer, out_pem_length);
    memory_free(b64_encoded);

    if (pem_data_bytes == NULL) {
        return -1;
    }

    *out_pem = (char_t*)pem_data_bytes;

    return 0;
}


int8_t pem_decode(const char_t* header,
                  const char_t* pem_data, size_t pem_length,
                  uint8_t** out_der_data, size_t* out_der_length) {
    if (pem_data == NULL || pem_length == 0 || out_der_data == NULL || out_der_length == NULL) {
        return -1;
    }


    if(strncmp(pem_data, PEM_START_MARKER, PEM_START_MARKER_LEN) != 0) {
        return -1;
    }

    pem_data += PEM_START_MARKER_LEN;
    pem_length -= PEM_START_MARKER_LEN;

    if(strncmp(pem_data, header, strlen(header)) != 0) {
        return -1;
    }

    pem_data += strlen(header);
    pem_length -= strlen(header);

    if(strncmp(pem_data, PEM_MARKER_SUFFIX, PEM_MARKER_SUFFIX_LEN) != 0) {
        return -1;
    }

    pem_data += PEM_MARKER_SUFFIX_LEN;
    pem_length -= PEM_MARKER_SUFFIX_LEN;

    const char_t* end_marker_pos = strstr(pem_data, PEM_END_MARKER);
    if (end_marker_pos == NULL) {
        return -1;
    }

    const char_t* b64_end = end_marker_pos;

    end_marker_pos += PEM_END_MARKER_LEN;
    if(strncmp(end_marker_pos, header, strlen(header)) != 0) {
        return -1;
    }

    end_marker_pos += strlen(header);
    if(strncmp(end_marker_pos, PEM_MARKER_SUFFIX, PEM_MARKER_SUFFIX_LEN - 1) != 0) { // -1 to ignore final newline
        return -1;
    }

    size_t b64_length = b64_end - pem_data;
    uint8_t* b64_data = memory_malloc(b64_length + 1);
    if (b64_data == NULL) {
        return -1;
    }

    memory_memcopy(pem_data, b64_data, b64_length);
    b64_data[b64_length] = '\0';

    uint8_t* der_data = NULL;
    size_t der_length = base64_decode(b64_data, b64_length, &der_data); // auto handles newlines
    memory_free(b64_data);

    if (der_length == 0 || der_data == NULL) {
        return -1;
    }

    *out_der_data = der_data;
    *out_der_length = der_length;

    return 0;
}
