/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * Simple terminal screen buffer with VT100/xterm escape sequence parsing.
 */

#include "macos_termbuf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Parser states */
enum {
    ST_NORMAL,
    ST_ESC,         /* Received ESC */
    ST_CSI,         /* Received ESC [ */
    ST_CSI_PARAM,   /* Collecting CSI parameters */
    ST_OSC,         /* Operating System Command ESC ] */
    ST_OSC_STRING,  /* Collecting OSC string */
    ST_CHARSET,     /* ESC ( or ESC ) */
};

#define MAX_CSI_PARAMS 16
#define MAX_OSC_LEN 256
#define TAB_WIDTH 8

struct termbuf {
    termbuf_cell_t *cells;
    int cols, rows;
    int cx, cy;             /* cursor position */
    uint8_t cur_attr;
    uint8_t cur_fg, cur_bg;
    int dirty;

    /* Parser state */
    int state;
    int csi_params[MAX_CSI_PARAMS];
    int csi_nparam;
    int csi_private;        /* '?' prefix */
    char osc_buf[MAX_OSC_LEN];
    int osc_len;

    /* Saved cursor */
    int saved_cx, saved_cy;
    uint8_t saved_attr, saved_fg, saved_bg;

    /* Scroll region */
    int scroll_top, scroll_bottom;

    /* Output callback */
    termbuf_output_cb output_cb;
    void *output_ctx;

    /* Modes */
    int mode_autowrap;      /* Auto-wrap at end of line */
    int mode_origin;        /* Origin mode */
    int wrap_pending;       /* Next char should wrap */
    int mode_insert;        /* Insert mode (IRM) */
    int cursor_visible;     /* DECTCEM: cursor visible */
    int cursor_shape;       /* 0=block, 1=beam, 2=underline */
    int bracketed_paste;    /* Bracketed paste mode */
    int mouse_mode;         /* Mouse tracking mode */
    int mouse_encoding;     /* Mouse encoding (default/utf8/sgr) */

    /* Alternate screen buffer */
    termbuf_cell_t *alt_cells;
    int alt_cx, alt_cy;
    uint8_t alt_attr, alt_fg, alt_bg;
    int on_alt_screen;

    /* Scrollback buffer */
    termbuf_cell_t *scrollback;
    int scrollback_max;     /* max lines in scrollback */
    int scrollback_count;   /* current number of scrollback lines */
    int scrollback_head;    /* ring buffer head index */
    int viewport_offset;    /* 0 = at bottom, >0 = scrolled up */

    /* Callbacks */
    termbuf_title_cb title_cb;
    void *title_ctx;
    termbuf_bell_cb bell_cb;
    void *bell_ctx;
    termbuf_log_cb log_cb;
    void *log_ctx;
};

/* Default 256-color palette */
static const uint32_t default_colors[256] = {
    /* Standard 16 colors (RGB) */
    0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5,
    0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00, 0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
    /* 216 color cube (indices 16-231) - filled at init */
    [16 ... 231] = 0,
    /* 24 grayscale (indices 232-255) - filled at init */
    [232 ... 255] = 0,
};

static uint32_t compute_color(int index) {
    if (index < 16) return default_colors[index];
    if (index < 232) {
        /* 6x6x6 color cube */
        int i = index - 16;
        int r = (i / 36) * 51;
        int g = ((i / 6) % 6) * 51;
        int b = (i % 6) * 51;
        return (r << 16) | (g << 8) | b;
    }
    /* Grayscale ramp */
    int v = 8 + (index - 232) * 10;
    return (v << 16) | (v << 8) | v;
}

uint32_t termbuf_default_color(int index) {
    if (index < 0 || index > 255) return 0;
    if (index < 16) return default_colors[index];
    return compute_color(index);
}

static termbuf_cell_t *cell_at(termbuf_t *tb, int col, int row) {
    if (col < 0 || col >= tb->cols || row < 0 || row >= tb->rows) return NULL;
    return &tb->cells[row * tb->cols + col];
}

static void clear_region(termbuf_t *tb, int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2 && y < tb->rows; y++) {
        int start = (y == y1) ? x1 : 0;
        int end = (y == y2) ? x2 : tb->cols - 1;
        for (int x = start; x <= end && x < tb->cols; x++) {
            termbuf_cell_t *c = cell_at(tb, x, y);
            if (c) {
                c->ch = 0;
                c->attr = 0;
                c->fg = 7;  /* default fg: white */
                c->bg = 0;  /* default bg: black */
            }
        }
    }
}

static void scrollback_push_line(termbuf_t *tb, int row) {
    if (!tb->scrollback || tb->scrollback_max <= 0) return;
    /* Copy screen row into scrollback ring buffer */
    int idx = (tb->scrollback_head + tb->scrollback_count) % tb->scrollback_max;
    if (tb->scrollback_count >= tb->scrollback_max) {
        /* Buffer full, overwrite oldest */
        tb->scrollback_head = (tb->scrollback_head + 1) % tb->scrollback_max;
    } else {
        tb->scrollback_count++;
    }
    memcpy(&tb->scrollback[idx * tb->cols],
           &tb->cells[row * tb->cols],
           tb->cols * sizeof(termbuf_cell_t));
}

static void scroll_up(termbuf_t *tb, int top, int bottom, int n) {
    if (n <= 0 || top >= bottom) return;
    if (n > bottom - top + 1) n = bottom - top + 1;

    /* Save lines going off top into scrollback (only if scrolling the whole screen) */
    if (top == 0 && !tb->on_alt_screen) {
        for (int i = 0; i < n; i++) {
            scrollback_push_line(tb, i);
        }
    }

    /* Move lines up */
    for (int y = top; y <= bottom - n; y++) {
        memcpy(&tb->cells[y * tb->cols],
               &tb->cells[(y + n) * tb->cols],
               tb->cols * sizeof(termbuf_cell_t));
    }
    /* Clear bottom lines */
    for (int y = bottom - n + 1; y <= bottom; y++) {
        clear_region(tb, 0, y, tb->cols - 1, y);
    }
}

static void scroll_down(termbuf_t *tb, int top, int bottom, int n) {
    if (n <= 0 || top >= bottom) return;
    if (n > bottom - top + 1) n = bottom - top + 1;

    for (int y = bottom; y >= top + n; y--) {
        memcpy(&tb->cells[y * tb->cols],
               &tb->cells[(y - n) * tb->cols],
               tb->cols * sizeof(termbuf_cell_t));
    }
    for (int y = top; y < top + n; y++) {
        clear_region(tb, 0, y, tb->cols - 1, y);
    }
}

static void cursor_down_scroll(termbuf_t *tb) {
    if (tb->cy == tb->scroll_bottom) {
        scroll_up(tb, tb->scroll_top, tb->scroll_bottom, 1);
    } else if (tb->cy < tb->rows - 1) {
        tb->cy++;
    }
}

static void put_char(termbuf_t *tb, uint32_t ch) {
    if (tb->wrap_pending) {
        tb->cx = 0;
        cursor_down_scroll(tb);
        tb->wrap_pending = 0;
    }

    /* Insert mode: shift characters right */
    if (tb->mode_insert) {
        for (int x = tb->cols - 1; x > tb->cx; x--) {
            tb->cells[tb->cy * tb->cols + x] = tb->cells[tb->cy * tb->cols + x - 1];
        }
    }

    termbuf_cell_t *c = cell_at(tb, tb->cx, tb->cy);
    if (c) {
        c->ch = ch;
        c->attr = tb->cur_attr;
        c->fg = tb->cur_fg;
        c->bg = tb->cur_bg;
    }

    if (tb->cx < tb->cols - 1) {
        tb->cx++;
    } else if (tb->mode_autowrap) {
        tb->wrap_pending = 1;
    }

    /* Auto-scroll to bottom on new output */
    if (tb->viewport_offset > 0) {
        tb->viewport_offset = 0;
    }
    tb->dirty = 1;
}

/* CSI parameter helpers */
static int csi_param(termbuf_t *tb, int idx, int def) {
    if (idx >= tb->csi_nparam || tb->csi_params[idx] <= 0)
        return def;
    return tb->csi_params[idx];
}

static void clamp_cursor(termbuf_t *tb) {
    if (tb->cx < 0) tb->cx = 0;
    if (tb->cx >= tb->cols) tb->cx = tb->cols - 1;
    if (tb->cy < 0) tb->cy = 0;
    if (tb->cy >= tb->rows) tb->cy = tb->rows - 1;
}

static void handle_sgr(termbuf_t *tb) {
    /* SGR - Select Graphic Rendition */
    if (tb->csi_nparam == 0) {
        /* ESC[m = reset */
        tb->cur_attr = 0;
        tb->cur_fg = 7;
        tb->cur_bg = 0;
        return;
    }
    for (int i = 0; i < tb->csi_nparam; i++) {
        int p = tb->csi_params[i];
        if (p == 0) {
            tb->cur_attr = 0; tb->cur_fg = 7; tb->cur_bg = 0;
        } else if (p == 1) {
            tb->cur_attr |= TERMBUF_ATTR_BOLD;
        } else if (p == 4) {
            tb->cur_attr |= TERMBUF_ATTR_UNDERLINE;
        } else if (p == 5) {
            tb->cur_attr |= TERMBUF_ATTR_BLINK;
        } else if (p == 7) {
            tb->cur_attr |= TERMBUF_ATTR_REVERSE;
        } else if (p == 22) {
            tb->cur_attr &= ~TERMBUF_ATTR_BOLD;
        } else if (p == 24) {
            tb->cur_attr &= ~TERMBUF_ATTR_UNDERLINE;
        } else if (p == 25) {
            tb->cur_attr &= ~TERMBUF_ATTR_BLINK;
        } else if (p == 27) {
            tb->cur_attr &= ~TERMBUF_ATTR_REVERSE;
        } else if (p >= 30 && p <= 37) {
            tb->cur_fg = p - 30;
        } else if (p == 38 && i + 2 < tb->csi_nparam && tb->csi_params[i+1] == 5) {
            /* 256-color foreground: ESC[38;5;Nm */
            tb->cur_fg = tb->csi_params[i+2] & 0xFF;
            i += 2;
        } else if (p == 39) {
            tb->cur_fg = 7; /* default fg */
        } else if (p >= 40 && p <= 47) {
            tb->cur_bg = p - 40;
        } else if (p == 48 && i + 2 < tb->csi_nparam && tb->csi_params[i+1] == 5) {
            /* 256-color background: ESC[48;5;Nm */
            tb->cur_bg = tb->csi_params[i+2] & 0xFF;
            i += 2;
        } else if (p == 49) {
            tb->cur_bg = 0; /* default bg */
        } else if (p >= 90 && p <= 97) {
            tb->cur_fg = p - 90 + 8; /* bright foreground */
        } else if (p >= 100 && p <= 107) {
            tb->cur_bg = p - 100 + 8; /* bright background */
        }
    }
}

static void handle_csi(termbuf_t *tb, char cmd) {
    int n, m;

    switch (cmd) {
    case 'A': /* CUU - Cursor Up */
        n = csi_param(tb, 0, 1);
        tb->cy -= n;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'B': /* CUD - Cursor Down */
        n = csi_param(tb, 0, 1);
        tb->cy += n;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'C': /* CUF - Cursor Forward */
        n = csi_param(tb, 0, 1);
        tb->cx += n;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'D': /* CUB - Cursor Back */
        n = csi_param(tb, 0, 1);
        tb->cx -= n;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'E': /* CNL - Cursor Next Line */
        n = csi_param(tb, 0, 1);
        tb->cy += n;
        tb->cx = 0;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'F': /* CPL - Cursor Previous Line */
        n = csi_param(tb, 0, 1);
        tb->cy -= n;
        tb->cx = 0;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'G': /* CHA - Cursor Horizontal Absolute */
        tb->cx = csi_param(tb, 0, 1) - 1;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'H': /* CUP - Cursor Position */
    case 'f':
        tb->cy = csi_param(tb, 0, 1) - 1;
        tb->cx = csi_param(tb, 1, 1) - 1;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'J': /* ED - Erase in Display */
        n = csi_param(tb, 0, 0);
        if (n == 0) {
            /* Clear from cursor to end */
            clear_region(tb, tb->cx, tb->cy, tb->cols - 1, tb->cy);
            if (tb->cy + 1 < tb->rows)
                clear_region(tb, 0, tb->cy + 1, tb->cols - 1, tb->rows - 1);
        } else if (n == 1) {
            /* Clear from start to cursor */
            if (tb->cy > 0)
                clear_region(tb, 0, 0, tb->cols - 1, tb->cy - 1);
            clear_region(tb, 0, tb->cy, tb->cx, tb->cy);
        } else if (n == 2 || n == 3) {
            /* Clear entire screen */
            clear_region(tb, 0, 0, tb->cols - 1, tb->rows - 1);
        }
        break;

    case 'K': /* EL - Erase in Line */
        n = csi_param(tb, 0, 0);
        if (n == 0) {
            clear_region(tb, tb->cx, tb->cy, tb->cols - 1, tb->cy);
        } else if (n == 1) {
            clear_region(tb, 0, tb->cy, tb->cx, tb->cy);
        } else if (n == 2) {
            clear_region(tb, 0, tb->cy, tb->cols - 1, tb->cy);
        }
        break;

    case 'L': /* IL - Insert Lines */
        n = csi_param(tb, 0, 1);
        scroll_down(tb, tb->cy, tb->scroll_bottom, n);
        break;

    case 'M': /* DL - Delete Lines */
        n = csi_param(tb, 0, 1);
        scroll_up(tb, tb->cy, tb->scroll_bottom, n);
        break;

    case 'P': /* DCH - Delete Characters */
        n = csi_param(tb, 0, 1);
        if (n > tb->cols - tb->cx) n = tb->cols - tb->cx;
        for (int x = tb->cx; x < tb->cols - n; x++) {
            tb->cells[tb->cy * tb->cols + x] = tb->cells[tb->cy * tb->cols + x + n];
        }
        clear_region(tb, tb->cols - n, tb->cy, tb->cols - 1, tb->cy);
        break;

    case 'S': /* SU - Scroll Up */
        n = csi_param(tb, 0, 1);
        scroll_up(tb, tb->scroll_top, tb->scroll_bottom, n);
        break;

    case 'T': /* SD - Scroll Down */
        n = csi_param(tb, 0, 1);
        scroll_down(tb, tb->scroll_top, tb->scroll_bottom, n);
        break;

    case 'X': /* ECH - Erase Characters */
        n = csi_param(tb, 0, 1);
        m = tb->cx + n - 1;
        if (m >= tb->cols) m = tb->cols - 1;
        clear_region(tb, tb->cx, tb->cy, m, tb->cy);
        break;

    case 'd': /* VPA - Vertical Position Absolute */
        tb->cy = csi_param(tb, 0, 1) - 1;
        clamp_cursor(tb);
        tb->wrap_pending = 0;
        break;

    case 'h': /* SM - Set Mode */
        if (tb->csi_private) {
            for (int i = 0; i < tb->csi_nparam; i++) {
                n = tb->csi_params[i];
                switch (n) {
                case 1: /* DECCKM - Application cursor keys (handled by keyboard) */ break;
                case 6: tb->mode_origin = 1; break;
                case 7: tb->mode_autowrap = 1; break;
                case 25: tb->cursor_visible = 1; break; /* DECTCEM */
                case 1000: tb->mouse_mode = TERMBUF_MOUSE_VT200; break;
                case 1002: tb->mouse_mode = TERMBUF_MOUSE_BTN_EVENT; break;
                case 1003: tb->mouse_mode = TERMBUF_MOUSE_ALL_EVENT; break;
                case 1005: tb->mouse_encoding = TERMBUF_MOUSE_ENC_UTF8; break;
                case 1006: tb->mouse_encoding = TERMBUF_MOUSE_ENC_SGR; break;
                case 1049: /* Alt screen buffer + save cursor */
                case 47: case 1047:
                    if (!tb->on_alt_screen) {
                        /* Save main screen */
                        tb->alt_cx = tb->cx; tb->alt_cy = tb->cy;
                        tb->alt_attr = tb->cur_attr; tb->alt_fg = tb->cur_fg; tb->alt_bg = tb->cur_bg;
                        if (!tb->alt_cells)
                            tb->alt_cells = calloc(tb->cols * tb->rows, sizeof(termbuf_cell_t));
                        if (tb->alt_cells) {
                            memcpy(tb->alt_cells, tb->cells, tb->cols * tb->rows * sizeof(termbuf_cell_t));
                            /* Clear screen for alt buffer */
                            clear_region(tb, 0, 0, tb->cols - 1, tb->rows - 1);
                            if (n == 1049) { tb->cx = 0; tb->cy = 0; }
                            tb->on_alt_screen = 1;
                        }
                    }
                    break;
                case 2004: tb->bracketed_paste = 1; break;
                }
            }
        } else {
            n = csi_param(tb, 0, 0);
            if (n == 4) tb->mode_insert = 1; /* IRM */
        }
        break;

    case 'l': /* RM - Reset Mode */
        if (tb->csi_private) {
            for (int i = 0; i < tb->csi_nparam; i++) {
                n = tb->csi_params[i];
                switch (n) {
                case 1: break;
                case 6: tb->mode_origin = 0; break;
                case 7: tb->mode_autowrap = 0; break;
                case 25: tb->cursor_visible = 0; break;
                case 1000: case 1002: case 1003: tb->mouse_mode = TERMBUF_MOUSE_NONE; break;
                case 1005: case 1006: tb->mouse_encoding = TERMBUF_MOUSE_ENC_DEFAULT; break;
                case 1049: case 47: case 1047:
                    if (tb->on_alt_screen && tb->alt_cells) {
                        /* Restore main screen */
                        memcpy(tb->cells, tb->alt_cells, tb->cols * tb->rows * sizeof(termbuf_cell_t));
                        if (n == 1049) {
                            tb->cx = tb->alt_cx; tb->cy = tb->alt_cy;
                            tb->cur_attr = tb->alt_attr; tb->cur_fg = tb->alt_fg; tb->cur_bg = tb->alt_bg;
                        }
                        tb->on_alt_screen = 0;
                    }
                    break;
                case 2004: tb->bracketed_paste = 0; break;
                }
            }
        } else {
            n = csi_param(tb, 0, 0);
            if (n == 4) tb->mode_insert = 0;
        }
        break;

    case 'm': /* SGR - Select Graphic Rendition */
        handle_sgr(tb);
        break;

    case 'n': /* DSR - Device Status Report */
        n = csi_param(tb, 0, 0);
        if (n == 6 && tb->output_cb) {
            /* CPR - Cursor Position Report */
            char resp[32];
            int len = snprintf(resp, sizeof(resp), "\033[%d;%dR", tb->cy + 1, tb->cx + 1);
            tb->output_cb(resp, len, tb->output_ctx);
        }
        break;

    case 'r': /* DECSTBM - Set Scrolling Region */
        tb->scroll_top = csi_param(tb, 0, 1) - 1;
        tb->scroll_bottom = csi_param(tb, 1, tb->rows) - 1;
        if (tb->scroll_top < 0) tb->scroll_top = 0;
        if (tb->scroll_bottom >= tb->rows) tb->scroll_bottom = tb->rows - 1;
        if (tb->scroll_top >= tb->scroll_bottom) {
            tb->scroll_top = 0;
            tb->scroll_bottom = tb->rows - 1;
        }
        tb->cx = 0;
        tb->cy = tb->mode_origin ? tb->scroll_top : 0;
        tb->wrap_pending = 0;
        break;

    case '@': /* ICH - Insert Characters */
        n = csi_param(tb, 0, 1);
        if (n > tb->cols - tb->cx) n = tb->cols - tb->cx;
        for (int x = tb->cols - 1; x >= tb->cx + n; x--) {
            tb->cells[tb->cy * tb->cols + x] = tb->cells[tb->cy * tb->cols + x - n];
        }
        clear_region(tb, tb->cx, tb->cy, tb->cx + n - 1, tb->cy);
        break;

    case 'c': /* DA - Device Attributes */
        if (tb->csi_private == 0 && tb->output_cb) {
            /* Primary DA: report VT220 */
            const char *resp = "\033[?62;1;2;6;7;8;9c";
            tb->output_cb(resp, (int)strlen(resp), tb->output_ctx);
        }
        break;

    case 'q': /* DECSCUSR - Set Cursor Style (CSI Ps SP q) */
        /* Note: 'q' with space intermediate - we handle it loosely */
        n = csi_param(tb, 0, 1);
        if (n <= 2) tb->cursor_shape = TERMBUF_CURSOR_BLOCK;
        else if (n <= 4) tb->cursor_shape = TERMBUF_CURSOR_UNDERLINE;
        else tb->cursor_shape = TERMBUF_CURSOR_BEAM;
        break;

    case 't': /* Window manipulation (xterm) */
        /* Ignore most, but handle size queries */
        break;

    default:
        /* Unknown CSI sequence - ignore */
        break;
    }
    tb->dirty = 1;
}

/* Process a single byte through the parser */
static void process_byte(termbuf_t *tb, unsigned char ch) {
    switch (tb->state) {
    case ST_NORMAL:
        if (ch == '\033') {
            tb->state = ST_ESC;
        } else if (ch == '\r') {
            tb->cx = 0;
            tb->wrap_pending = 0;
        } else if (ch == '\n' || ch == '\013' || ch == '\014') {
            cursor_down_scroll(tb);
            tb->wrap_pending = 0;
            tb->dirty = 1;
        } else if (ch == '\b') {
            if (tb->cx > 0) tb->cx--;
            tb->wrap_pending = 0;
        } else if (ch == '\t') {
            int next = ((tb->cx / TAB_WIDTH) + 1) * TAB_WIDTH;
            if (next >= tb->cols) next = tb->cols - 1;
            tb->cx = next;
            tb->wrap_pending = 0;
        } else if (ch == '\a') {
            /* Bell */
            if (tb->bell_cb) tb->bell_cb(tb->bell_ctx);
        } else if (ch == 0x0E || ch == 0x0F) {
            /* SI/SO - character set shift, ignore */
        } else if (ch >= 0x20) {
            put_char(tb, ch);
        }
        break;

    case ST_ESC:
        if (ch == '[') {
            tb->state = ST_CSI;
            tb->csi_nparam = 0;
            tb->csi_private = 0;
            memset(tb->csi_params, 0, sizeof(tb->csi_params));
        } else if (ch == ']') {
            tb->state = ST_OSC;
            tb->osc_len = 0;
        } else if (ch == '(') {
            tb->state = ST_CHARSET;
        } else if (ch == ')') {
            tb->state = ST_CHARSET;
        } else if (ch == 'D') {
            /* IND - Index (cursor down, scroll if needed) */
            cursor_down_scroll(tb);
            tb->state = ST_NORMAL;
        } else if (ch == 'M') {
            /* RI - Reverse Index */
            if (tb->cy == tb->scroll_top) {
                scroll_down(tb, tb->scroll_top, tb->scroll_bottom, 1);
            } else if (tb->cy > 0) {
                tb->cy--;
            }
            tb->state = ST_NORMAL;
        } else if (ch == 'E') {
            /* NEL - Next Line */
            tb->cx = 0;
            cursor_down_scroll(tb);
            tb->state = ST_NORMAL;
        } else if (ch == '7') {
            /* DECSC - Save Cursor */
            tb->saved_cx = tb->cx;
            tb->saved_cy = tb->cy;
            tb->saved_attr = tb->cur_attr;
            tb->saved_fg = tb->cur_fg;
            tb->saved_bg = tb->cur_bg;
            tb->state = ST_NORMAL;
        } else if (ch == '8') {
            /* DECRC - Restore Cursor */
            tb->cx = tb->saved_cx;
            tb->cy = tb->saved_cy;
            tb->cur_attr = tb->saved_attr;
            tb->cur_fg = tb->saved_fg;
            tb->cur_bg = tb->saved_bg;
            clamp_cursor(tb);
            tb->state = ST_NORMAL;
        } else if (ch == 'c') {
            /* RIS - Full Reset */
            int cols = tb->cols, rows = tb->rows;
            termbuf_output_cb cb = tb->output_cb;
            void *ctx = tb->output_ctx;
            memset(tb->cells, 0, cols * rows * sizeof(termbuf_cell_t));
            tb->cx = tb->cy = 0;
            tb->cur_attr = 0;
            tb->cur_fg = 7;
            tb->cur_bg = 0;
            tb->scroll_top = 0;
            tb->scroll_bottom = rows - 1;
            tb->mode_autowrap = 1;
            tb->mode_origin = 0;
            tb->output_cb = cb;
            tb->output_ctx = ctx;
            clear_region(tb, 0, 0, cols - 1, rows - 1);
            tb->state = ST_NORMAL;
        } else {
            tb->state = ST_NORMAL;
        }
        break;

    case ST_CSI:
        if (ch == '?') {
            tb->csi_private = 1;
            tb->state = ST_CSI_PARAM;
        } else if (ch >= '0' && ch <= '9') {
            tb->csi_params[0] = ch - '0';
            tb->csi_nparam = 1;
            tb->state = ST_CSI_PARAM;
        } else if (ch == ';') {
            tb->csi_nparam = 2;
            tb->state = ST_CSI_PARAM;
        } else if (ch >= 0x40 && ch <= 0x7E) {
            /* No params, direct command */
            handle_csi(tb, ch);
            tb->state = ST_NORMAL;
        } else {
            tb->state = ST_NORMAL;
        }
        break;

    case ST_CSI_PARAM:
        if (ch >= '0' && ch <= '9') {
            if (tb->csi_nparam == 0) tb->csi_nparam = 1;
            int idx = tb->csi_nparam - 1;
            if (idx < MAX_CSI_PARAMS) {
                tb->csi_params[idx] = tb->csi_params[idx] * 10 + (ch - '0');
            }
        } else if (ch == ';') {
            if (tb->csi_nparam < MAX_CSI_PARAMS) {
                tb->csi_nparam++;
            }
        } else if (ch >= 0x40 && ch <= 0x7E) {
            handle_csi(tb, ch);
            tb->state = ST_NORMAL;
        } else {
            /* Unknown intermediate byte - abort */
            tb->state = ST_NORMAL;
        }
        break;

    case ST_OSC:
        if (ch >= '0' && ch <= '9') {
            /* Store OSC number as first bytes */
            if (tb->osc_len < MAX_OSC_LEN - 1)
                tb->osc_buf[tb->osc_len++] = ch;
            tb->state = ST_OSC_STRING;
        } else if (ch == ';') {
            if (tb->osc_len < MAX_OSC_LEN - 1)
                tb->osc_buf[tb->osc_len++] = ch;
            tb->state = ST_OSC_STRING;
        } else {
            tb->state = ST_NORMAL;
        }
        break;

    case ST_OSC_STRING:
        if (ch == '\a' || ch == '\033') {
            /* End of OSC string */
            tb->osc_buf[tb->osc_len] = '\0';
            /* Parse OSC: "N;string" where N is the OSC number */
            char *semi = strchr(tb->osc_buf, ';');
            if (semi) {
                int osc_num = atoi(tb->osc_buf);
                char *osc_str = semi + 1;
                if ((osc_num == 0 || osc_num == 2) && tb->title_cb) {
                    tb->title_cb(osc_str, tb->title_ctx);
                }
            }
            if (ch == '\033') {
                /* Might be followed by \\ (ST) - consume it */
            }
            tb->state = ST_NORMAL;
        } else {
            if (tb->osc_len < MAX_OSC_LEN - 1)
                tb->osc_buf[tb->osc_len++] = ch;
        }
        break;

    case ST_CHARSET:
        /* Consume one char (the charset designator) and return to normal */
        tb->state = ST_NORMAL;
        break;
    }
}

/* Handle UTF-8 decoding */
static void process_data(termbuf_t *tb, const char *data, int len) {
    const unsigned char *p = (const unsigned char *)data;
    int i = 0;

    while (i < len) {
        unsigned char ch = p[i];

        /* Control characters always processed directly */
        if (ch < 0x20 || ch == 0x7F || ch == 0x1B ||
            tb->state != ST_NORMAL) {
            process_byte(tb, ch);
            i++;
            continue;
        }

        /* UTF-8 decoding for printable characters */
        uint32_t codepoint;
        int bytes;
        if (ch < 0x80) {
            codepoint = ch;
            bytes = 1;
        } else if ((ch & 0xE0) == 0xC0) {
            codepoint = ch & 0x1F;
            bytes = 2;
        } else if ((ch & 0xF0) == 0xE0) {
            codepoint = ch & 0x0F;
            bytes = 3;
        } else if ((ch & 0xF8) == 0xF0) {
            codepoint = ch & 0x07;
            bytes = 4;
        } else {
            /* Invalid UTF-8 start byte */
            i++;
            continue;
        }

        if (i + bytes > len) break; /* Incomplete sequence */

        int valid = 1;
        for (int j = 1; j < bytes; j++) {
            if ((p[i + j] & 0xC0) != 0x80) { valid = 0; break; }
            codepoint = (codepoint << 6) | (p[i + j] & 0x3F);
        }

        if (valid && codepoint >= 0x20) {
            put_char(tb, codepoint);
        }
        i += bytes;
    }
}

/* Public API */

termbuf_t *termbuf_create(int cols, int rows) {
    termbuf_t *tb = calloc(1, sizeof(termbuf_t));
    if (!tb) return NULL;

    tb->cols = cols;
    tb->rows = rows;
    tb->cells = calloc(cols * rows, sizeof(termbuf_cell_t));
    if (!tb->cells) { free(tb); return NULL; }

    tb->cur_fg = 7;
    tb->cur_bg = 0;
    tb->scroll_top = 0;
    tb->scroll_bottom = rows - 1;
    tb->mode_autowrap = 1;
    tb->cursor_visible = 1;
    tb->cursor_shape = TERMBUF_CURSOR_BLOCK;
    tb->dirty = 1;

    /* Scrollback: default 10000 lines */
    tb->scrollback_max = 10000;
    tb->scrollback = calloc(tb->scrollback_max * cols, sizeof(termbuf_cell_t));

    clear_region(tb, 0, 0, cols - 1, rows - 1);
    return tb;
}

void termbuf_destroy(termbuf_t *tb) {
    if (!tb) return;
    free(tb->cells);
    free(tb->alt_cells);
    free(tb->scrollback);
    free(tb);
}

void termbuf_write(termbuf_t *tb, const char *data, int len) {
    if (!tb || !data || len <= 0) return;
    /* Log raw data if callback set */
    if (tb->log_cb) tb->log_cb(data, len, tb->log_ctx);
    process_data(tb, data, len);
}

void termbuf_set_output_cb(termbuf_t *tb, termbuf_output_cb cb, void *ctx) {
    if (!tb) return;
    tb->output_cb = cb;
    tb->output_ctx = ctx;
}

int termbuf_get_cols(const termbuf_t *tb) { return tb ? tb->cols : 0; }
int termbuf_get_rows(const termbuf_t *tb) { return tb ? tb->rows : 0; }

const termbuf_cell_t *termbuf_get_cell(const termbuf_t *tb, int col, int row) {
    if (!tb || col < 0 || col >= tb->cols || row < 0 || row >= tb->rows) return NULL;
    return &tb->cells[row * tb->cols + col];
}

void termbuf_get_cursor(const termbuf_t *tb, int *col, int *row) {
    if (!tb) return;
    if (col) *col = tb->cx;
    if (row) *row = tb->cy;
}

int termbuf_is_dirty(const termbuf_t *tb) { return tb ? tb->dirty : 0; }
void termbuf_clear_dirty(termbuf_t *tb) { if (tb) tb->dirty = 0; }

void termbuf_resize(termbuf_t *tb, int cols, int rows) {
    if (!tb || cols <= 0 || rows <= 0) return;
    if (cols == tb->cols && rows == tb->rows) return;

    termbuf_cell_t *new_cells = calloc(cols * rows, sizeof(termbuf_cell_t));
    if (!new_cells) return;

    /* Copy existing content */
    int copy_rows = (rows < tb->rows) ? rows : tb->rows;
    int copy_cols = (cols < tb->cols) ? cols : tb->cols;
    for (int y = 0; y < copy_rows; y++) {
        memcpy(&new_cells[y * cols], &tb->cells[y * tb->cols],
               copy_cols * sizeof(termbuf_cell_t));
    }

    free(tb->cells);
    tb->cells = new_cells;

    /* Resize alt screen if it exists */
    if (tb->alt_cells) {
        termbuf_cell_t *new_alt = calloc(cols * rows, sizeof(termbuf_cell_t));
        if (new_alt) {
            for (int y = 0; y < copy_rows; y++) {
                memcpy(&new_alt[y * cols], &tb->alt_cells[y * tb->cols],
                       copy_cols * sizeof(termbuf_cell_t));
            }
            free(tb->alt_cells);
            tb->alt_cells = new_alt;
        }
    }

    /* Reallocate scrollback for new column width */
    if (tb->scrollback && cols != tb->cols) {
        termbuf_cell_t *new_sb = calloc(tb->scrollback_max * cols, sizeof(termbuf_cell_t));
        if (new_sb) {
            /* Copy what fits */
            int copy_sb_cols = (cols < tb->cols) ? cols : tb->cols;
            for (int i = 0; i < tb->scrollback_count; i++) {
                int old_idx = (tb->scrollback_head + i) % tb->scrollback_max;
                memcpy(&new_sb[i * cols],
                       &tb->scrollback[old_idx * tb->cols],
                       copy_sb_cols * sizeof(termbuf_cell_t));
            }
            free(tb->scrollback);
            tb->scrollback = new_sb;
            tb->scrollback_head = 0;
        }
    }

    tb->cols = cols;
    tb->rows = rows;
    tb->scroll_top = 0;
    tb->scroll_bottom = rows - 1;
    if (tb->cx >= cols) tb->cx = cols - 1;
    if (tb->cy >= rows) tb->cy = rows - 1;
    tb->viewport_offset = 0;
    tb->dirty = 1;
}

/* --- Scrollback public API --- */

int termbuf_scrollback_lines(const termbuf_t *tb) {
    return tb ? tb->scrollback_count : 0;
}

const termbuf_cell_t *termbuf_get_scrollback_cell(const termbuf_t *tb, int col, int line) {
    if (!tb || !tb->scrollback || col < 0 || col >= tb->cols ||
        line < 0 || line >= tb->scrollback_count) return NULL;
    /* line 0 = oldest, line scrollback_count-1 = most recent */
    int idx = (tb->scrollback_head + line) % tb->scrollback_max;
    return &tb->scrollback[idx * tb->cols + col];
}

void termbuf_set_scrollback_size(termbuf_t *tb, int max_lines) {
    if (!tb || max_lines < 0) return;
    /* Simplified: just change max, reallocate */
    free(tb->scrollback);
    tb->scrollback_max = max_lines;
    tb->scrollback_count = 0;
    tb->scrollback_head = 0;
    if (max_lines > 0) {
        tb->scrollback = calloc(max_lines * tb->cols, sizeof(termbuf_cell_t));
    } else {
        tb->scrollback = NULL;
    }
}

void termbuf_scroll_viewport(termbuf_t *tb, int delta) {
    if (!tb) return;
    tb->viewport_offset += delta;
    if (tb->viewport_offset < 0) tb->viewport_offset = 0;
    if (tb->viewport_offset > tb->scrollback_count)
        tb->viewport_offset = tb->scrollback_count;
    tb->dirty = 1;
}

int termbuf_get_viewport_offset(const termbuf_t *tb) {
    return tb ? tb->viewport_offset : 0;
}

void termbuf_scroll_to_bottom(termbuf_t *tb) {
    if (!tb) return;
    tb->viewport_offset = 0;
    tb->dirty = 1;
}

/* --- Text extraction --- */

char *termbuf_get_text(const termbuf_t *tb, int col1, int row1, int col2, int row2) {
    if (!tb) return NULL;
    /* Normalize: ensure row1 <= row2 */
    if (row1 > row2 || (row1 == row2 && col1 > col2)) {
        int tc = col1, tr = row1;
        col1 = col2; row1 = row2;
        col2 = tc; row2 = tr;
    }

    /* Allocate generous buffer (4 bytes per char for UTF-8 + newlines) */
    int max_chars = (row2 - row1 + 1) * (tb->cols + 1);
    char *buf = malloc(max_chars * 4 + 1);
    if (!buf) return NULL;

    int pos = 0;
    for (int row = row1; row <= row2 && row < tb->rows; row++) {
        int start_col = (row == row1) ? col1 : 0;
        int end_col = (row == row2) ? col2 : tb->cols - 1;
        if (start_col < 0) start_col = 0;
        if (end_col >= tb->cols) end_col = tb->cols - 1;

        /* Find last non-blank char in this row segment for trimming */
        int last_nonblank = start_col - 1;
        for (int c = end_col; c >= start_col; c--) {
            const termbuf_cell_t *cell = termbuf_get_cell(tb, c, row);
            if (cell && cell->ch != 0 && cell->ch != ' ') {
                last_nonblank = c;
                break;
            }
        }

        for (int c = start_col; c <= last_nonblank; c++) {
            const termbuf_cell_t *cell = termbuf_get_cell(tb, c, row);
            uint32_t ch = (cell && cell->ch) ? cell->ch : ' ';
            /* Encode as UTF-8 */
            if (ch < 0x80) {
                buf[pos++] = (char)ch;
            } else if (ch < 0x800) {
                buf[pos++] = 0xC0 | (ch >> 6);
                buf[pos++] = 0x80 | (ch & 0x3F);
            } else if (ch < 0x10000) {
                buf[pos++] = 0xE0 | (ch >> 12);
                buf[pos++] = 0x80 | ((ch >> 6) & 0x3F);
                buf[pos++] = 0x80 | (ch & 0x3F);
            } else {
                buf[pos++] = 0xF0 | (ch >> 18);
                buf[pos++] = 0x80 | ((ch >> 12) & 0x3F);
                buf[pos++] = 0x80 | ((ch >> 6) & 0x3F);
                buf[pos++] = 0x80 | (ch & 0x3F);
            }
        }
        if (row < row2) buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    return buf;
}

char *termbuf_get_all_text(const termbuf_t *tb) {
    if (!tb) return NULL;
    return termbuf_get_text(tb, 0, 0, tb->cols - 1, tb->rows - 1);
}

/* --- Clear / Reset --- */

void termbuf_clear_screen(termbuf_t *tb) {
    if (!tb) return;
    clear_region(tb, 0, 0, tb->cols - 1, tb->rows - 1);
    tb->cx = 0; tb->cy = 0;
    tb->dirty = 1;
}

void termbuf_clear_buffer(termbuf_t *tb) {
    if (!tb) return;
    termbuf_clear_screen(tb);
    tb->scrollback_count = 0;
    tb->scrollback_head = 0;
    tb->viewport_offset = 0;
}

void termbuf_reset(termbuf_t *tb) {
    if (!tb) return;
    termbuf_clear_buffer(tb);
    tb->cur_attr = 0;
    tb->cur_fg = 7;
    tb->cur_bg = 0;
    tb->scroll_top = 0;
    tb->scroll_bottom = tb->rows - 1;
    tb->mode_autowrap = 1;
    tb->mode_origin = 0;
    tb->mode_insert = 0;
    tb->cursor_visible = 1;
    tb->cursor_shape = TERMBUF_CURSOR_BLOCK;
    tb->bracketed_paste = 0;
    tb->mouse_mode = TERMBUF_MOUSE_NONE;
    tb->mouse_encoding = TERMBUF_MOUSE_ENC_DEFAULT;
    tb->on_alt_screen = 0;
    free(tb->alt_cells);
    tb->alt_cells = NULL;
    tb->state = ST_NORMAL;
}

/* --- Query modes --- */

int termbuf_get_cursor_shape(const termbuf_t *tb) { return tb ? tb->cursor_shape : 0; }
int termbuf_get_mouse_mode(const termbuf_t *tb) { return tb ? tb->mouse_mode : 0; }
int termbuf_get_mouse_encoding(const termbuf_t *tb) { return tb ? tb->mouse_encoding : 0; }
int termbuf_get_bracketed_paste(const termbuf_t *tb) { return tb ? tb->bracketed_paste : 0; }
int termbuf_is_alt_screen(const termbuf_t *tb) { return tb ? tb->on_alt_screen : 0; }
int termbuf_get_cursor_visible(const termbuf_t *tb) { return tb ? tb->cursor_visible : 1; }

/* --- Mouse event reporting --- */

void termbuf_report_mouse(termbuf_t *tb, int button, int x, int y, int pressed, int modifiers) {
    if (!tb || !tb->output_cb || tb->mouse_mode == TERMBUF_MOUSE_NONE) return;

    char buf[32];
    int len = 0;

    if (tb->mouse_encoding == TERMBUF_MOUSE_ENC_SGR) {
        /* SGR encoding: ESC [ < Pb ; Px ; Py M/m */
        int cb = button;
        if (modifiers & 1) cb |= 4;  /* shift */
        if (modifiers & 2) cb |= 8;  /* meta/alt */
        if (modifiers & 4) cb |= 16; /* ctrl */
        len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c",
                       cb, x + 1, y + 1, pressed ? 'M' : 'm');
    } else {
        /* Default encoding */
        int cb = button + 32;
        if (!pressed) cb = 35; /* release */
        if (modifiers & 1) cb |= 4;
        if (modifiers & 2) cb |= 8;
        if (modifiers & 4) cb |= 16;
        len = snprintf(buf, sizeof(buf), "\033[M%c%c%c",
                       cb, x + 33, y + 33);
    }

    if (len > 0) tb->output_cb(buf, len, tb->output_ctx);
}

/* --- Callback setters --- */

void termbuf_set_title_cb(termbuf_t *tb, termbuf_title_cb cb, void *ctx) {
    if (!tb) return;
    tb->title_cb = cb;
    tb->title_ctx = ctx;
}

void termbuf_set_bell_cb(termbuf_t *tb, termbuf_bell_cb cb, void *ctx) {
    if (!tb) return;
    tb->bell_cb = cb;
    tb->bell_ctx = ctx;
}

void termbuf_set_log_cb(termbuf_t *tb, termbuf_log_cb cb, void *ctx) {
    if (!tb) return;
    tb->log_cb = cb;
    tb->log_ctx = ctx;
}
