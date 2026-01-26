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

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"-----BEGIN ", strlen("-----BEGIN "))) {
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)header, strlen(header))) {
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"-----\n", strlen("-----\n"))) {
        buffer_destroy(pem_buffer);
        return -1;
    }

    uint8_t* b64_encoded = NULL;
    size_t b64_length = base64_encode(der_data, der_length, true, &b64_encoded);
    if (b64_length == 0 || b64_encoded == NULL) {
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, b64_encoded, b64_length)) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"\n-----END ", strlen("\n-----END "))) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)header, strlen(header))) {
        memory_free(b64_encoded);
        buffer_destroy(pem_buffer);
        return -1;
    }

    if(!buffer_append_bytes(pem_buffer, (uint8_t*)"-----\n", strlen("-----\n"))) {
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

    const char_t* begin_marker = "-----BEGIN ";;
    const char_t* end_marker = "\n-----END ";

    if(strncmp(pem_data, begin_marker, strlen(begin_marker)) != 0) {
        return -1;
    }

    pem_data += strlen(begin_marker);
    pem_length -= strlen(begin_marker);

    if(strncmp(pem_data, header, strlen(header)) != 0) {
        return -1;
    }

    pem_data += strlen(header);
    pem_length -= strlen(header);

    if(strncmp(pem_data, "-----\n", strlen("-----\n")) != 0) {
        return -1;
    }

    pem_data += strlen("-----\n");
    pem_length -= strlen("-----\n");

    const char_t* end_marker_pos = strstr(pem_data, end_marker);
    if (end_marker_pos == NULL) {
        return -1;
    }

    const char_t* b64_end = end_marker_pos;

    end_marker_pos += strlen(end_marker);
    if(strncmp(end_marker_pos, header, strlen(header)) != 0) {
        return -1;
    }

    end_marker_pos += strlen(header);
    if(strncmp(end_marker_pos, "-----", strlen("-----")) != 0) {
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
    size_t der_length = base64_decode(b64_data, b64_length, &der_data);
    memory_free(b64_data);

    if (der_length == 0 || der_data == NULL) {
        return -1;
    }

    *out_der_data = der_data;
    *out_der_length = der_length;

    return 0;
}
