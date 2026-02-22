/**
 * @file http.64.c
 * @brief HTTP Protocol Implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___HTTP_IMPLEMENTATION

#include <network/http.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.lib.network.http");

typedef struct http_handler_entry_t {
    http_method_t  method;
    char_t*        path;
    http_handler_f handler;
} http_handler_entry_t;

static const char_t*const http_content_type_strings[] = {
    [HTTP_CONTENT_TYPE_TEXT_PLAIN] = "text/plain; charset=UTF-8",
    [HTTP_CONTENT_TYPE_TEXT_HTML]  = "text/html; charset=UTF-8",
    [HTTP_CONTENT_TYPE_APPLICATION_JSON] = "application/json; charset=UTF-8",
    [HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM] = "application/octet-stream",
};

int8_t http_response_set_content_type(http_response_t* response, http_content_type_t content_type) {
    if(!response) {
        return -1;
    }

    response->content_type = content_type;
    return 0;
}

int8_t http_response_set_status_code(http_response_t* response, http_status_code_t status_code) {
    if(!response) {
        return -1;
    }

    response->status_code = status_code;
    return 0;
}

int8_t http_response_write_string(http_response_t* response, const char_t* str) {
    if(!response || !str) {
        return -1;
    }

    if(!response->body) {
        response->body = buffer_new();
        if(!response->body) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for response body");
            return -1;
        }
    }

    return buffer_append_bytes(response->body, (uint8_t*)str, strlen(str)) != NULL ? 0 : -1;
}

static http_handler_f http_find_handler(http_application_context_t* app_ctx, http_method_t method, const char_t* path) {
    if(!app_ctx || !path) {
        return NULL;
    }

    if(!app_ctx->http_handlers) {
        return NULL;
    }

    for(size_t i = 0; i < list_size(app_ctx->http_handlers); i++) {
        http_handler_entry_t* entry = (http_handler_entry_t*)list_get_data_at_position(app_ctx->http_handlers, i);
        if(entry && entry->method == method && strcmp(entry->path, path) == 0) {
            return entry->handler;
        }
    }

    return NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"

int8_t http_add_handler(http_application_context_t* app_ctx, http_method_t method, const char_t* path, http_handler_f handler) {
    if(!app_ctx || !path || !handler) {
        return -1;
    }

    http_handler_entry_t* entry = (http_handler_entry_t*)memory_malloc(sizeof(http_handler_entry_t));
    if(!entry) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP handler entry");
        return -1;
    }

    entry->method = method;
    entry->path = strdup(path);
    entry->handler = handler;

    if(!entry->path) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP handler path");
        memory_free(entry);
        return -1;
    }

    if(!app_ctx->http_handlers) {
        app_ctx->http_handlers = list_create_list();
        if(!app_ctx->http_handlers) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP handlers list");
            memory_free(entry->path);
            memory_free(entry);
            return -1;
        }
    }

    if(list_list_insert(app_ctx->http_handlers, entry) == -1ULL) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to insert HTTP handler into application context handlers list");
        memory_free(entry->path);
        memory_free(entry);
        return -1;
    }

    return 0;
}

int8_t http_response_add_header(http_response_t* response, const char_t* name, const char_t* value) {
    if(!response || !name || !value) {
        return -1;
    }

    if(strcmp(name, "content-type") == 0 || strcmp(name, "Content-Type") == 0) {
        return -1; // content-type should be set using http_response_set_content_type
    }

    if(strcmp(name, "content-length") == 0 || strcmp(name, "Content-Length") == 0) {
        return -1; // content-length should be set automatically based on response body length
    }

    http_header_t* header = (http_header_t*)memory_malloc(sizeof(http_header_t));
    if(!header) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP header");
        return -1;
    }

    header->name  = strdup(name);
    header->value = strdup(value);
    if(!header->name || !header->value) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP header strings");
        memory_free(header->name);
        memory_free(header->value);
        memory_free(header);
        return -1;
    }

    if(list_list_insert(response->headers, header) == -1ULL) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to insert header into response headers list");
        memory_free(header->name);
        memory_free(header->value);
        memory_free(header);
        return -1;
    }

    return 0;
}

int8_t http_handle(http_application_context_t* app_ctx, http_request_t* request, http_response_t* response) {
    if(!app_ctx || !request || !response) {
        PRINTLOG(HTTP, LOG_ERROR, "Invalid arguments to http_handle");
        return -1;
    }

    // Simple handler: respond with 200 OK and a hello message
    response->status_code = 200;
    response->version = request->version;
    response->body = buffer_new();
    if(!response->body) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for response body");
        return -1;
    }

    response->headers = list_create_list();
    if(!response->headers) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for response headers list");
        return -1;
    }

    http_handler_f handler = http_find_handler(app_ctx, request->method, request->path);

    if(handler) {
        if(handler(request, response) != 0) {
            PRINTLOG(HTTP, LOG_ERROR, "HTTP handler for path '%s' returned error", request->path);
            return -1;
        }
    } else {
        response->status_code  = 404;
        response->content_type = HTTP_CONTENT_TYPE_TEXT_HTML;
        const char_t* message = "<html><body><h1>404 Not Found</h1></body></html>\n";
        buffer_append_bytes(response->body, (uint8_t*)message, strlen(message));
    }

    // Add Content-Type header
    http_header_t* content_type_header = (http_header_t*)memory_malloc(sizeof(http_header_t));
    if(!content_type_header) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for Content-Type header");
        return -1;
    }
    content_type_header->name = strdup("content-type");

    if(response->content_type >= HTTP_CONTENT_TYPE_COUNT) {
        response->content_type = HTTP_CONTENT_TYPE_TEXT_PLAIN; // default to text/plain if invalid content type
    }

    content_type_header->value = strdup(http_content_type_strings[response->content_type]);
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

http_application_context_t* http_create_application_context(const char_t* server_host_port) {
    http_application_context_t* app_ctx = (http_application_context_t*)memory_malloc(sizeof(http_application_context_t));
    if(!app_ctx) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP application context");
        return NULL;
    }
    app_ctx->server_host_port = server_host_port;
    return app_ctx;
}

tls13_application_context_t* http_get_tls13_application_context(http_application_context_t* app_ctx) {
    return (tls13_application_context_t*)app_ctx;
}

void http_destroy_application_context(http_application_context_t* app_ctx) {
    if(!app_ctx) {
        return;
    }

    if(app_ctx->http_handlers) {
        size_t handler_count = list_size(app_ctx->http_handlers);
        for(size_t i = 0; i < handler_count; i++) {
            http_handler_entry_t* entry = (http_handler_entry_t*)list_get_data_at_position(app_ctx->http_handlers, i);
            if(entry) {
                memory_free(entry->path);
                memory_free(entry);
            }
        }
        list_destroy(app_ctx->http_handlers);
    }

    memory_free(app_ctx);
}

int8_t http_plaintext_redirect_handler(tls13_application_context_t* app_ctx,
                                       tls13_session_t*             tls13_session,
                                       const uint8_t*               data,
                                       size_t                       data_len,
                                       uint8_t*                     response_buf,
                                       size_t                       response_buf_len,
                                       size_t*                      out_response_buf_len) {
    if(!tls13_session || !data || data_len == 0 || !response_buf || !response_buf_len) {
        return -1;
    }

    http_application_context_t* http_app_ctx = (http_application_context_t*)app_ctx;

    // check for GET request (HTTP)
    if(memory_memcompare(data, "GET ", 4) == 0 // GET
       || (memory_memcompare(data, "HEAD ", 5) == 0) // HEAD
       || (memory_memcompare(data, "POST ", 5) == 0) // POST
       ) {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "Received HTTP request on TLS port, sending 308 redirect to HTTPS");

        // find Host header
        char_t default_host[256];
        memory_memclean(default_host, sizeof(default_host));

        const char_t* default_host_port = http_app_ctx->server_host_port;
        memory_memcopy(default_host_port, default_host, strlen(default_host_port));

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

        if(!redirect_str) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for redirect string");
            return -1;
        }

        if(strlen(redirect_str) > response_buf_len) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Redirect string length exceeds response buffer size");
            memory_free(redirect_str);
            return -1;
        }

        memory_memcopy(redirect_str, response_buf, strlen(redirect_str));
        *out_response_buf_len = strlen(redirect_str);
        PRINTLOG(HTTP, LOG_INFO, "Sent 308 redirect to https://%s/", default_host);

        memory_free(redirect_str);

        return 0;
    }

    PRINTLOG(HTTP, LOG_WARNING, "Received non-HTTP request on TLS port, ignoring");

    return -1;
}

int8_t http_application_handler(tls13_application_context_t* app_ctx, tls13_session_t* tls13_session) {
    if(!app_ctx || !tls13_session) {
        return -1;
    }

    http_application_context_t* http_app_ctx = (http_application_context_t*)app_ctx;

    http_app_ctx->tls13_session = tls13_session;

    if(tls13_has_alpn_h2(tls13_session)) {
        if(http2_handle_connection(http_app_ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/2 connection handling failed");
            return -1;
        }
    } else {
        if(http11_handle_connection(http_app_ctx) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "HTTP/1.1 connection handling failed");
            return -1;
        }
    }

    return 0;
}
