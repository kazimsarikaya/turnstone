/**
 * @file http2.64.c
 * @brief HTTP 2 Protocol Implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/http.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.lib.network.http");

static int8_t http2_send_reset_stream(tls13_context_t*   ctx,
                                      uint32_t           stream_id,
                                      http2_error_code_t error_code) {
    uint8_t payload[4] = {
        (error_code >> 24) & 0xFF,
        (error_code >> 16) & 0xFF,
        (error_code >> 8) & 0xFF,
        error_code & 0xFF
    };

    uint8_t frame_header[9] = {
        0x00, 0x00, 0x04, // Length: 4
        HTTP2_FRAME_TYPE_RST_STREAM,
        0x00, // Flags
        (stream_id >> 24) & 0x7F,
        (stream_id >> 16) & 0xFF,
        (stream_id >> 8) & 0xFF,
        stream_id & 0xFF
    };

    if(tls13_write(ctx, frame_header, sizeof(frame_header)) < 0 ||
       tls13_write(ctx, payload, sizeof(payload)) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP/2 RST_STREAM frame");
        return -1;
    }

    return 0;
}

static int8_t http2_send_goaway(tls13_context_t*   ctx,
                                uint32_t           last_stream_id,
                                http2_error_code_t error_code) {
    uint8_t payload[8] = {
        (last_stream_id >> 24) & 0x7F,
        (last_stream_id >> 16) & 0xFF,
        (last_stream_id >> 8) & 0xFF,
        last_stream_id & 0xFF,
        (error_code >> 24) & 0xFF,
        (error_code >> 16) & 0xFF,
        (error_code >> 8) & 0xFF,
        error_code & 0xFF
    };

    uint8_t frame_header[9] = {
        0x00, 0x00, 0x08, // Length: 8
        HTTP2_FRAME_TYPE_GOAWAY,
        0x00, // Flags
        0x00, 0x00, 0x00, 0x00 // Stream Identifier: 0
    };

    if(tls13_write(ctx, frame_header, sizeof(frame_header)) < 0 ||
       tls13_write(ctx, payload, sizeof(payload)) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP/2 GOAWAY frame");
        return -1;
    }

    return 0;
}

static int8_t http2_send_settings_ack(tls13_context_t* ctx) {
    uint8_t settings_ack_frame[9] = {
        0x00, 0x00, 0x00, // Length: 0
        HTTP2_FRAME_TYPE_SETTINGS, // Type: SETTINGS
        HTTP2_FLAG_ACK, // Flags: ACK
        0x00, 0x00, 0x00, 0x00 // Stream Identifier: 0
    };

    if(tls13_write(ctx, settings_ack_frame, sizeof(settings_ack_frame)) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP/2 SETTINGS ACK frame");
        return -1;
    }

    return 0;
}

static int8_t http2_send_ping_ack(tls13_context_t * ctx, http2_frame_t * rx_frame) {
    uint8_t ack_header[9] = {
        0x00, 0x00, 0x08, // Length 8
        0x06, // Type PING
        0x01, // Flag ACK
        0x00, 0x00, 0x00, 0x00 // Stream 0
    };

    if(tls13_write(ctx, ack_header, 9) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send PING ACK frame header");
        return -1;
    }

    if(tls13_write(ctx, rx_frame->payload, 8) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send PING ACK frame payload");
        return -1;
    }

    PRINTLOG(HTTP, LOG_DEBUG, "Sent PING ACK to client");
    return 0;
}

static int8_t http2_parse_settings(http2_context_t* ctx, http2_frame_t* frame) {
    if(frame->length % 6 != 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Invalid SETTINGS frame length");
        return -1;
    }

    if(frame->length == 0) {
        PRINTLOG(HTTP, LOG_DEBUG, "Received empty SETTINGS frame");
        return 0;
    }

    if(!frame->payload) {
        PRINTLOG(HTTP, LOG_ERROR, "SETTINGS frame payload is NULL");
        return -1;
    }

    for(uint32_t i = 0; i < frame->length; i += 6) {
        uint16_t setting_id = (frame->payload[i] << 8) | frame->payload[i + 1];
        uint32_t value = (frame->payload[i + 2] << 24) | (frame->payload[i + 3] << 16) |
                         (frame->payload[i + 4] << 8) | frame->payload[i + 5];

        switch(setting_id) {
        case HTTP2_SETTING_HEADER_TABLE_SIZE:
            ctx->remote_settings.header_table_size = value;
            break;
        case HTTP2_SETTING_ENABLE_PUSH:
            ctx->remote_settings.enable_push = (uint8_t)value;
            break;
        case HTTP2_SETTING_MAX_CONCURRENT_STREAMS:
            ctx->remote_settings.max_concurrent_streams = value;
            break;
        case HTTP2_SETTING_INITIAL_WINDOW_SIZE:
            ctx->remote_settings.initial_window_size = value;
            break;
        case HTTP2_SETTING_MAX_FRAME_SIZE:
            ctx->remote_settings.max_frame_size = value;
            break;
        case HTTP2_SETTING_MAX_HEADER_LIST_SIZE:
            ctx->remote_settings.max_header_list_size = value;
            break;
        default:
            PRINTLOG(HTTP, LOG_WARNING, "Unknown SETTINGS ID: %u", setting_id);
            break;
        }
    }

    return 0;
}


static int8_t http2_send_settings(tls13_context_t* ctx, http2_context_t* http2_ctx) {
    uint8_t settings_payload[128]; // Sufficiently large buffer
    uint32_t offset = 0;

    http2_settings_t* settings = &http2_ctx->local_settings;

    if(settings->header_table_size != 0) {
        settings_payload[offset++] = (HTTP2_SETTING_HEADER_TABLE_SIZE >> 8) & 0xFF;
        settings_payload[offset++] = HTTP2_SETTING_HEADER_TABLE_SIZE & 0xFF;
        settings_payload[offset++] = (settings->header_table_size >> 24) & 0xFF;
        settings_payload[offset++] = (settings->header_table_size >> 16) & 0xFF;
        settings_payload[offset++] = (settings->header_table_size >> 8) & 0xFF;
        settings_payload[offset++] = settings->header_table_size & 0xFF;
    }

    if(settings->enable_push != 0) {
        settings_payload[offset++] = (HTTP2_SETTING_ENABLE_PUSH >> 8) & 0xFF;
        settings_payload[offset++] = HTTP2_SETTING_ENABLE_PUSH & 0xFF;
        settings_payload[offset++] = 0x00;
        settings_payload[offset++] = 0x00;
        settings_payload[offset++] = 0x00;
        settings_payload[offset++] = settings->enable_push;
    }

    if(settings->max_concurrent_streams != 0) {
        settings_payload[offset++] = (HTTP2_SETTING_MAX_CONCURRENT_STREAMS >> 8) & 0xFF;
        settings_payload[offset++] = HTTP2_SETTING_MAX_CONCURRENT_STREAMS & 0xFF;
        settings_payload[offset++] = (settings->max_concurrent_streams >> 24) & 0xFF;
        settings_payload[offset++] = (settings->max_concurrent_streams >> 16) & 0xFF;
        settings_payload[offset++] = (settings->max_concurrent_streams >> 8) & 0xFF;
        settings_payload[offset++] = settings->max_concurrent_streams & 0xFF;
    }

    if(settings->initial_window_size != 0) {
        settings_payload[offset++] = (HTTP2_SETTING_INITIAL_WINDOW_SIZE >> 8) & 0xFF;
        settings_payload[offset++] = HTTP2_SETTING_INITIAL_WINDOW_SIZE & 0xFF;
        settings_payload[offset++] = (settings->initial_window_size >> 24) & 0xFF;
        settings_payload[offset++] = (settings->initial_window_size >> 16) & 0xFF;
        settings_payload[offset++] = (settings->initial_window_size >> 8) & 0xFF;
        settings_payload[offset++] = settings->initial_window_size & 0xFF;
    }

    if(settings->max_frame_size != 0) {
        settings_payload[offset++] = (HTTP2_SETTING_MAX_FRAME_SIZE >> 8) & 0xFF;
        settings_payload[offset++] = HTTP2_SETTING_MAX_FRAME_SIZE & 0xFF;
        settings_payload[offset++] = (settings->max_frame_size >> 24) & 0xFF;
        settings_payload[offset++] = (settings->max_frame_size >> 16) & 0xFF;
        settings_payload[offset++] = (settings->max_frame_size >> 8) & 0xFF;
        settings_payload[offset++] = settings->max_frame_size & 0xFF;
    }

    if(settings->max_header_list_size != 0) {
        settings_payload[offset++] = (HTTP2_SETTING_MAX_HEADER_LIST_SIZE >> 8) & 0xFF;
        settings_payload[offset++] = HTTP2_SETTING_MAX_HEADER_LIST_SIZE & 0xFF;
        settings_payload[offset++] = (settings->max_header_list_size >> 24) & 0xFF;
        settings_payload[offset++] = (settings->max_header_list_size >> 16) & 0xFF;
        settings_payload[offset++] = (settings->max_header_list_size >> 8) & 0xFF;
        settings_payload[offset++] = settings->max_header_list_size & 0xFF;
    }

    uint32_t frame_length = offset;
    uint8_t frame_header[9] = {
        (frame_length >> 16) & 0xFF,
        (frame_length >> 8) & 0xFF,
        frame_length & 0xFF,
        HTTP2_FRAME_TYPE_SETTINGS,
        0x00, // Flags
        0x00, 0x00, 0x00, 0x00 // Stream Identifier: 0
    };

    if(tls13_write(ctx, frame_header, sizeof(frame_header)) < 0 ||
       tls13_write(ctx, settings_payload, frame_length) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP/2 SETTINGS frame");
        return -1;
    }

    return 0;
}

static int8_t http2_parse_window_update(http2_context_t* ctx, http2_frame_t* frame) {
    if(frame->length != 4) {
        PRINTLOG(HTTP, LOG_ERROR, "Invalid WINDOW_UPDATE frame length");
        return -1;
    }

    if(!frame->payload) {
        PRINTLOG(HTTP, LOG_ERROR, "WINDOW_UPDATE frame payload is NULL");
        return -1;
    }

    uint32_t window_size_increment = (frame->payload[0] << 24) | (frame->payload[1] << 16) |
                                     (frame->payload[2] << 8) | frame->payload[3];

    if(window_size_increment == 0) {
        PRINTLOG(HTTP, LOG_ERROR, "WINDOW_UPDATE with zero increment");
        return -1;
    }

    ctx->streams[frame->stream_id].remote_window_size += window_size_increment;

    return 0;
}

static int8_t http2_send_window_update(tls13_context_t* ctx,
                                       http2_context_t* http2_ctx,
                                       uint32_t stream_id, uint32_t window_size_increment) {
    uint8_t payload[4] = {
        (window_size_increment >> 24) & 0x7F,
        (window_size_increment >> 16) & 0xFF,
        (window_size_increment >> 8) & 0xFF,
        window_size_increment & 0xFF
    };

    uint8_t frame_header[9] = {
        0x00, 0x00, 0x04, // Length: 4
        HTTP2_FRAME_TYPE_WINDOW_UPDATE,
        0x00, // Flags
        (stream_id >> 24) & 0x7F,
        (stream_id >> 16) & 0xFF,
        (stream_id >> 8) & 0xFF,
        stream_id & 0xFF
    };

    http2_ctx->streams[0].local_window_size += window_size_increment;

    if(stream_id != 0) {
        http2_ctx->streams[stream_id].local_window_size += window_size_increment;
    }

    if(tls13_write(ctx, frame_header, sizeof(frame_header)) < 0 ||
       tls13_write(ctx, payload, sizeof(payload)) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP/2 WINDOW_UPDATE frame");
        return -1;
    }

    return 0;
}

int8_t http2_hpack_encode_int(buffer_t* buffer, uint32_t value, uint8_t prefix_bits, uint8_t type_bits);
int8_t http2_hpack_encode_literal(http2_context_t* ctx, buffer_t* buffer, const char_t* name, const char_t* value,
                                  bool add_to_dynamic_table);
static int8_t http2_send_response(tls13_context_t* tls_ctx,
                                  http2_context_t* ctx, http2_stream_t* stream) {
    http_response_t* res = stream->response;
    uint32_t stream_id = stream->stream_id;

    // --- 1. ENCODE HEADERS (HPACK) ---
    // We'll use a temporary buffer to build the HPACK payload.
    // For :status, index 8 is ":status: 200", index 14 is ":status: 404".
    buffer_t* hpack_buf = buffer_new();

    switch(res->status_code) {
    case 200:
        http2_hpack_encode_int(hpack_buf, 8, 7, 0x80); // Indexed Header Field (index 8)
        break;
    case 404:
        http2_hpack_encode_int(hpack_buf, 13, 7, 0x80); // Indexed Header Field (index 13)
        break;
    default:
        // For simplicity, encode as Literal Header Field with New Name
        char status_str[4];
        itoa_with_buffer(status_str, res->status_code);
        if(http2_hpack_encode_literal(ctx, hpack_buf, ":status", status_str, false) != 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to encode :status header for HTTP/2 response");
            buffer_destroy(hpack_buf);
            return -1;
        }
        break;
    }

    // add x-powered-by: Turnstone OS header as an example of a custom header
    if(http2_hpack_encode_literal(ctx, hpack_buf, "x-powered-by", "Turnstone OS", true) != 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to encode x-powered-by header for HTTP/2 response");
        buffer_destroy(hpack_buf);
        return -1;
    }

    if(res->headers) {
        for(uint32_t i = 0; i < list_size(res->headers); i++) {
            http_header_t* header = (http_header_t*)list_get_data_at_position(res->headers, i);
            if(header && header->name && header->value) {
                boolean_t add_to_dynamic_table = true; // You can implement logic to decide this based on header name/value

                if(strcmp(header->name, "content-length") == 0) {
                    add_to_dynamic_table = false; // Content-Length can vary widely, so we might choose not to add it to the dynamic table
                }

                if(http2_hpack_encode_literal(ctx, hpack_buf, header->name, header->value, add_to_dynamic_table) != 0) {
                    PRINTLOG(HTTP, LOG_ERROR, "Failed to encode header '%s: %s' for HTTP/2 response",
                             header->name, header->value);
                    buffer_destroy(hpack_buf);
                    return -1;
                }
            }
        }
    }

    // --- 2. SEND HEADERS FRAME ---
    size_t hpack_len = buffer_get_length(hpack_buf);
    uint8_t h_frame[9];
    h_frame[0] = (hpack_len >> 16) & 0xFF;
    h_frame[1] = (hpack_len >> 8) & 0xFF;
    h_frame[2] = hpack_len & 0xFF;
    h_frame[3] = 0x01; // Type: HEADERS
    h_frame[4] = 0x04; // Flag: END_HEADERS (Assume no CONTINUATION)
    h_frame[5] = (stream_id >> 24) & 0x7F;
    h_frame[6] = (stream_id >> 16) & 0xFF;
    h_frame[7] = (stream_id >> 8) & 0xFF;
    h_frame[8] = stream_id & 0xFF;

    tls13_write(tls_ctx, h_frame, 9);
    uint8_t* hpack_data = buffer_get_all_bytes_and_destroy(hpack_buf, &hpack_len);
    tls13_write(tls_ctx, hpack_data, hpack_len);
    memory_free(hpack_data);

    // --- 3. SEND DATA FRAME ---
    // TODO: If body is larger than max frame size or window size, we should chunk it and send multiple DATA frames. For simplicity, we assume it fits in one frame here.
    size_t body_len = res->body ? buffer_get_length(res->body) : 0;

    // Safety check for Flow Control Windows
    if (body_len > (size_t)ctx->streams[0].remote_window_size ||
        body_len > (size_t)stream->remote_window_size) {
        // In a real world, we'd chunk this. For now, we trust the 64KB default.
        PRINTLOG(HTTP, LOG_WARNING, "Body exceeds window, might stall.");
    }

    uint8_t d_frame[9];
    d_frame[0] = (body_len >> 16) & 0xFF;
    d_frame[1] = (body_len >> 8) & 0xFF;
    d_frame[2] = body_len & 0xFF;
    d_frame[3] = 0x00; // Type: DATA
    d_frame[4] = 0x01; // Flag: END_STREAM
    d_frame[5] = (stream_id >> 24) & 0x7F;
    d_frame[6] = (stream_id >> 16) & 0xFF;
    d_frame[7] = (stream_id >> 8) & 0xFF;
    d_frame[8] = stream_id & 0xFF;

    tls13_write(tls_ctx, d_frame, 9);
    if (body_len > 0) {
        uint8_t* body_data = buffer_get_all_bytes(res->body, NULL);
        tls13_write(tls_ctx, body_data, body_len);
        memory_free(body_data);

        // Scientific Rule: Subtract from windows after sending
        ctx->streams[0].remote_window_size -= body_len;
        stream->remote_window_size -= body_len;
    }

    PRINTLOG(HTTP, LOG_INFO, "Response sent on stream %u", stream_id);
    return 0;
}

int8_t http2_hpack_handle_indexed(http2_context_t* ctx, http2_stream_t* stream, uint32_t index);
int8_t http2_hpack_decode_int(const uint8_t* data, uint8_t prefix_bits, uint32_t * result, size_t * consumed);
int8_t http2_hpack_decode_literal(http2_context_t* ctx, http2_stream_t* stream,
                                  uint8_t* data, bool add_to_dynamic_table, size_t* consumed_bytes);
static int8_t http2_parse_headers(tls13_context_t* tls_ctx,
                                  http2_context_t* ctx, http2_frame_t* frame) {
    PRINTLOG(HTTP, LOG_DEBUG, "Received %s frame on stream %u",
             (frame->type == HTTP2_FRAME_TYPE_HEADERS) ? "HEADERS" : "CONTINUATION",
             frame->stream_id);

    if(frame->stream_id == 0) {
        PRINTLOG(HTTP, LOG_ERROR, "HEADERS frame with stream ID 0 is invalid");
        http2_send_goaway(tls_ctx, 0, HTTP2_ERROR_PROTOCOL_ERROR);
        return -1;
    }

    if(0 && frame->payload) {
        // print data with 16 bytes per line for debugging
        printf("HPACK Payload (length %u) flags 0x%x:\n", frame->length, frame->flags);
        for (size_t i = 0; i < frame->length; i++) {
            printf("%02x ", frame->payload[i]);
            if ((i + 1) % 16 == 0) {
                printf("\n");
            }
        }
        printf("\n");
    }

    uint8_t is_end_headers = (frame->flags & HTTP2_FLAG_END_HEADERS) == HTTP2_FLAG_END_HEADERS;
    uint8_t* payload = frame->payload;
    size_t payload_len = frame->length;
    boolean_t is_payload_from_buffer = false;

    // 1. Get or create the stream object
    http2_stream_t* stream = &ctx->streams[frame->stream_id];

    if(frame->type == HTTP2_FRAME_TYPE_HEADERS) {
        if(stream->active) {
            PRINTLOG(HTTP, LOG_ERROR, "Received HEADERS frame for already active stream %u", frame->stream_id);
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_STREAM_CLOSED);
            return -1;
        }

        if(frame->length == 0 || !frame->payload) {
            PRINTLOG(HTTP, LOG_ERROR, "HEADERS frame payload is NULL");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
            return -1;
        }

        if(frame->flags & HTTP2_FLAG_PADDED) {
            if(payload_len < 1) {
                PRINTLOG(HTTP, LOG_ERROR, "Invalid PADDED HEADERS frame: insufficient payload for Pad Length");
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }
            size_t pad_length = payload[0]; // Pad Length is the first byte of the payload

            if (pad_length + 1 > payload_len) {
                PRINTLOG(HTTP, LOG_ERROR, "Invalid PADDED HEADERS frame: Pad Length exceeds payload");
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }

            // Adjust length to exclude padding
            payload += 1; // Move past Pad Length byte
            payload_len -= (1 + pad_length); // Subtract Pad Length byte and padding
        }

        if(frame->flags & HTTP2_FLAG_PRIORITY) {
            if (payload_len < 5) {
                PRINTLOG(HTTP, LOG_ERROR, "Invalid PRIORITY HEADERS frame: insufficient space for priority fields");
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }
            // Skip 5 bytes of priority fields
            payload += 5;
            payload_len -= 5;
        }

        if(!is_end_headers) {
            if(stream->header_block_buffer) {
                PRINTLOG(HTTP, LOG_ERROR, "Received fragmented HEADERS frame but header block buffer already exists for stream %u", frame->stream_id);
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }

            stream->header_block_buffer = buffer_new();
            if (!stream->header_block_buffer) {
                PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for header block buffer on stream %u", frame->stream_id);
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
                return -1;
            }
            // put the initial fragment of the header block into the buffer
            if(!buffer_append_bytes(stream->header_block_buffer, payload, payload_len)) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to append initial HEADERS fragment to buffer for stream %u", frame->stream_id);
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
                return -1;
            }

            stream->active = true; // Mark stream as active to allow CONTINUATION frames to be processed

            PRINTLOG(HTTP, LOG_DEBUG, "Started receiving fragmented HEADERS for stream %u", frame->stream_id);
            return 0; // Wait for CONTINUATION frames
        }

        stream->active = true; // Mark stream as active for unfragmented HEADERS
        PRINTLOG(HTTP, LOG_DEBUG, "Received complete HEADERS frame for stream %u", frame->stream_id);
    } else if(frame->type == HTTP2_FRAME_TYPE_CONTINUATION) {
        if(!stream->active) {
            PRINTLOG(HTTP, LOG_ERROR, "Received CONTINUATION frame for inactive stream %u", frame->stream_id);
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_STREAM_CLOSED);
            return -1;
        }

        if(!stream->header_block_buffer) {
            PRINTLOG(HTTP, LOG_ERROR, "Received CONTINUATION frame but no header block buffer exists for stream %u", frame->stream_id);
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
            return -1;
        }

        size_t current_buffer_len = buffer_get_length(stream->header_block_buffer);
        if(current_buffer_len + payload_len > ctx->local_settings.max_header_list_size) {
            PRINTLOG(HTTP, LOG_ERROR, "Header block size exceeds max header list size for stream %u", frame->stream_id);
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
            return -1;
        }

        if(!buffer_append_bytes(stream->header_block_buffer, payload, payload_len)) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to append CONTINUATION fragment to buffer for stream %u", frame->stream_id);
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }

        if(!is_end_headers) {
            PRINTLOG(HTTP, LOG_DEBUG, "Received CONTINUATION frame for stream %u, waiting for more fragments", frame->stream_id);
            return 0; // Wait for more CONTINUATION frames
        }

        payload = buffer_get_all_bytes_and_destroy(stream->header_block_buffer, &payload_len);
        stream->header_block_buffer = NULL;
        is_payload_from_buffer = true;

        PRINTLOG(HTTP, LOG_DEBUG, "Finished receiving fragmented HEADERS for stream %u", frame->stream_id);

    } else {
        PRINTLOG(HTTP, LOG_ERROR, "Invalid frame type %u for headers parsing on stream %u", frame->type, frame->stream_id);
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_PROTOCOL_ERROR);
        return -1;
    }

    // sanity check
    if(!is_end_headers) {
        PRINTLOG(HTTP, LOG_ERROR, "HEADERS frame without END_HEADERS flag should have been handled as fragmented, but got unfragmented frame on stream %u", frame->stream_id);
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
        return -1;
    }

    if(payload_len == 0 || !payload) {
        PRINTLOG(HTTP, LOG_ERROR, "HEADERS frame has no payload after processing fragments for stream %u", frame->stream_id);
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
        return -1;
    }

    if(!stream->active) {
        PRINTLOG(HTTP, LOG_ERROR, "Stream %u is not active after processing HEADERS frame", frame->stream_id);
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_STREAM_CLOSED);
        return -1;
    }

    if(stream->request) {
        PRINTLOG(HTTP, LOG_ERROR, "Stream %u already has a request object when processing HEADERS frame", frame->stream_id);
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_STREAM_CLOSED);
        return -1;
    }

    stream->request = (http_request_t*)memory_malloc(sizeof(http_request_t));
    if (!stream->request) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP/2 stream request");
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
        return -1;
    }

    stream->request->version = HTTP_VERSION_2;

    stream->request->headers = list_create_list();
    if (!stream->request->headers) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP/2 request headers list");
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
        return -1;
    }

    stream->error_code = HTTP2_ERROR_NO_ERROR;
    stream->stream_id  = frame->stream_id;
    stream->local_window_size  = ctx->local_settings.initial_window_size;
    stream->remote_window_size = ctx->remote_settings.initial_window_size;

    uint8_t* data = payload;
    size_t data_len = payload_len;
    size_t offset = 0;
    size_t consumed_bytes = 0;

    PRINTLOG(HTTP, LOG_DEBUG, "Starting HPACK decoding for stream %u req 0x%p with payload length %llu",
             frame->stream_id, stream->request, payload_len);

    while (offset < data_len) {
        uint8_t first_byte = data[offset];

        if (first_byte & 0x80) {
            // TYPE 1: Indexed Header Field (1xxxxxxx)
            // The index is the remaining 7 bits.
            uint32_t index = first_byte & 0x7F;
            if(http2_hpack_handle_indexed(ctx, stream, index) != 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to handle HPACK indexed header");
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }
            offset += 1;
        }else if ((first_byte & 0xC0) == 0x40) {
            // TYPE 2: Literal Header Field with Incremental Indexing (01xxxxxx)
            // This adds a new entry to the Dynamic Table.
            if(http2_hpack_decode_literal(ctx, stream, &data[offset], true, &consumed_bytes) != 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to decode HPACK literal header with indexing");
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }
            offset += consumed_bytes;
        }else if ((first_byte & 0xF0) == 0x00 || (first_byte & 0xF0) == 0x10) {
            // TYPE 3: Literal Header Field without Indexing (0000xxxx or 0001xxxx)
            // Does NOT add to the Dynamic Table.
            if(http2_hpack_decode_literal(ctx, stream, &data[offset], false, &consumed_bytes) != 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to decode HPACK literal header without indexing");
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }
            offset += consumed_bytes;
        } else if ((first_byte & 0xE0) == 0x20) {
            // TYPE 4: Dynamic Table Size Update (001xxxxx)
            // This updates the size of the Dynamic Table.
            uint32_t new_size;
            size_t consumed;
            if(http2_hpack_decode_int(&data[offset], 5, &new_size, &consumed) != 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to decode HPACK dynamic table size update");
                http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
                return -1;
            }
            ctx->local_settings.header_table_size = new_size;
            offset += consumed;
        } else {
            PRINTLOG(HTTP, LOG_ERROR, "Unknown HPACK header representation in HEADERS frame. First byte: 0x%02x", first_byte);
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_COMPRESSION_ERROR);
            return -1;
        }
    }

    if(payload && is_payload_from_buffer) {
        memory_free(payload);
    }

    // 2. Check if this is the end of the stream (END_STREAM flag)
    if (frame->flags & HTTP2_FLAG_END_STREAM) {
        http_response_t* response = (http_response_t*)memory_malloc(sizeof(http_response_t));
        if (!response) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP response");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }

        stream->response = response;

        if(http_handle(stream->request, response) != 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to handle HTTP/2 request");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }

        if(http2_send_response(tls_ctx, ctx, stream) != 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP/2 response");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;

        }

        http_free_request(stream->request);
        stream->request = NULL;
        http_free_response(stream->response);
        stream->response = NULL;
        stream->active = false;
    } else {
        PRINTLOG(HTTP, LOG_DEBUG, "Received complete headers for stream %u, waiting for DATA frames", frame->stream_id);
    }

    return 0;
}

static int8_t http2_parse_data_frame(tls13_context_t* tls_ctx,
                                     http2_context_t* ctx, http2_frame_t* frame) {
    PRINTLOG(HTTP, LOG_DEBUG, "Received DATA frame on stream %u with length %u", frame->stream_id, frame->length);

    http2_stream_t* stream = &ctx->streams[frame->stream_id];
    if (!stream->active || !stream->request) {
        PRINTLOG(HTTP, LOG_ERROR, "DATA frame received for inactive or non-existent stream %u", frame->stream_id);
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_STREAM_CLOSED);
        return -1;
    }

    if(frame->length == 0) {
        PRINTLOG(HTTP, LOG_WARNING, "DATA frame has zero length");
        return 0;
    }

    if(!frame->payload) {
        PRINTLOG(HTTP, LOG_ERROR, "DATA frame payload is NULL");
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
        return -1;
    }

    if(frame->length > (uint32_t)stream->local_window_size ||
       frame->length > (uint32_t)ctx->streams[0].local_window_size) {
        PRINTLOG(HTTP, LOG_ERROR, "DATA frame exceeds flow control window for stream %u", frame->stream_id);
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_FLOW_CONTROL_ERROR);
        return -1;
    }

    // Append data to request body
    if(!stream->request->body) {
        stream->request->body = buffer_new();
        if(!stream->request->body) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP request body buffer");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }
    }

    if(!buffer_append_bytes(stream->request->body, frame->payload, frame->length)) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to append data to HTTP request body buffer");
        http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
        return -1;
    }

    // Update flow control windows
    ctx->streams[0].local_window_size -= frame->length;
    stream->local_window_size -= frame->length;

    if(stream->local_window_size < (ctx->local_settings.initial_window_size / 2)) {
        uint32_t increment = ctx->local_settings.initial_window_size - stream->local_window_size;
        if(http2_send_window_update(tls_ctx, ctx, frame->stream_id, increment) != 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to send WINDOW_UPDATE for stream %u", frame->stream_id);
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }
    }

    // Check for END_STREAM flag
    if (frame->flags & HTTP2_FLAG_END_STREAM) {
        http_response_t* response = (http_response_t*)memory_malloc(sizeof(http_response_t));
        if (!response) {
            PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP response");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }

        stream->response = response;

        if(http_handle(stream->request, response) != 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to handle HTTP/2 request");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }
        if(http2_send_response(tls_ctx, ctx, stream) != 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to send HTTP/2 response");
            http2_send_reset_stream(tls_ctx, frame->stream_id, HTTP2_ERROR_INTERNAL_ERROR);
            return -1;
        }
        http_free_request(stream->request);
        stream->request = NULL;
        http_free_response(stream->response);
        stream->response = NULL;
        stream->active = false;
    }
    return 0;
}

int8_t http2_apply_header(http_request_t* request, const char_t* name, const char_t* value);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t http2_apply_header(http_request_t* request, const char_t* name, const char_t* value) {
    http_header_t* header = (http_header_t*)memory_malloc(sizeof(http_header_t));
    if (!header) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP header");
        return -1;
    }

    header->name  = strdup(name);
    header->value = strdup(value);

    if (!header->name || !header->value) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP header name/value");
        if (header->name) {
            memory_free(header->name);
        }
        if (header->value) {
            memory_free(header->value);
        }
        memory_free(header);
        return -1;
    }

    if (list_list_insert(request->headers, header) == -1ULL) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to insert HTTP header into request headers list");
        memory_free(header->name);
        memory_free(header->value);
        memory_free(header);
        return -1;
    }

    if(strcmp(name, "content-length") == 0) {
        request->content_length = atou(value);
    } else if(strcmp(name, ":method") == 0) {
        const char_t* method = value;
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
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Unknown HTTP method: %s", method);
            return -1;
        }
    } else if(strcmp(name, ":path") == 0) {
        char_t path[1024];
        memory_memclean(path, sizeof(path));
        memory_memcopy(value, path, strlen(value) + 1);

        // path can have query string, parse and separate it
        size_t query_pos = 0;
        while(path[query_pos] != '\0' && path[query_pos] != '?') {
            query_pos++;
        }

        if(path[query_pos] == '?') {
            request->query_params = list_create_list();
            if(!request->query_params) {
                PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP query params list");
                return -1;
            }

            path[query_pos] = '\0'; // terminate path
            memory_memcopy(path, request->path, query_pos + 1);

            // now parse query string
            size_t qp_start = query_pos + 1;;
            while(path[qp_start] != '\0' && qp_start < sizeof(path)) {
                size_t name_start = qp_start;

                while(path[qp_start] != '=' && path[qp_start] != '&' && path[qp_start] != '\0' && qp_start < sizeof(path)) {
                    qp_start++;
                }

                char_t* q_name = (char_t*)memory_malloc(qp_start - name_start + 1);
                if(!q_name) {
                    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP query param name");
                    return -1;
                }
                memory_memcopy(&path[name_start], q_name, qp_start - name_start);

                q_name[qp_start - name_start] = '\0';

                char_t* q_value = NULL;

                if(path[qp_start] == '=') {
                    qp_start++;
                    size_t value_start = qp_start;

                    while(path[qp_start] != '&' && path[qp_start] != '\0') {
                        qp_start++;
                    }

                    q_value = (char_t*)memory_malloc(qp_start - value_start + 1);
                    if(!q_value) {
                        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP query param value");
                        memory_free(q_name);
                        return -1;
                    }
                    memory_memcopy(&path[value_start], q_value, qp_start - value_start);
                    q_value[qp_start - value_start] = '\0';
                }

                http_query_param_t* param = (http_query_param_t*)memory_malloc(sizeof(http_query_param_t));
                if(!param) {
                    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for HTTP query param");
                    memory_free(q_name);
                    if(q_value) {
                        memory_free(q_value);
                    }
                    return -1;
                }

                param->name  = q_name;
                param->value = q_value;

                if(list_list_insert(request->query_params, param) == -1ULL) {
                    PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to insert HTTP query param into list");
                    memory_free(q_name);
                    if(q_value) {
                        memory_free(q_value);
                    }
                    memory_free(param);
                    return -1;
                }

                if(path[qp_start] == '&') {
                    qp_start++;
                }
            }
        } else {
            memory_memcopy(path, request->path, strlen(path) + 1);
        }
    }

    PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Added HTTP header: %s: %s", name, value);

    return 0;
}
#pragma GCC diagnostic pop

void http2_hpack_free_dynamic_table(http2_context_t* ctx);
int8_t http2_handle_connection(tls13_context_t* ctx) {


    uint8_t preface[HTTP2_PREFACE_LEN];
    if(tls13_read(ctx, preface, sizeof(preface)) < 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to read HTTP/2 preface");
        return -1;
    }

    if(memory_memcompare(preface, HTTP2_PREFACE, sizeof(preface)) != 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Invalid HTTP/2 preface");
        return -1;
    }

    http2_context_t http2_ctx;
    memory_memset(&http2_ctx, 0, sizeof(http2_ctx));

    http2_ctx.streams = (http2_stream_t*)memory_malloc(sizeof(http2_stream_t) * HTTP2_MAX_STREAMS);
    if(!http2_ctx.streams) {
        PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP/2 streams");
        return -1;
    }

    http2_ctx.local_settings.header_table_size = 4096;
    http2_ctx.local_settings.enable_push = 0;
    http2_ctx.local_settings.max_concurrent_streams = 100;
    http2_ctx.local_settings.initial_window_size = 65535;
    http2_ctx.local_settings.max_frame_size = 16384;
    http2_ctx.local_settings.max_header_list_size = 65536;

    if(http2_send_settings(ctx, &http2_ctx) != 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send SETTINGS frame");
        return -1;
    } else {
        PRINTLOG(HTTP, LOG_DEBUG, "Sent SETTINGS frame");
    }

    if(http2_send_window_update(ctx, &http2_ctx, 0, 1 << 20) != 0) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to send initial WINDOW_UPDATE frame");
        return -1;
    } else {
        PRINTLOG(HTTP, LOG_DEBUG, "Sent initial WINDOW_UPDATE frame");
    }

    http2_ctx.headers_table = list_create_list();
    if(!http2_ctx.headers_table) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to create HTTP/2 headers table");
        return -1;
    }
    http2_ctx.headers_table_size = 0;
    http2_ctx.remote_headers_table = list_create_list();
    if(!http2_ctx.remote_headers_table) {
        PRINTLOG(HTTP, LOG_ERROR, "Failed to create HTTP/2 remote headers table");
        return -1;
    }
    http2_ctx.remote_headers_table_size = 0;

    int8_t error_code = 0;

    while(true && error_code == 0) {
        uint8_t header[9];
        if(tls13_read(ctx, header, sizeof(header)) < 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Failed to read HTTP/2 frame header");
            error_code = -1;
            break;
        }

        uint32_t length = (header[0] << 16) | (header[1] << 8) | header[2];
        http2_frame_type_t type = (http2_frame_type_t)header[3];
        uint8_t flags = header[4];
        uint32_t stream_id = ((header[5] & 0x7F) << 24) | (header[6] << 16) | (header[7] << 8) | header[8];

        if(stream_id > 0 && stream_id % 2 == 0) {
            PRINTLOG(HTTP, LOG_ERROR, "Received frame with invalid stream ID %u (must be odd for client-initiated frames)", stream_id);
            http2_send_goaway(ctx, stream_id, HTTP2_ERROR_PROTOCOL_ERROR);
            error_code = -1;
            break;
        }

        uint8_t* payload = NULL;
        if(length > 0) {
            payload = memory_malloc(length);
            if(!payload) {
                PRINTLOG(HTTP, LOG_ERROR, "Memory allocation failed for HTTP/2 frame payload");
                error_code = -1;
                break;
            }

            if(tls13_read(ctx, payload, length) < 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to read HTTP/2 frame payload");
                memory_free(payload);
                error_code = -1;
                break;
            }
        }

        http2_frame_t frame = {
            .length = length,
            .type  = type,
            .flags = flags,
            .stream_id = stream_id,
            .payload = payload
        };

        boolean_t connection_close = false;

        switch(type) {
        case HTTP2_FRAME_TYPE_GOAWAY: {
            PRINTLOG(HTTP, LOG_DEBUG, "Received GOAWAY frame, closing connection");
            if(frame.length >= 8 && frame.payload) {
                uint32_t last_stream_id = (frame.payload[0] << 24) | (frame.payload[1] << 16) |
                                          (frame.payload[2] << 8) | frame.payload[3];
                uint32_t http2_error_code = (frame.payload[4] << 24) | (frame.payload[5] << 16) |
                                            (frame.payload[6] << 8) | frame.payload[7];
                PRINTLOG(HTTP, (error_code == 0?LOG_DEBUG:LOG_ERROR),
                         "GOAWAY last stream ID: %u, error code: %u", last_stream_id, http2_error_code);
            }
            connection_close = true;
            break;
        }
        case HTTP2_FRAME_TYPE_PING:
            if (flags & HTTP2_FLAG_ACK) {
                PRINTLOG(HTTP, LOG_DEBUG, "Received PING ACK");
            } else {
                if(frame.length != 8 || !frame.payload) {
                    PRINTLOG(HTTP, LOG_ERROR, "Invalid PING frame length");
                    http2_send_goaway(ctx, stream_id, HTTP2_ERROR_FRAME_SIZE_ERROR);
                    error_code = -1;
                } else {
                    if(http2_send_ping_ack(ctx, &frame) != 0) {
                        PRINTLOG(HTTP, LOG_ERROR, "Failed to send PING ACK");
                        error_code = -1;
                    } else {
                        PRINTLOG(HTTP, LOG_DEBUG, "Sent PING ACK");
                    }
                }
            }
            break;
        case HTTP2_FRAME_TYPE_RST_STREAM: {
            PRINTLOG(HTTP, LOG_ERROR, "Received RST_STREAM for stream %u", stream_id);
            if(frame.length != 4 || !frame.payload) {
                PRINTLOG(HTTP, LOG_ERROR, "Invalid RST_STREAM frame length");
                http2_send_goaway(ctx, stream_id, HTTP2_ERROR_FRAME_SIZE_ERROR);
                error_code = -1;
            } else {
                error_code = (frame.payload[0] << 24) | (frame.payload[1] << 16) |
                             (frame.payload[2] << 8) | frame.payload[3];
                PRINTLOG(HTTP, LOG_WARNING, "RST_STREAM error code: %u", error_code);
                http2_ctx.streams[stream_id].active = false;
                http_free_request(http2_ctx.streams[stream_id].request);
                http2_ctx.streams[stream_id].request = NULL;
                http_free_response(http2_ctx.streams[stream_id].response);
                http2_ctx.streams[stream_id].response = NULL;
            }
            break;
        }
        case HTTP2_FRAME_TYPE_PRIORITY:
            // We don't support client-initiated PRIORITY frames, so just ignore them.
            PRINTLOG(HTTP, LOG_WARNING, "Received PRIORITY frame (not supported), ignoring");
            break;
        case HTTP2_FRAME_TYPE_PUSH_PROMISE:
            // client-initiated PUSH_PROMISE frames are not valid, so send GOAWAY.
            PRINTLOG(HTTP, LOG_ERROR, "Received invalid PUSH_PROMISE frame from client");
            http2_send_goaway(ctx, stream_id, HTTP2_ERROR_PROTOCOL_ERROR);
            break;
        case HTTP2_FRAME_TYPE_SETTINGS:
            if (flags & HTTP2_FLAG_ACK) {
                // This is the client acknowledging OUR settings.
                // We don't need to send anything back.
                PRINTLOG(HTTP, LOG_DEBUG, "Client ACKed our settings");
            } else {
                // This is the client giving us their settings.
                if(http2_parse_settings(&http2_ctx, &frame) == 0) {
                    if(http2_send_settings_ack(ctx) != 0) {
                        PRINTLOG(HTTP, LOG_ERROR, "Failed to send SETTINGS ACK");
                        error_code = -1;
                    }else {
                        PRINTLOG(HTTP, LOG_DEBUG, "Sent SETTINGS ACK");
                    }
                } else {
                    PRINTLOG(HTTP, LOG_ERROR, "Failed to parse SETTINGS frame");
                    http2_send_goaway(ctx, stream_id, HTTP2_ERROR_PROTOCOL_ERROR);
                    error_code = -1;
                }
            }
            break;
        case HTTP2_FRAME_TYPE_WINDOW_UPDATE:
            if(http2_parse_window_update(&http2_ctx, &frame) != 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to parse WINDOW_UPDATE frame");
            }
            break;
        case HTTP2_FRAME_TYPE_HEADERS:
        case HTTP2_FRAME_TYPE_CONTINUATION:
            if(http2_parse_headers(ctx, &http2_ctx, &frame) != 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to parse CONTINUATION frame");
            }
            break;
        case HTTP2_FRAME_TYPE_DATA:
            if(http2_parse_data_frame(ctx, &http2_ctx, &frame) != 0) {
                PRINTLOG(HTTP, LOG_ERROR, "Failed to parse DATA frame");
            }
            break;
        default:
            PRINTLOG(HTTP, LOG_WARNING, "Received unknown HTTP/2 frame type %u", type);
            http2_send_goaway(ctx, stream_id, HTTP2_ERROR_PROTOCOL_ERROR);
            break;
        }

        memory_free(payload);

        if(connection_close) {
            PRINTLOG(HTTP, LOG_DEBUG, "Closing HTTP/2 connection as requested");
            break;
        }
    }

    http2_hpack_free_dynamic_table(&http2_ctx);

    for(uint32_t i = 0; i < HTTP2_MAX_STREAMS; i++) {
        http2_stream_t* stream = &http2_ctx.streams[i];
        if(stream->request) {
            http_free_request(stream->request);
            stream->request = NULL;
        }
        if(stream->response) {
            http_free_response(stream->response);
            stream->response = NULL;
        }
    }

    memory_free(http2_ctx.streams);

    return error_code;
}

