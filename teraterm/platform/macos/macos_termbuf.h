/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * Terminal screen buffer with VT100/VT220/xterm escape sequence parsing.
 * Used by TTTerminalView to display terminal output on macOS.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Character cell attributes */
#define TERMBUF_ATTR_BOLD      0x01
#define TERMBUF_ATTR_FAINT     0x02
#define TERMBUF_ATTR_UNDERLINE 0x04
#define TERMBUF_ATTR_BLINK     0x08
#define TERMBUF_ATTR_REVERSE   0x10
#define TERMBUF_ATTR_INVISIBLE 0x20
#define TERMBUF_ATTR_STRIKEOUT 0x40
#define TERMBUF_ATTR_ITALIC    0x80

/* Cursor shapes */
#define TERMBUF_CURSOR_BLOCK     0
#define TERMBUF_CURSOR_BEAM      1
#define TERMBUF_CURSOR_UNDERLINE 2

/* Mouse tracking modes */
#define TERMBUF_MOUSE_NONE       0
#define TERMBUF_MOUSE_X10        1
#define TERMBUF_MOUSE_VT200      2
#define TERMBUF_MOUSE_BTN_EVENT  3
#define TERMBUF_MOUSE_ALL_EVENT  4

/* Mouse encoding */
#define TERMBUF_MOUSE_ENC_DEFAULT 0
#define TERMBUF_MOUSE_ENC_UTF8    1
#define TERMBUF_MOUSE_ENC_SGR     2

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

/* Feed raw data from connection */
void termbuf_write(termbuf_t *tb, const char *data, int len);

/* Write to connection (keyboard input) */
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

/* Scrollback buffer */
int termbuf_scrollback_lines(const termbuf_t *tb);
const termbuf_cell_t *termbuf_get_scrollback_cell(const termbuf_t *tb, int col, int line);
void termbuf_set_scrollback_size(termbuf_t *tb, int max_lines);

/* Scroll viewport (positive = scroll up into history, negative = scroll down) */
void termbuf_scroll_viewport(termbuf_t *tb, int delta);
int termbuf_get_viewport_offset(const termbuf_t *tb);
void termbuf_scroll_to_bottom(termbuf_t *tb);

/* Text selection & extraction */
char *termbuf_get_text(const termbuf_t *tb, int col1, int row1, int col2, int row2);
char *termbuf_get_all_text(const termbuf_t *tb);

/* Clear screen / buffer */
void termbuf_clear_screen(termbuf_t *tb);
void termbuf_clear_buffer(termbuf_t *tb);

/* Reset terminal to initial state */
void termbuf_reset(termbuf_t *tb);

/* Query modes */
int termbuf_get_cursor_shape(const termbuf_t *tb);
int termbuf_get_mouse_mode(const termbuf_t *tb);
int termbuf_get_mouse_encoding(const termbuf_t *tb);
int termbuf_get_bracketed_paste(const termbuf_t *tb);
int termbuf_is_alt_screen(const termbuf_t *tb);
int termbuf_get_cursor_visible(const termbuf_t *tb);

/* Mouse event reporting */
void termbuf_report_mouse(termbuf_t *tb, int button, int x, int y, int pressed, int modifiers);

/* Title callback */
typedef void (*termbuf_title_cb)(const char *title, void *ctx);
void termbuf_set_title_cb(termbuf_t *tb, termbuf_title_cb cb, void *ctx);

/* Bell callback */
typedef void (*termbuf_bell_cb)(void *ctx);
void termbuf_set_bell_cb(termbuf_t *tb, termbuf_bell_cb cb, void *ctx);

/* Logging callback - called for every byte received */
typedef void (*termbuf_log_cb)(const char *data, int len, void *ctx);
void termbuf_set_log_cb(termbuf_t *tb, termbuf_log_cb cb, void *ctx);

#ifdef __cplusplus
}
#endif
