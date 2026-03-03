/**
 * @file network_echo.64.c
 * @brief Network echo protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_echo.h>
#include <network/network_connection.h>
#include <logging.h>
#include <cpu/task.h>

MODULE("turnstone.lib.network");

static void* network_echo_server_args[1] = {0};

static int8_t network_echo_server(int32_t argc, void** argv) {
    if(argc < 1) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info argument is missing");
        return -1;
    }


    const network_info_t* ni = argv[0];

    if(!ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info is NULL");
        return -1;
    }

    network_listener_t* echo_listener = network_listener_create_tcpv4_server(ni, ni->ipv4_address,
                                                                             NETWORK_APPLICATION_PORT_ECHO_SERVER);

    if(!echo_listener) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create echo listener");
        return -1;
    }

    while(true) {
        network_connection_t* connection = network_connection_accept(echo_listener);

        if(!connection) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to accept echo connection");
            continue;
        }

        uint8_t buffer[1024];
        int32_t received_len = network_connection_receive(connection, buffer, sizeof(buffer));

        if(received_len < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to receive data from echo connection");
            network_connection_close(connection);
            continue;
        }

        if(network_connection_send(connection, buffer, received_len) < 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to send data to echo connection");
        }

        network_connection_close(connection);
        network_connection_destroy(connection);
    }

    network_listener_destroy(echo_listener);

    return 0;
}

int8_t network_echo_init(const network_info_t* ni) {
    if(!ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info is NULL");
        return -1;
    }

    network_echo_server_args[0] = (void*)ni;

    if(task_create_task(NULL, 2 << 20, 64 << 10, network_echo_server, 1, network_echo_server_args, "echo server") == -1ULL) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create network echo server task");
        return -1;
    }

    return 0;
}
