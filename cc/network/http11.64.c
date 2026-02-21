/**
 * @file http11.64.c
 * @brief HTTP 1.1 Protocol Implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/http.h>
#include <logging.h>
#include <strings.h>
#include <list.h>

MODULE("turnstone.lib.network.http");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t http11_handle_connection(tls13_session_t* ctx) {
    int8_t ret = -1;
    http_request_t* request = NULL;
    http_response_t* response = NULL;


    uint8_t buffer[16384];
    memory_memclean(buffer, sizeof(buffer));
    int32_t bytes_received = 0;
    bytes_received = tls13_read(ctx, buffer, sizeof(buffer) - 1);
    if (bytes_received < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "TLS application data read failed");
        return -1;
    }

    buffer[bytes_received] = '\0'; // Null-terminate for string operations

    request = (http_request_t*)memory_malloc(sizeof(http_request_t));
    if(!request) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP request");
        return -1;
    }

    char_t method[16], path[1024], version[16];
    size_t offset = 0;

    while(buffer[offset] == ' ') {
        offset++; // Skip leading spaces
    }

    while(buffer[offset] != ' ' && buffer[offset] != '\0' && offset < sizeof(buffer) - 1) {
        method[offset] = buffer[offset];
        offset++;
    }
    method[offset] = '\0';

    if(strcmp(method, "GET") == 0) {
        request->method = HTTP_METHOD_GET;
    } else if(strcmp(method, "POST") == 0) {
        request->method = HTTP_METHOD_POST;
    } else if(strcmp(method, "PUT") == 0) {
        request->method = HTTP_METHOD_PUT;
    } else if(strcmp(method, "DELETE") == 0) {
        request->method = HTTP_METHOD_DELETE;
    } else if(strcmp(method, "HEAD") == 0) {
        request->method = HTTP_METHOD_HEAD;
    } else if(strcmp(method, "OPTIONS") == 0) {
        request->method = HTTP_METHOD_OPTIONS;
    } else if(strcmp(method, "PATCH") == 0) {
        request->method = HTTP_METHOD_PATCH;
    } else {
        PRINTLOG(HTTP, LOG_ERROR, "Unknown HTTP method: %s", method);
        goto error_cleanup;
    }

    while(buffer[offset] == ' ') {
        offset++; // Skip spaces
    }

    size_t path_start = offset;
    while(buffer[offset] != ' ' && buffer[offset] != '\0' && offset < sizeof(buffer) - 1) {
        path[offset - path_start] = buffer[offset];
        offset++;
    }
    path[offset - path_start] = '\0';

    while(buffer[offset] == ' ') {
        offset++; // Skip spaces
    }

    size_t version_start = offset;
    while(buffer[offset] != '\r' && buffer[offset] != '\n' && buffer[offset] != '\0' && offset < sizeof(buffer) - 1) {
        version[offset - version_start] = buffer[offset];
        offset++;
    }
    version[offset - version_start] = '\0';

    if(strcmp(version, "HTTP/1.1") == 0) {
        request->version = HTTP_VERSION_1_1;
    } else {
        PRINTLOG(HTTP, LOG_ERROR, "Unsupported HTTP version: %s", version);
        goto error_cleanup;
    }

    // path can have query string, parse and separate it
    size_t query_pos = 0;
    while(path[query_pos] != '\0' && path[query_pos] != '?') {
        query_pos++;
    }

    if(path[query_pos] == '?') {
        request->query_params = list_create_list();
        if(!request->query_params) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP query params list");
            goto error_cleanup;
        }

        path[query_pos] = '\0'; // terminate path
        memory_memcopy(path, request->path, query_pos + 1);

        // now parse query string
        size_t qp_start = query_pos + 1;;
        while(path[qp_start] != '\0') {
            size_t name_start = qp_start;

            while(path[qp_start] != '=' && path[qp_start] != '&' && path[qp_start] != '\0') {
                qp_start++;
            }

            char_t* name = (char_t*)memory_malloc(qp_start - name_start + 1);
            if(!name) {
                PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP query param name");
                goto error_cleanup;
            }

            memory_memcopy(&path[name_start], name, qp_start - name_start);

            name[qp_start - name_start] = '\0';

            char_t* value = NULL;

            if(path[qp_start] == '=') {
                qp_start++;
                size_t value_start = qp_start;

                while(path[qp_start] != '&' && path[qp_start] != '\0') {
                    qp_start++;
                }

                value = (char_t*)memory_malloc(qp_start - value_start + 1);
                if(!value) {
                    PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP query param value");
                    memory_free(name);
                    goto error_cleanup;
                }
                memory_memcopy(&path[value_start], value, qp_start - value_start);
                value[qp_start - value_start] = '\0';
            }

            http_query_param_t* param = (http_query_param_t*)memory_malloc(sizeof(http_query_param_t));
            if(!param) {
                PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP query param");
                memory_free(name);
                if(value) {
                    memory_free(value);
                }
                goto error_cleanup;
            }

            param->name  = name;
            param->value = value;

            if(list_list_insert(request->query_params, param) == -1ULL) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to insert HTTP query param into list");
                memory_free(name);
                if(value) {
                    memory_free(value);
                }
                memory_free(param);
                goto error_cleanup;
            }

            if(path[qp_start] == '&') {
                qp_start++;
            }
        }
    } else {
        memory_memcopy(path, request->path, strlen(path) + 1);
    }

    request->headers = list_create_list();
    if(!request->headers) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP headers list");
        goto error_cleanup;
    }

    size_t content_length = 0;

    // remove CRLF from the end of the request line
    if(buffer[offset] == '\r' && buffer[offset + 1] == '\n') {
        offset += 2;
    }

    while(buffer[offset] != '\0' && !(buffer[offset] == '\r' && buffer[offset + 1] == '\n') && offset < (size_t)bytes_received) {
        // parse header line
        size_t name_start = offset;
        while(buffer[offset] != ':' && buffer[offset] != '\0' && offset < (size_t)bytes_received) {
            offset++;
        }

        if(buffer[offset] == '\0') {
            break;
        }

        char_t* name = (char_t*)memory_malloc(offset - name_start + 1);
        if(!name) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP header name");
            goto error_cleanup;
        }
        memory_memcopy(&buffer[name_start], name, offset - name_start);
        name[offset - name_start] = '\0';

        offset++; // skip ':'
        while(buffer[offset] == ' ') {
            offset++; // skip spaces
        }

        size_t value_start = offset;
        while(!(buffer[offset] == '\r' && buffer[offset + 1] == '\n') && buffer[offset] != '\0') {
            offset++;
        }

        char_t* value = (char_t*)memory_malloc(offset - value_start + 1);
        if(!value) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP header value");
            memory_free(name);
            goto error_cleanup;
        }
        memory_memcopy(&buffer[value_start], value, offset - value_start);
        value[offset - value_start] = '\0';

        if(buffer[offset] == '\r' && buffer[offset + 1] == '\n') {
            offset += 2; // skip CRLF
        }

        http_header_t* header = (http_header_t*)memory_malloc(sizeof(http_header_t));
        if(!header) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP header");
            memory_free(name);
            memory_free(value);
            goto error_cleanup;
        }

        header->name  = name;
        header->value = value;

        if(list_list_insert(request->headers, header) == -1ULL) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to insert HTTP header into list");
            memory_free(name);
            memory_free(value);
            memory_free(header);
            goto error_cleanup;
        }

        if(strcmp(name, "Content-Length") == 0  || strcmp(name, "content-length") == 0) {
            content_length = atou(value);
            request->content_length = content_length;
        }
    }

    if(buffer[offset] == '\r' && buffer[offset + 1] == '\n') {
        offset += 2; // skip final CRLF
    }

    if(content_length > 0) {
        request->body = buffer_new_with_capacity(NULL, content_length);
        if(!request->body) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP request body");
            goto error_cleanup;
        }

        size_t body_bytes_read = bytes_received - offset;
        if(body_bytes_read > content_length) {
            body_bytes_read = content_length;
        }

        buffer_append_bytes(request->body, &buffer[offset], body_bytes_read);

        while(body_bytes_read < content_length) {
            uint8_t temp_buffer[4096];
            int32_t to_read = (content_length - body_bytes_read > sizeof(temp_buffer)) ? sizeof(temp_buffer) : (content_length - body_bytes_read);
            int32_t br = tls13_read(ctx, temp_buffer, to_read);
            if(br <= 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to read HTTP request body");
                goto error_cleanup;
            }
            buffer_append_bytes(request->body, temp_buffer, br);
            body_bytes_read += br;
        }
    }

    response = (http_response_t*)memory_malloc(sizeof(http_response_t));
    if(!response) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP response");
        goto error_cleanup;
    }

    if(http_handle(request, response) != 0) {
        PRINTLOG(HTTP, LOG_ERROR, "HTTP handler failed");
        goto error_cleanup;
    }

    // Send response
    buffer_t* header_buffer = buffer_new();

    if(!header_buffer) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP response header buffer");
        goto error_cleanup;
    }

    buffer_printf(header_buffer, "HTTP/1.1 %d OK\r\n", response->status_code);
    // add x-powered-by header
    buffer_printf(header_buffer, "x-powered-by: Turnstone OS\r\n");
    for(size_t i = 0; i < list_size(response->headers); i++) {
        http_header_t* header = (http_header_t*)list_get_data_at_position(response->headers, i);
        buffer_printf(header_buffer, "%s: %s\r\n", header->name, header->value);
    }
    buffer_printf(header_buffer, "\r\n"); // End of headers

    size_t header_data_len = 0;
    uint8_t* header_data = buffer_get_all_bytes_and_destroy(header_buffer, &header_data_len);
    if(tls13_write(ctx, header_data, header_data_len) <= 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP response headers");
        memory_free(header_data);
        goto error_cleanup;
    }
    memory_free(header_data);

    if(response->body && buffer_get_length(response->body) > 0) {
        size_t body_data_len = 0;
        uint8_t* body_data = buffer_get_all_bytes_and_destroy(response->body, &body_data_len);
        uint8_t* original_body_data = body_data;
        response->body = NULL; // prevent double free

        while(body_data_len > 0) {
            int32_t to_write = (body_data_len > 4096) ? 4096 : body_data_len;
            int32_t written  = tls13_write(ctx, body_data, to_write);
            if(written <= 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP response body");
                memory_free(body_data);
                goto error_cleanup;
            }
            body_data += to_write;
            body_data_len -= to_write;
        }

        memory_free(original_body_data);
    }

    ret = 0;
error_cleanup:
    http_free_request(request);
    http_free_response(response);
    return ret;
}
#pragma GCC diagnostic pop
