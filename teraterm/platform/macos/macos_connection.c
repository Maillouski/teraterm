/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * Unified connection manager for macOS.
 */

#include "macos_connection.h"
#include "macos_serial.h"
#include "macos_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
    int type;               /* TT_CONN_SERIAL, TT_CONN_TCP_RAW, TT_CONN_TELNET */
    char description[512];

    /* Serial */
    TTMacSerial serial;

    /* TCP */
    int sock;
    int telnet_mode;
} TTMacConnectionImpl;

TTMacConnection tt_conn_open_serial(const TTSerialParams *params) {
    if (!params || !params->device[0]) return NULL;

    TTMacSerial serial = tt_mac_serial_open(params->device);
    if (!serial) return NULL;

    /* Configure port */
    tt_mac_serial_set_baud(serial, params->baud_rate > 0 ? params->baud_rate : 9600);
    tt_mac_serial_set_params(serial, params->data_bits > 0 ? params->data_bits : 8,
                              params->stop_bits > 0 ? params->stop_bits : 1,
                              params->parity);
    tt_mac_serial_set_flow_control(serial, params->flow_control);

    TTMacConnectionImpl *impl = calloc(1, sizeof(TTMacConnectionImpl));
    if (!impl) { tt_mac_serial_close(serial); return NULL; }

    impl->type = TT_CONN_SERIAL;
    impl->serial = serial;
    impl->sock = -1;
    snprintf(impl->description, sizeof(impl->description),
             "%s (%d baud)", params->device, params->baud_rate > 0 ? params->baud_rate : 9600);

    return (TTMacConnection)impl;
}

TTMacConnection tt_conn_open_tcp(const TTTcpParams *params) {
    if (!params || !params->host[0] || params->port <= 0) return NULL;

    int sock = tt_mac_tcp_connect_timeout(params->host, params->port, 10000);
    if (sock < 0) return NULL;

    /* Set non-blocking for async I/O */
    tt_mac_socket_set_nonblocking(sock, 1);
    tt_mac_socket_set_nodelay(sock, 1);
    tt_mac_socket_set_keepalive(sock, 1);

    TTMacConnectionImpl *impl = calloc(1, sizeof(TTMacConnectionImpl));
    if (!impl) { tt_mac_tcp_disconnect(sock); return NULL; }

    impl->type = params->telnet ? TT_CONN_TELNET : TT_CONN_TCP_RAW;
    impl->serial = NULL;
    impl->sock = sock;
    impl->telnet_mode = params->telnet;
    snprintf(impl->description, sizeof(impl->description),
             "%s:%d (%s)", params->host, params->port,
             params->telnet ? "Telnet" : "TCP");

    return (TTMacConnection)impl;
}

void tt_conn_close(TTMacConnection conn) {
    if (!conn) return;
    TTMacConnectionImpl *impl = (TTMacConnectionImpl *)conn;

    if (impl->serial) {
        tt_mac_serial_close(impl->serial);
        impl->serial = NULL;
    }
    if (impl->sock >= 0) {
        tt_mac_tcp_disconnect(impl->sock);
        impl->sock = -1;
    }
    impl->type = TT_CONN_NONE;
    free(impl);
}

int tt_conn_read(TTMacConnection conn, void *buffer, int size) {
    if (!conn) return -1;
    TTMacConnectionImpl *impl = (TTMacConnectionImpl *)conn;

    if (impl->type == TT_CONN_SERIAL && impl->serial) {
        return tt_mac_serial_read(impl->serial, buffer, size);
    }
    if ((impl->type == TT_CONN_TCP_RAW || impl->type == TT_CONN_TELNET) && impl->sock >= 0) {
        int n = tt_mac_socket_recv(impl->sock, buffer, size);
        if (n < 0) {
            /* EAGAIN/EWOULDBLOCK is normal for non-blocking */
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (n == 0) return -1; /* Connection closed */
        return n;
    }
    return -1;
}

int tt_conn_write(TTMacConnection conn, const void *data, int len) {
    if (!conn) return -1;
    TTMacConnectionImpl *impl = (TTMacConnectionImpl *)conn;

    if (impl->type == TT_CONN_SERIAL && impl->serial) {
        return tt_mac_serial_write(impl->serial, data, len);
    }
    if ((impl->type == TT_CONN_TCP_RAW || impl->type == TT_CONN_TELNET) && impl->sock >= 0) {
        return tt_mac_socket_send(impl->sock, data, len);
    }
    return -1;
}

int tt_conn_is_open(TTMacConnection conn) {
    if (!conn) return 0;
    TTMacConnectionImpl *impl = (TTMacConnectionImpl *)conn;
    if (impl->type == TT_CONN_SERIAL && impl->serial) return 1;
    if ((impl->type == TT_CONN_TCP_RAW || impl->type == TT_CONN_TELNET) && impl->sock >= 0) return 1;
    return 0;
}

int tt_conn_get_type(TTMacConnection conn) {
    if (!conn) return TT_CONN_NONE;
    return ((TTMacConnectionImpl *)conn)->type;
}

const char *tt_conn_get_description(TTMacConnection conn) {
    if (!conn) return "Not connected";
    return ((TTMacConnectionImpl *)conn)->description;
}

int tt_conn_send_break(TTMacConnection conn) {
    if (!conn) return -1;
    TTMacConnectionImpl *impl = (TTMacConnectionImpl *)conn;
    if (impl->type == TT_CONN_SERIAL && impl->serial)
        return tt_mac_serial_send_break(impl->serial);
    return -1;
}

int tt_conn_get_fd(TTMacConnection conn) {
    if (!conn) return -1;
    TTMacConnectionImpl *impl = (TTMacConnectionImpl *)conn;
    if (impl->type == TT_CONN_SERIAL && impl->serial) {
        /* Get fd from serial - we know the internal structure */
        return -1; /* Would need API extension; use polling for now */
    }
    if (impl->sock >= 0) return impl->sock;
    return -1;
}
