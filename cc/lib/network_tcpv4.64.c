/**
 * @file network_tcpv4.64.c
 * @brief TCPv4 protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_tcpv4.h>
#include <network/network_ipv4.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>
#include <time.h>
#include <logging.h>
#include <hashmap.h>
#include <random.h>
#include <strings.h>

MODULE("turnstone.lib");

hashmap_t* network_tcpv4_listener_ip_map = NULL;

network_tcpv4_listener_t*   network_tcpv4_listener_get(network_ipv4_address_t ip, uint16_t port);
void                        network_tcpv4_listener_add(network_ipv4_address_t ip, uint16_t port);
network_tcpv4_connection_t* network_tcpv4_connection_get(network_ipv4_address_t local_ip, uint16_t local_port, network_ipv4_address_t remote_ip, uint16_t remote_port);
void                        network_tcpv4_connection_add(network_tcpv4_connection_t* connection);
void                        network_tcpv4_connection_del(network_tcpv4_connection_t* connection);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
network_tcpv4_listener_t* network_tcpv4_listener_get(network_ipv4_address_t ip, uint16_t port) {
    if(network_tcpv4_listener_ip_map == NULL) {
        network_tcpv4_listener_ip_map = hashmap_integer(128);
    }

    uint64_t key = ((uint64_t)ip.as_dword << 16) | port;

    network_tcpv4_listener_t* listener = (network_tcpv4_listener_t*)hashmap_get(network_tcpv4_listener_ip_map, (void*)key);

    if(listener == NULL && (port == 7 || port == 80)) { // create dummy listener for echo and http service
        listener = memory_malloc(sizeof(network_tcpv4_listener_t));

        if(listener == NULL) {
            return NULL;
        }

        listener->local_ip = ip;
        listener->local_port = port;
        listener->connections = hashmap_integer(128);

        hashmap_put(network_tcpv4_listener_ip_map, (void*)key, listener);

        return listener;
    }

    return listener;
}

void network_tcpv4_listener_add(network_ipv4_address_t ip, uint16_t port) {
    if(network_tcpv4_listener_ip_map == NULL) {
        network_tcpv4_listener_ip_map = hashmap_integer(128);
    }

    uint64_t key = ((uint64_t)ip.as_dword << 16) | port;

    network_tcpv4_listener_t* listener = memory_malloc(sizeof(network_tcpv4_listener_t));

    if(listener == NULL) {
        return;
    }

    listener->local_ip = ip;
    listener->local_port = port;
    listener->connections = hashmap_integer(128);

    hashmap_put(network_tcpv4_listener_ip_map, (void*)key, listener);
}

network_tcpv4_connection_t* network_tcpv4_connection_get(network_ipv4_address_t local_ip, uint16_t local_port, network_ipv4_address_t remote_ip, uint16_t remote_port) {
    network_tcpv4_listener_t* listener = network_tcpv4_listener_get(local_ip, local_port);

    if(listener == NULL) {
        return NULL;
    }

    uint64_t key = ((uint64_t)remote_ip.as_dword << 16) | remote_port;

    network_tcpv4_connection_t* connection = (network_tcpv4_connection_t*)hashmap_get(listener->connections, (void*)key);

    return connection;
}

void network_tcpv4_connection_add(network_tcpv4_connection_t* connection) {
    network_tcpv4_listener_t* listener = network_tcpv4_listener_get(connection->local_ip, connection->local_port);

    if(listener == NULL) {
        return;
    }

    uint64_t key = ((uint64_t)connection->remote_ip.as_dword << 16) | connection->remote_port;

    hashmap_put(listener->connections, (void*)key, connection);
}

void network_tcpv4_connection_del(network_tcpv4_connection_t* connection) {
    network_tcpv4_listener_t* listener = network_tcpv4_listener_get(connection->local_ip, connection->local_port);

    if(listener == NULL) {
        return;
    }

    uint64_t key = ((uint64_t)connection->remote_ip.as_dword << 16) | connection->remote_port;

    hashmap_delete(listener->connections, (void*)key);

    memory_free(connection);
}

static uint16_t network_tcpv4_generate_checksum(network_tcpv4_header_t* tcpv4_packet, uint16_t packet_len) {
    uint32_t sum = 0;

    sum += BYTE_SWAP16(NETWORK_IPV4_PROTOCOL_TCPV4);
    sum += BYTE_SWAP16(packet_len);

    uint8_t* packet = (uint8_t*)tcpv4_packet;

    boolean_t single_byte = packet_len & 1;
    uint16_t tmp_len = packet_len - single_byte;

    for(uint16_t i = 0; i < tmp_len; i += 2) {
        sum += (packet[i + 1] << 8) | packet[i];
    }

    if(single_byte) {
        sum += packet[packet_len - 1];
    }

    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return sum;
}


static network_tcpv4_header_t* network_tcpv4_create_reset_packet(uint16_t dest_port, uint16_t source_port, uint32_t sequence_number, uint32_t acknowledgement_number) {
    network_tcpv4_header_t* res = memory_malloc(sizeof(network_tcpv4_header_t));

    if(res == NULL) {
        return NULL;
    }

    res->source_port = BYTE_SWAP16(source_port);
    res->destination_port = BYTE_SWAP16(dest_port);
    res->sequence_number = BYTE_SWAP32(sequence_number);
    res->acknowledgement_number = BYTE_SWAP32(acknowledgement_number);
    res->header_length = 5;
    res->rst = 1;
    res->ack = 1;

    res->checksum = network_tcpv4_generate_checksum(res, sizeof(network_tcpv4_header_t));

    return res;
}

static network_tcpv4_header_t* network_tcpv4_create_syn_ack_packet_from_connection(network_tcpv4_connection_t* connection) {
    network_tcpv4_header_t* res = memory_malloc(sizeof(network_tcpv4_header_t));

    if(res == NULL) {
        return NULL;
    }

    res->source_port = BYTE_SWAP16(connection->local_port);
    res->destination_port = BYTE_SWAP16(connection->remote_port);
    res->sequence_number = BYTE_SWAP32(connection->local_sequence_number);
    res->acknowledgement_number = BYTE_SWAP32(connection->remote_sequence_number);
    res->window_size = BYTE_SWAP16(connection->window_size);
    res->header_length = 5;
    res->syn = 1;
    res->ack = 1;

    res->checksum = network_tcpv4_generate_checksum(res, sizeof(network_tcpv4_header_t));

    return res;
}

static network_tcpv4_header_t* network_tcpv4_create_ack_packet_from_connection(network_tcpv4_connection_t* connection) {
    network_tcpv4_header_t* res = memory_malloc(sizeof(network_tcpv4_header_t));

    if(res == NULL) {
        return NULL;
    }

    res->source_port = BYTE_SWAP16(connection->local_port);
    res->destination_port = BYTE_SWAP16(connection->remote_port);
    res->sequence_number = BYTE_SWAP32(connection->local_sequence_number);
    res->acknowledgement_number = BYTE_SWAP32(connection->remote_sequence_number);
    res->window_size = BYTE_SWAP16(connection->window_size);
    res->header_length = 5;
    res->ack = 1;

    res->checksum = network_tcpv4_generate_checksum(res, sizeof(network_tcpv4_header_t));

    return res;
}

static network_tcpv4_header_t* network_tcpv4_create_psh_ack_packet_from_connection(network_tcpv4_connection_t* connection, uint8_t* data, uint16_t data_len) {
    network_tcpv4_header_t* res = memory_malloc(sizeof(network_tcpv4_header_t) + data_len);

    if(res == NULL) {
        return NULL;
    }

    res->source_port = BYTE_SWAP16(connection->local_port);
    res->destination_port = BYTE_SWAP16(connection->remote_port);
    res->sequence_number = BYTE_SWAP32(connection->local_sequence_number);
    res->acknowledgement_number = BYTE_SWAP32(connection->remote_sequence_number);
    res->window_size = BYTE_SWAP16(connection->window_size);
    res->header_length = 5;
    res->ack = 1;
    res->psh = 1;

    uint8_t* t_res = (uint8_t*)res;
    t_res += sizeof(network_tcpv4_header_t);

    memory_memcopy(data, t_res, data_len);

    res->checksum = network_tcpv4_generate_checksum(res, sizeof(network_tcpv4_header_t) + data_len);

    return res;
}

uint8_t* network_tcpv4_process_packet(network_ipv4_address_t dip, network_ipv4_address_t sip, network_tcpv4_header_t* recv_tcpv4_packet, void* network_info, uint16_t packet_len, uint16_t* return_packet_len) {
    UNUSED(network_info);
    UNUSED(return_packet_len);

    if (recv_tcpv4_packet == NULL || return_packet_len == NULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "recv_tcpv4_packet/return_packet_len is NULL");
        return NULL;
    }

    PRINTLOG(NETWORK, LOG_TRACE, "Processing TCPv4 packet");

    uint16_t header_length = recv_tcpv4_packet->header_length * 4;
    uint16_t data_length = packet_len - header_length;

    uint16_t source_port = recv_tcpv4_packet->source_port;
    source_port = BYTE_SWAP16(source_port);

    uint16_t dest_port = recv_tcpv4_packet->destination_port;
    dest_port = BYTE_SWAP16(dest_port);


    uint32_t seq_num = recv_tcpv4_packet->sequence_number;
    seq_num = BYTE_SWAP32(seq_num);


    uint32_t ack_num = recv_tcpv4_packet->acknowledgement_number;
    ack_num = BYTE_SWAP32(ack_num);


    uint16_t window_size = recv_tcpv4_packet->window_size;
    window_size = BYTE_SWAP16(window_size);

    PRINTLOG(NETWORK, LOG_TRACE, "Source IP: %i.%i.%i.%i Destination IP: %i.%i.%i.%i",
             dip.as_bytes[0], dip.as_bytes[1], dip.as_bytes[2], dip.as_bytes[3],
             sip.as_bytes[0], sip.as_bytes[1], sip.as_bytes[2], sip.as_bytes[3]);
    PRINTLOG(NETWORK, LOG_TRACE, "Source port: %i Destination port: %i", source_port, dest_port);
    PRINTLOG(NETWORK, LOG_TRACE, "Sequence number: %i Acknowledgement number: %i", seq_num, ack_num);
    PRINTLOG(NETWORK, LOG_TRACE, "Window size: %i", window_size);
    PRINTLOG(NETWORK, LOG_TRACE, "Data length: %i", data_length);


    if(recv_tcpv4_packet->syn) {
        PRINTLOG(NETWORK, LOG_TRACE, "SYN flag set");
    }

    if(recv_tcpv4_packet->fin) {
        PRINTLOG(NETWORK, LOG_TRACE, "FIN flag set");
    }

    if(recv_tcpv4_packet->ack) {
        PRINTLOG(NETWORK, LOG_TRACE, "ACK flag set");
    }

    if(recv_tcpv4_packet->rst) {
        PRINTLOG(NETWORK, LOG_TRACE, "RST flag set");
    }

    if(recv_tcpv4_packet->psh) {
        PRINTLOG(NETWORK, LOG_TRACE, "PSH flag set");
    }

    if(header_length > sizeof(network_tcpv4_header_t)) {
        PRINTLOG(NETWORK, LOG_TRACE, "Options present");

        uint16_t options_length = header_length - sizeof(network_tcpv4_header_t);

        uint8_t* options = (uint8_t*)recv_tcpv4_packet;
        options += sizeof(network_tcpv4_header_t);

        PRINTLOG(NETWORK, LOG_TRACE, "Options length: %i", options_length);

        uint16_t i = 0;

        while(i < options_length) {
            PRINTLOG(NETWORK, LOG_TRACE, "Option %i: %i", i, options[i]);

            if(options[i] == 0) {
                break;
            } else if(options[i] == 1) {
                PRINTLOG(NETWORK, LOG_TRACE, "NOP option");
                i++;
            } else if(options[i] == 2) {
                PRINTLOG(NETWORK, LOG_TRACE, "MSS option");

                if(options[i + 1] == 4) {
                    uint16_t mss = (options[i + 2] << 8) | options[i + 3];
                    PRINTLOG(NETWORK, LOG_TRACE, "MSS: %i", mss);
                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "Invalid MSS option length");

                    return NULL;
                }

                i += 4;
            } else if(options[i] == 3) {
                PRINTLOG(NETWORK, LOG_TRACE, "Window scale option");

                if(options[i + 1] == 3) {
                    uint8_t window_scale = options[i + 2];
                    PRINTLOG(NETWORK, LOG_TRACE, "Window scale: %i", window_scale);
                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "Invalid window scale option length");

                    return NULL;
                }

                i += 3;
            } else if(options[i] == 4) {
                PRINTLOG(NETWORK, LOG_TRACE, "SACK permitted option");

                if(options[i + 1] == 2) {
                    PRINTLOG(NETWORK, LOG_TRACE, "SACK permitted option length is valid");
                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "Invalid SACK permitted option length");

                    return NULL;
                }

                i += 2;
            } else if(options[i] == 5) {
                PRINTLOG(NETWORK, LOG_TRACE, "SACK option");

                if(options[i + 1] > 2) {
                    uint8_t sack_length = options[i + 1];
                    PRINTLOG(NETWORK, LOG_TRACE, "SACK length: %i", sack_length);

                    uint8_t* sack = &options[i + 2];

                    for(uint8_t j = 0; j < sack_length - 2; j += 8) {
                        uint32_t left_edge = (sack[j] << 24) | (sack[j + 1] << 16) | (sack[j + 2] << 8) | sack[j + 3];
                        uint32_t right_edge = (sack[j + 4] << 24) | (sack[j + 5] << 16) | (sack[j + 6] << 8) | sack[j + 7];

                        PRINTLOG(NETWORK, LOG_TRACE, "SACK block %i: %i-%i", j / 8, left_edge, right_edge);
                    }

                    i += sack_length;
                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "Invalid SACK option length");

                    return NULL;
                }

                i += options[i + 1];
            } else if(options[i] == 8) {
                PRINTLOG(NETWORK, LOG_TRACE, "Timestamp option");

                if(options[i + 1] == 10) {
                    uint32_t timestamp = (options[i + 2] << 24) | (options[i + 3] << 16) | (options[i + 4] << 8) | options[i + 5];
                    uint32_t echo_reply = (options[i + 6] << 24) | (options[i + 7] << 16) | (options[i + 8] << 8) | options[i + 9];

                    PRINTLOG(NETWORK, LOG_TRACE, "Timestamp: %i Echo reply: %i", timestamp, echo_reply);
                } else {
                    PRINTLOG(NETWORK, LOG_TRACE, "Invalid timestamp option length");

                    return NULL;
                }

                i += 10;
            } else {
                PRINTLOG(NETWORK, LOG_TRACE, "Invalid/Unsupported option: %i", options[i]);

                return NULL;
            }
        }
    }

    network_tcpv4_header_t* res = NULL;

    network_tcpv4_connection_t* connection = network_tcpv4_connection_get(dip, dest_port, sip, source_port);


    if(recv_tcpv4_packet->syn) {
        if(connection) {
            // SYN received on an existing connection
            // send RST
            res = network_tcpv4_create_reset_packet(source_port, dest_port, 0, seq_num + 1);

            if(res != NULL) {
                *return_packet_len = sizeof(network_tcpv4_header_t);
                PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

                return (uint8_t*)res;
            }

            return NULL;
        } else {
            // new connection
            connection = memory_malloc(sizeof(network_tcpv4_connection_t));

            if(connection == NULL) {
                return NULL;
            }

            connection->local_ip = dip;
            connection->local_port = dest_port;
            connection->remote_ip = sip;
            connection->remote_port = source_port;
            connection->state = NETWORK_TCP_CONNECTION_STATE_SYN_RECEIVED;

            connection->local_sequence_number = rand();
            connection->remote_sequence_number = seq_num + 1;

            connection->local_acknowledgement_number = ack_num;
            connection->remote_acknowledgement_number = 0;

            connection->window_size = window_size;

            network_tcpv4_connection_add(connection);

            res = network_tcpv4_create_syn_ack_packet_from_connection(connection);

            if(res != NULL) {
                *return_packet_len = sizeof(network_tcpv4_header_t);
                PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

                return (uint8_t*)res;
            }
        }
    }

    if(recv_tcpv4_packet->ack) {
        if(connection) {
            if(connection->state == NETWORK_TCP_CONNECTION_STATE_SYN_RECEIVED) {
                if(ack_num == connection->local_sequence_number + 1) {
                    connection->state = NETWORK_TCP_CONNECTION_STATE_ESTABLISHED;
                }
            }

            connection->remote_sequence_number = seq_num;
            connection->remote_acknowledgement_number = ack_num;

            if(connection->state == NETWORK_TCP_CONNECTION_STATE_ESTABLISHED) {
                if(recv_tcpv4_packet->psh) {
                    // data received
                    PRINTLOG(NETWORK, LOG_TRACE, "Data received");

                    uint8_t* data = (uint8_t*)recv_tcpv4_packet;
                    data += header_length;

                    PRINTLOG(NETWORK, LOG_INFO, "Data(%i): %s", data_length, data);

                    // send ACK

                    connection->local_sequence_number = connection->remote_acknowledgement_number;
                    connection->remote_sequence_number = connection->remote_sequence_number + data_length;

                    uint16_t extra_data_length = 0;

                    if(dest_port == 7) {
                        res = network_tcpv4_create_psh_ack_packet_from_connection(connection, data, data_length);
                        extra_data_length = data_length;
                    } else if(dest_port == 80) {
                        const char* http_response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 12\r\n\r\nHello World!";
                        extra_data_length = strlen(http_response);
                        res = network_tcpv4_create_psh_ack_packet_from_connection(connection, (uint8_t*)http_response, strlen(http_response));
                    } else {
                        res = network_tcpv4_create_ack_packet_from_connection(connection);
                    }

                    if(res != NULL) {
                        *return_packet_len = sizeof(network_tcpv4_header_t) + extra_data_length;
                        PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

                        return (uint8_t*)res;
                    }

                    return NULL;
                } else if(recv_tcpv4_packet->fin && connection->state == NETWORK_TCP_CONNECTION_STATE_ESTABLISHED) {
                    // connection closed
                    PRINTLOG(NETWORK, LOG_INFO, "Connection closed");

                    connection->local_sequence_number = connection->remote_acknowledgement_number;
                    connection->remote_sequence_number = seq_num + 1;

                    connection->state = NETWORK_TCP_CONNECTION_STATE_CLOSE_WAIT;

                    res = network_tcpv4_create_ack_packet_from_connection(connection);

                    if(res != NULL) {
                        *return_packet_len = sizeof(network_tcpv4_header_t);
                        PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

                        return (uint8_t*)res;
                    }

                    return NULL;
                }
            } else if(connection->state == NETWORK_TCP_CONNECTION_STATE_CLOSE_WAIT) {
                network_tcpv4_connection_del(connection);
            }

            return NULL;
        } else {
            // send RST
            res = network_tcpv4_create_reset_packet(source_port, dest_port, 0, seq_num + 1);

            if(res != NULL) {
                *return_packet_len = sizeof(network_tcpv4_header_t);
                PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

                return (uint8_t*)res;
            }

            return NULL;
        }
    }

    if(recv_tcpv4_packet->rst) {
        if(connection) {
            // connection reset
            PRINTLOG(NETWORK, LOG_TRACE, "Connection reset");

            network_tcpv4_connection_del(connection);

            return NULL;
        } else {
            // send RST
            res = network_tcpv4_create_reset_packet(source_port, dest_port, 0, seq_num + 1);

            if(res != NULL) {
                *return_packet_len = sizeof(network_tcpv4_header_t);
                PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

                return (uint8_t*)res;
            }

            return NULL;
        }
    }

    if(recv_tcpv4_packet->fin) {
        if(connection) {
            // connection closed
            PRINTLOG(NETWORK, LOG_TRACE, "Connection closed");

            return NULL;
        } else {
            // send RST
            res = network_tcpv4_create_reset_packet(source_port, dest_port, 0, seq_num + 1);

            if(res != NULL) {
                *return_packet_len = sizeof(network_tcpv4_header_t);
                PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

                return (uint8_t*)res;
            }

            return NULL;
        }
    }


    res = network_tcpv4_create_reset_packet(source_port, dest_port, 0, seq_num + 1);

    if(res != NULL) {
        *return_packet_len = sizeof(network_tcpv4_header_t);
        PRINTLOG(NETWORK, LOG_TRACE, "return packet len: %i", *return_packet_len);

        return (uint8_t*)res;
    }


    return NULL;
}
#pragma GCC diagnostic pop
