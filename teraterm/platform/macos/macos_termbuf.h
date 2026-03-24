/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * Simple terminal screen buffer with basic VT100 escape sequence parsing.
 * Used by TTTerminalView to display terminal output on macOS.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Character cell attributes */
#define TERMBUF_ATTR_BOLD      0x01
#define TERMBUF_ATTR_UNDERLINE 0x04
#define TERMBUF_ATTR_BLINK     0x08
#define TERMBUF_ATTR_REVERSE   0x10

typedef struct {
    uint32_t ch;        /* Unicode codepoint (0 = empty) */
    uint8_t  attr;      /* Character attributes */
    uint8_t  fg;        /* Foreground color index (0-255) */
    uint8_t  bg;        /* Background color index (0-255) */
} termbuf_cell_t;

typedef struct termbuf termbuf_t;

/* Create/destroy */
termbuf_t *termbuf_create(int cols, int rows);
void termbuf_destroy(termbuf_t *tb);

/* Feed raw data from PTY */
void termbuf_write(termbuf_t *tb, const char *data, int len);

/* Write to PTY (keyboard input) */
typedef void (*termbuf_output_cb)(const char *data, int len, void *ctx);
void termbuf_set_output_cb(termbuf_t *tb, termbuf_output_cb cb, void *ctx);

/* Access screen buffer */
int termbuf_get_cols(const termbuf_t *tb);
int termbuf_get_rows(const termbuf_t *tb);
const termbuf_cell_t *termbuf_get_cell(const termbuf_t *tb, int col, int row);
void termbuf_get_cursor(const termbuf_t *tb, int *col, int *row);
int termbuf_is_dirty(const termbuf_t *tb);
void termbuf_clear_dirty(termbuf_t *tb);

/* Resize */
void termbuf_resize(termbuf_t *tb, int cols, int rows);

/* Default ANSI color palette (RGB values) */
uint32_t termbuf_default_color(int index);

#ifdef __cplusplus
}
#endif
