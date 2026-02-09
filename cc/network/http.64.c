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
