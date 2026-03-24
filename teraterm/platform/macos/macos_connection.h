/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * Unified connection manager for macOS.
 * Wraps serial, TCP, and telnet connections behind a single I/O interface.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void* TTMacConnection;

/* Connection types */
enum {
    TT_CONN_NONE = 0,
    TT_CONN_SERIAL,
    TT_CONN_TCP_RAW,
    TT_CONN_TELNET,
};

/* Serial port parameters */
typedef struct {
    char device[256];
    int  baud_rate;     /* 300..921600 */
    int  data_bits;     /* 5,6,7,8 */
    int  stop_bits;     /* 1,2 */
    int  parity;        /* 0=none, 1=odd, 2=even */
    int  flow_control;  /* 0=none, 1=xon/xoff, 2=rts/cts */
} TTSerialParams;

/* TCP parameters */
typedef struct {
    char host[256];
    int  port;
    int  telnet;        /* 1 = use telnet protocol negotiation */
} TTTcpParams;

/* Open a serial connection */
TTMacConnection tt_conn_open_serial(const TTSerialParams *params);

/* Open a TCP/Telnet connection */
TTMacConnection tt_conn_open_tcp(const TTTcpParams *params);

/* Close connection */
void tt_conn_close(TTMacConnection conn);

/* I/O — returns bytes read/written, 0 = no data available, <0 = error/closed */
int tt_conn_read(TTMacConnection conn, void *buffer, int size);
int tt_conn_write(TTMacConnection conn, const void *data, int len);

/* Query state */
int tt_conn_is_open(TTMacConnection conn);
int tt_conn_get_type(TTMacConnection conn);
const char *tt_conn_get_description(TTMacConnection conn);

/* Send break signal (serial only) */
int tt_conn_send_break(TTMacConnection conn);

/* Get underlying file descriptor (for select/poll) */
int tt_conn_get_fd(TTMacConnection conn);

#ifdef __cplusplus
}
#endif
