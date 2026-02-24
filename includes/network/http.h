/**
 * @file http.h
 * @brief HTTP Protocol Definitions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___HTTP_H
#define ___HTTP_H 0

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <crypto/tls13.h>
#include <buffer.h>
#include <list.h>

typedef enum http_version_t {
    HTTP_VERSION_1_1,
    HTTP_VERSION_2,
} http_version_t;

typedef enum http_method_t {
    HTTP_METHOD_UNKNOWN,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_PATCH,
} http_method_t;

typedef enum http_content_type_t {
    HTTP_CONTENT_TYPE_TEXT_PLAIN,
    HTTP_CONTENT_TYPE_TEXT_HTML,
    HTTP_CONTENT_TYPE_APPLICATION_JSON,
    HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM,
    HTTP_CONTENT_TYPE_COUNT,
} http_content_type_t;

typedef enum http_status_code_t {
    HTTP_STATUS_CODE_OK               = 200,
    HTTP_STATUS_NO_CONTENT            = 204,
    HTTP_STATUS_PARTIAL_CONTENT       = 206,
    HTTP_STATUS_MULTIPLE_CHOICES      = 300,
    HTTP_STATUS_MOVED_PERMANENTLY     = 301,
    HTTP_STATUS_FOUND                 = 302,
    HTTP_STATUS_SEE_OTHER             = 303,
    HTTP_STATUS_NOT_MODIFIED          = 304,
    HTTP_STATUS_BAD_REQUEST           = 400,
    HTTP_STATUS_UNAUTHORIZED          = 401,
    HTTP_STATUS_FORBIDDEN             = 403,
    HTTP_STATUS_NOT_FOUND             = 404,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED       = 501,
    HTTP_STATUS_BAD_GATEWAY           = 502,
    HTTP_STATUS_SERVICE_UNAVAILABLE   = 503,
} http_status_code_t;

typedef struct http_application_context_t http_application_context_t;

typedef struct http_request_t     http_request_t;
typedef struct http_response_t    http_response_t;
typedef struct http_header_t      http_header_t;
typedef struct http_query_param_t http_query_param_t;

typedef int8_t (*http_handler_f)(http_request_t* request, http_response_t* response);

#if defined(___HTTP_IMPLEMENTATION) || defined(___DEPEND_ANALYSIS)

typedef struct http_request_t {
    http_version_t version;
    http_method_t  method;
    char_t         path[1024]; // request path without query string
    list_t*        headers; // list of http_header_t
    list_t*        query_params; // list of http_query_param_t
    buffer_t*      body; // for storing request body
    size_t         content_length;
} http_request_t;

typedef struct http_header_t {
    char_t* name;
    char_t* value;
} http_header_t;

typedef struct http_query_param_t {
    char_t* name;
    char_t* value;
} http_query_param_t;

typedef struct http_response_t {
    http_version_t      version;
    http_status_code_t  status_code;
    http_content_type_t content_type;
    list_t*             headers; // list of http_header_t
    buffer_t*           body; // for storing response body
} http_response_t;


#define HTTP2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define HTTP2_PREFACE_LEN (sizeof(HTTP2_PREFACE) - 1)

typedef enum http2_frame_type_t {
    HTTP2_FRAME_TYPE_DATA          = 0x0,
    HTTP2_FRAME_TYPE_HEADERS       = 0x1,
    HTTP2_FRAME_TYPE_PRIORITY      = 0x2,
    HTTP2_FRAME_TYPE_RST_STREAM    = 0x3,
    HTTP2_FRAME_TYPE_SETTINGS      = 0x4,
    HTTP2_FRAME_TYPE_PUSH_PROMISE  = 0x5,
    HTTP2_FRAME_TYPE_PING          = 0x6,
    HTTP2_FRAME_TYPE_GOAWAY        = 0x7,
    HTTP2_FRAME_TYPE_WINDOW_UPDATE = 0x8,
    HTTP2_FRAME_TYPE_CONTINUATION  = 0x9,
} http2_frame_type_t;

typedef enum http2_flag_t {
    HTTP2_FLAG_END_STREAM  = 0x1,
    HTTP2_FLAG_END_HEADERS = 0x4,
    HTTP2_FLAG_ACK         = 0x1,
    HTTP2_FLAG_PADDED      = 0x8,
    HTTP2_FLAG_PRIORITY    = 0x20,
} http2_flag_t;

typedef enum http2_setting_id_t {
    HTTP2_SETTING_HEADER_TABLE_SIZE      = 0x1,
    HTTP2_SETTING_ENABLE_PUSH            = 0x2,
    HTTP2_SETTING_MAX_CONCURRENT_STREAMS = 0x3,
    HTTP2_SETTING_INITIAL_WINDOW_SIZE    = 0x4,
    HTTP2_SETTING_MAX_FRAME_SIZE         = 0x5,
    HTTP2_SETTING_MAX_HEADER_LIST_SIZE   = 0x6,
} http2_setting_id_t;

typedef enum http2_error_code_t {
    HTTP2_ERROR_NO_ERROR            = 0x0,
    HTTP2_ERROR_PROTOCOL_ERROR      = 0x1,
    HTTP2_ERROR_INTERNAL_ERROR      = 0x2,
    HTTP2_ERROR_FLOW_CONTROL_ERROR  = 0x3,
    HTTP2_ERROR_SETTINGS_TIMEOUT    = 0x4,
    HTTP2_ERROR_STREAM_CLOSED       = 0x5,
    HTTP2_ERROR_FRAME_SIZE_ERROR    = 0x6,
    HTTP2_ERROR_REFUSED_STREAM      = 0x7,
    HTTP2_ERROR_CANCEL              = 0x8,
    HTTP2_ERROR_COMPRESSION_ERROR   = 0x9,
    HTTP2_ERROR_CONNECT_ERROR       = 0xa,
    HTTP2_ERROR_ENHANCE_YOUR_CALM   = 0xb,
    HTTP2_ERROR_INADEQUATE_SECURITY = 0xc,
    HTTP2_ERROR_HTTP_1_1_REQUIRED   = 0xd,
} http2_error_code_t;

typedef struct http2_frame_t {
    uint32_t           length;
    http2_frame_type_t type;
    uint8_t            flags;
    uint32_t           stream_id;
    uint8_t*           payload;
} http2_frame_t;

typedef struct http2_stream_t {
    uint32_t           stream_id;
    boolean_t          active;
    buffer_t*          header_block_buffer; // For storing fragmented header blocks
    http2_error_code_t error_code;
    uint32_t           local_window_size;
    uint32_t           remote_window_size;
    http_request_t*    request;
    http_response_t*   response;
} http2_stream_t;

typedef struct http2_settings_t {
    uint32_t header_table_size;
    uint8_t  enable_push;
    uint32_t max_concurrent_streams;
    uint32_t initial_window_size;
    uint32_t max_frame_size;
    uint32_t max_header_list_size;
} http2_settings_t;

#define HTTP2_MAX_STREAMS 1024

typedef struct http2_context_t {
    http2_settings_t local_settings;
    http2_settings_t remote_settings;
    http2_stream_t*  streams;
    uint32_t         stream_count;
    list_t*          headers_table; // For HPACK header compression
    size_t           headers_table_size;
    list_t*          remote_headers_table; // For HPACK header compression of headers send to client
    size_t           remote_headers_table_size;
} http2_context_t;

typedef struct http_application_context_t {
    const char_t* server_host_port; // e.g., "example.com:443", used for generating redirect URLs in plaintext redirect handler
    list_t*       http_handlers; // List of http_handler_f for handling different routes or methods
} http_application_context_t;

typedef struct http_session_t {
    http_application_context_t* app_ctx;
    tls13_session_t*            tls13_session;
} http_session_t;

int8_t http_handle(http_session_t* http_session, http_request_t* request, http_response_t* response);

void http_free_request(http_request_t* request);
void http_free_response(http_response_t* response);

int8_t http11_handle_connection(http_session_t* http_session);
int8_t http2_handle_connection(http_session_t* http_session);

#endif /* ___HTTP_IMPLEMENTATION */

http_application_context_t* http_create_application_context(const char_t* server_host_port);

tls13_application_context_t* http_get_tls13_application_context(http_application_context_t* app_ctx);

void http_destroy_application_context(http_application_context_t* app_ctx);

int8_t http_plaintext_redirect_handler(tls13_application_context_t* app_ctx,
                                       tls13_session_t*             tls13_session,
                                       const uint8_t*               data,
                                       size_t                       data_len,
                                       uint8_t*                     response_buf,
                                       size_t                       response_buf_len,
                                       size_t*                      out_response_buf_len);

int8_t http_application_handler(tls13_application_context_t* app_ctx, tls13_session_t* tls13_session);

int8_t http_add_handler(http_application_context_t* app_ctx, http_method_t method, const char_t* path, http_handler_f handler);

int8_t http_response_write_string(http_response_t* response, const char_t* str);
int8_t http_response_set_status_code(http_response_t* response, http_status_code_t status_code);
int8_t http_response_add_header(http_response_t* response, const char_t* name, const char_t* value);
int8_t http_response_set_content_type(http_response_t* response, http_content_type_t content_type);

#ifdef __cplusplus
}
#endif

#endif /* ___HTTP_H */
