/**
 * @file http.64.c
 * @brief HTTP Protocol Implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/http.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.lib.network.http");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t http_handle(http_request_t* request, http_response_t* response) {
    // Simple handler: respond with 200 OK and a hello message
    response->status_code = 200;
    response->version = request->version;
    response->body = buffer_new();
    if(!response->body) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for response body");
        return -1;
    }

    PRINTLOG(HTTP, LOG_INFO, "Handling HTTP request for path: %s", request->path);
    for (size_t i = 0; i < list_size(request->headers); i++) {
        http_header_t* header = (http_header_t*)list_get_data_at_position(request->headers, i);
        PRINTLOG(HTTP, LOG_INFO, "Request Header: %s: %s", header->name, header->value);
    }
    for (size_t i = 0; i < list_size(request->query_params); i++) {
        http_query_param_t* param = (http_query_param_t*)list_get_data_at_position(request->query_params, i);
        PRINTLOG(HTTP, LOG_INFO, "Query Param: %s=%s", param->name, param->value);
    }

    if(strcmp(request->path, "/") == 0 || strcmp(request->path, "/index.html") == 0) {
        const char_t* message = "<html><body><h1>Welcome to the Home Page!</h1></body></html>\n";
        buffer_append_bytes(response->body, (uint8_t*)message, strlen(message));
    } else if(strcmp(request->path, "/hello") == 0) {
        const char_t* message = "<html><body><h1>Hello, World!</h1></body></html>\n";
        buffer_append_bytes(response->body, (uint8_t*)message, strlen(message));
    } else {
        response->status_code = 404;
        const char_t* message = "<html><body><h1>404 Not Found</h1></body></html>\n";
        buffer_append_bytes(response->body, (uint8_t*)message, strlen(message));
    }

    response->headers = list_create_list();
    if(!response->headers) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for response headers list");
        return -1;
    }

    // Add Content-Type header
    http_header_t* content_type_header = (http_header_t*)memory_malloc(sizeof(http_header_t));
    if(!content_type_header) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for Content-Type header");
        return -1;
    }
    content_type_header->name  = strdup("content-type");
    content_type_header->value = strdup("text/html; charset=UTF-8");
    if(!content_type_header->name || !content_type_header->value) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for Content-Type header strings");
        memory_free(content_type_header);
        return -1;
    }

    if(list_list_insert(response->headers, content_type_header) == -1ULL) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to insert Content-Type header into response headers list");
        memory_free(content_type_header->name);
        memory_free(content_type_header->value);
        memory_free(content_type_header);
        return -1;
    }

    // Add Content-Length header
    http_header_t* content_length_header = (http_header_t*)memory_malloc(sizeof(http_header_t));
    if(!content_length_header) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for Content-Length header");
        return -1;
    }
    content_length_header->name  = strdup("content-length");
    content_length_header->value = itoa(buffer_get_length(response->body));
    if(!content_length_header->name || !content_length_header->value) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for Content-Length header name");
        memory_free(content_length_header->name);
        memory_free(content_length_header->value);
        memory_free(content_length_header);
        return -1;
    }

    if(list_list_insert(response->headers, content_length_header) == -1ULL) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to insert Content-Length header into response headers list");
        memory_free(content_length_header->name);
        memory_free(content_length_header->value);
        memory_free(content_length_header);
        return -1;
    }

    return 0;
}
#pragma GCC diagnostic pop

void http_free_request(http_request_t* request) {
    if(!request) {
        return;
    }

    if(request->headers) {
        size_t header_count = list_size(request->headers);
        for(size_t i = 0; i < header_count; i++) {
            http_header_t* header = (http_header_t*)list_get_data_at_position(request->headers, i);
            if(header) {
                memory_free(header->name);
                memory_free(header->value);
                memory_free(header);
            }
        }
        list_destroy(request->headers);
    }

    if(request->query_params) {
        size_t param_count = list_size(request->query_params);
        for(size_t i = 0; i < param_count; i++) {
            http_query_param_t* param = (http_query_param_t*)list_get_data_at_position(request->query_params, i);
            if(param) {
                memory_free(param->name);
                memory_free(param->value);
                memory_free(param);
            }
        }
        list_destroy(request->query_params);
    }

    if(request->body) {
        buffer_destroy(request->body);
    }

    memory_free(request);
}

void http_free_response(http_response_t* response) {
    if(!response) {
        return;
    }

    if(response->headers) {
        size_t header_count = list_size(response->headers);
        for(size_t i = 0; i < header_count; i++) {
            http_header_t* header = (http_header_t*)list_get_data_at_position(response->headers, i);
            if(header) {
                memory_free(header->name);
                memory_free(header->value);
                memory_free(header);
            }
        }
        list_destroy(response->headers);
    }

    if(response->body) {
        buffer_destroy(response->body);
    }

    memory_free(response);
}


int8_t http_plaintext_redirect_handler(tls13_session_t* tls13_session, const uint8_t* data, size_t data_len, uint8_t* response_buf, size_t* response_buf_len) {
    if(!tls13_session || !data || data_len == 0 || !response_buf || !response_buf_len) {
        return -1;
    }

    // check for GET request (HTTP)
    if(memory_memcompare(data, "GET ", 4) == 0 // GET
       || (memory_memcompare(data, "HEAD ", 5) == 0) // HEAD
       || (memory_memcompare(data, "POST ", 5) == 0) // POST
       ) {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Received HTTP request on TLS port, sending 308 redirect to HTTPS");

        // find Host header
        char_t default_host[256];
        memory_memclean(default_host, sizeof(default_host));

        char_t* default_host_port = tls13_get_host_port(tls13_session);
        memory_memcopy(default_host_port, default_host, strlen(default_host_port));
        memory_free(default_host_port);

        char_t* host_header = strstr((char_t*)data, "Host: ");
        if(host_header) {
            char_t* host_end = strstr(host_header, "\r\n");
            if(host_end) {
                size_t host_len = host_end - (host_header + 6);
                if(host_len < sizeof(default_host)) {
                    memory_memcopy(host_header + 6, default_host, host_len);
                    default_host[host_len] = '\0';
                }
            }
        }

        char_t* redirect_str = strprintf(
            "HTTP/1.1 308 Permanent Redirect\r\n"
            "Location: https://%s/\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n",
            default_host
            );

        memory_memcopy(redirect_str, response_buf, strlen(redirect_str));
        *response_buf_len = strlen(redirect_str);
        PRINTLOG(HTTP, LOG_INFO, "Sent 308 redirect to https://%s/", default_host);

        memory_free(redirect_str);

        return 0;
    }

    PRINTLOG(HTTP, LOG_WARNING, "Received non-HTTP request on TLS port, ignoring");

    return -1;
}

int8_t http_application_handler(tls13_session_t* tls13_session) {
    if(!tls13_session) {
        return -1;
    }

    if(tls13_has_alpn_h2(tls13_session)) {
        if(http2_handle_connection(tls13_session) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/2 connection handling failed");
            return -1;
        }
    } else {
        if(http11_handle_connection(tls13_session) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/1.1 connection handling failed");
            return -1;
        }
    }

    return 0;
}
