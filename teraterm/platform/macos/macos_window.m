/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * macOS native window implementation using Cocoa/AppKit.
 */

#import <Cocoa/Cocoa.h>
#include "macos_window.h"
#include "macos_termbuf.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* --- Helper: wchar_t to NSString --- */
static NSString* nsstring_from_wchar(const wchar_t* wstr) {
    if (!wstr) return nil;
    size_t len = wcslen(wstr);
    /* wchar_t is 4 bytes on macOS (UTF-32) */
    return [[NSString alloc] initWithBytes:wstr
                                    length:len * sizeof(wchar_t)
                                  encoding:NSUTF32LittleEndianStringEncoding];
}

/* COLORREF (0x00BBGGRR) to NSColor */
static NSColor* color_from_index(int index, int bold) {
    uint32_t rgb = termbuf_default_color(index);
    /* If bold and standard color (0-7), brighten */
    if (bold && index < 8) {
        rgb = termbuf_default_color(index + 8);
    }
    CGFloat r = ((rgb >> 16) & 0xFF) / 255.0;
    CGFloat g = ((rgb >> 8)  & 0xFF) / 255.0;
    CGFloat b = ((rgb >> 0)  & 0xFF) / 255.0;
    return [NSColor colorWithCalibratedRed:r green:g blue:b alpha:1.0];
}

/* --- Terminal View (custom NSView) --- */
@interface TTTerminalView : NSView <NSTextInputClient> {
    termbuf_t *_termbuf;
    int _charWidth;
    int _charHeight;
    int _fontAscent;
}
@property (nonatomic, strong) NSFont *termFont;
@property (nonatomic, strong) NSFont *termFontBold;
@property (nonatomic, strong) NSColor *fgColor;
@property (nonatomic, strong) NSColor *bgColor;
@property (nonatomic, assign) void (*outputCb)(const char*, int, void*);
@property (nonatomic, assign) void *outputCtx;
/* Text selection */
@property (nonatomic, assign) BOOL selecting;
@property (nonatomic, assign) int selStartCol;
@property (nonatomic, assign) int selStartRow;
@property (nonatomic, assign) int selEndCol;
@property (nonatomic, assign) int selEndRow;
@property (nonatomic, assign) BOOL hasSelection;
@end

@implementation TTTerminalView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _termFont = [NSFont fontWithName:@"Menlo" size:14.0];
        _termFontBold = [[NSFontManager sharedFontManager] convertFont:_termFont toHaveTrait:NSBoldFontMask];
        if (!_termFontBold) _termFontBold = _termFont;
        _fgColor = [NSColor whiteColor];
        _bgColor = [NSColor blackColor];
        _outputCb = NULL;
        _outputCtx = NULL;
        _hasSelection = NO;
        _selecting = NO;

        /* Calculate font metrics */
        [self updateFontMetrics];

        /* Create terminal buffer based on view size */
        int cols = (int)(frame.size.width / _charWidth);
        int rows = (int)(frame.size.height / _charHeight);
        if (cols < 1) cols = 80;
        if (rows < 1) rows = 24;
        _termbuf = termbuf_create(cols, rows);
    }
    return self;
}

- (void)dealloc {
    if (_termbuf) termbuf_destroy(_termbuf);
}

- (void)updateFontMetrics {
    NSDictionary *attrs = @{NSFontAttributeName: _termFont};
    NSSize charSize = [@"W" sizeWithAttributes:attrs];
    _charWidth = (int)ceil(charSize.width);
    _charHeight = (int)ceil(_termFont.ascender - _termFont.descender + _termFont.leading);
    _fontAscent = (int)ceil(_termFont.ascender);
}

- (BOOL)isFlipped {
    return YES;  /* Top-left origin like Windows */
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)isCellSelected:(int)col row:(int)row {
    if (!_hasSelection) return NO;
    int r1 = _selStartRow, c1 = _selStartCol, r2 = _selEndRow, c2 = _selEndCol;
    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        int tr = r1, tc = c1; r1 = r2; c1 = c2; r2 = tr; c2 = tc;
    }
    if (row < r1 || row > r2) return NO;
    if (row == r1 && row == r2) return col >= c1 && col <= c2;
    if (row == r1) return col >= c1;
    if (row == r2) return col <= c2;
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    if (!_termbuf) {
        [self.bgColor setFill];
        NSRectFill(dirtyRect);
        return;
    }

    int cols = termbuf_get_cols(_termbuf);
    int rows = termbuf_get_rows(_termbuf);
    int cx, cy;
    termbuf_get_cursor(_termbuf, &cx, &cy);
    int vp_offset = termbuf_get_viewport_offset(_termbuf);
    int sb_lines = termbuf_scrollback_lines(_termbuf);

    /* Calculate visible row range from dirty rect */
    int startRow = (int)(dirtyRect.origin.y / _charHeight);
    int endRow = (int)((dirtyRect.origin.y + dirtyRect.size.height) / _charHeight) + 1;
    if (startRow < 0) startRow = 0;
    if (endRow > rows) endRow = rows;

    /* Fill background */
    [self.bgColor setFill];
    NSRectFill(dirtyRect);

    /* Draw each visible row */
    for (int screen_row = startRow; screen_row < endRow; screen_row++) {
        /* Map screen row to data source (scrollback or screen buffer) */
        int data_row = screen_row - vp_offset;
        int from_scrollback = 0;
        int sb_line = -1;

        if (vp_offset > 0 && screen_row < vp_offset) {
            /* This row comes from scrollback */
            sb_line = sb_lines - vp_offset + screen_row;
            if (sb_line < 0 || sb_line >= sb_lines) continue;
            from_scrollback = 1;
        } else {
            data_row = screen_row - (vp_offset > rows ? rows : 0);
            /* Adjust: when scrolled up, bottom rows show screen buffer */
            data_row = screen_row;
            if (vp_offset > 0) {
                if (screen_row < vp_offset) {
                    from_scrollback = 1;
                    sb_line = sb_lines - vp_offset + screen_row;
                } else {
                    data_row = screen_row - vp_offset;
                }
            }
            if (data_row < 0 || data_row >= rows) continue;
        }

        int col = 0;
        while (col < cols) {
            const termbuf_cell_t *cell;
            if (from_scrollback) {
                cell = termbuf_get_scrollback_cell(_termbuf, col, sb_line);
            } else {
                cell = termbuf_get_cell(_termbuf, col, data_row);
            }
            if (!cell) { col++; continue; }

            uint8_t attr = cell->attr;
            uint8_t fg_idx = cell->fg;
            uint8_t bg_idx = cell->bg;
            int run_start = col;
            unichar run_chars[256];
            int run_len = 0;

            while (col < cols && run_len < 255) {
                if (from_scrollback)
                    cell = termbuf_get_scrollback_cell(_termbuf, col, sb_line);
                else
                    cell = termbuf_get_cell(_termbuf, col, data_row);
                if (!cell) break;
                if (col > run_start && (cell->attr != attr || cell->fg != fg_idx || cell->bg != bg_idx))
                    break;
                uint32_t ch = cell->ch;
                if (ch == 0 || ch == ' ') ch = ' ';
                run_chars[run_len++] = (ch > 0xFFFF) ? 0xFFFD : (unichar)ch;
                col++;
            }

            if (run_len == 0) continue;

            int is_bold = (attr & TERMBUF_ATTR_BOLD) != 0;
            int is_reverse = (attr & TERMBUF_ATTR_REVERSE) != 0;
            int is_underline = (attr & TERMBUF_ATTR_UNDERLINE) != 0;

            int draw_fg = is_reverse ? bg_idx : fg_idx;
            int draw_bg = is_reverse ? fg_idx : bg_idx;

            /* Draw background if non-default or selected */
            BOOL has_sel_in_run = NO;
            if (_hasSelection) {
                for (int rc = run_start; rc < run_start + run_len; rc++) {
                    if ([self isCellSelected:rc row:screen_row]) { has_sel_in_run = YES; break; }
                }
            }

            if (draw_bg != 0 || is_reverse || has_sel_in_run) {
                for (int rc = run_start; rc < run_start + run_len; rc++) {
                    NSColor *bgc;
                    if (_hasSelection && [self isCellSelected:rc row:screen_row]) {
                        bgc = [NSColor selectedTextBackgroundColor];
                    } else {
                        bgc = color_from_index(draw_bg, 0);
                    }
                    [bgc setFill];
                    NSRectFill(NSMakeRect(rc * _charWidth, screen_row * _charHeight,
                                          _charWidth, _charHeight));
                }
            }

            /* Draw text */
            NSFont *font = is_bold ? _termFontBold : _termFont;
            NSColor *fgColor_cell = color_from_index(draw_fg, is_bold);

            NSMutableDictionary *attrs_dict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                font, NSFontAttributeName,
                fgColor_cell, NSForegroundColorAttributeName,
                nil];
            if (is_underline) {
                [attrs_dict setObject:@(NSUnderlineStyleSingle) forKey:NSUnderlineStyleAttributeName];
            }

            NSString *str = [NSString stringWithCharacters:run_chars length:run_len];
            [str drawAtPoint:NSMakePoint(run_start * _charWidth,
                                         screen_row * _charHeight + (_charHeight - _fontAscent - (int)ceil(-_termFont.descender)) / 2)
              withAttributes:attrs_dict];
        }
    }

    /* Draw cursor (only when at bottom of scrollback and cursor visible) */
    if (vp_offset == 0 && termbuf_get_cursor_visible(_termbuf)) {
        if (cx >= 0 && cx < cols && cy >= startRow && cy < endRow) {
            NSRect cursorRect = NSMakeRect(cx * _charWidth, cy * _charHeight,
                                            _charWidth, _charHeight);
            int shape = termbuf_get_cursor_shape(_termbuf);
            if (shape == TERMBUF_CURSOR_BEAM) {
                cursorRect.size.width = 2;
                [[NSColor colorWithCalibratedWhite:0.9 alpha:0.9] setFill];
                NSRectFillUsingOperation(cursorRect, NSCompositingOperationSourceOver);
            } else if (shape == TERMBUF_CURSOR_UNDERLINE) {
                cursorRect.origin.y += _charHeight - 2;
                cursorRect.size.height = 2;
                [[NSColor colorWithCalibratedWhite:0.9 alpha:0.9] setFill];
                NSRectFillUsingOperation(cursorRect, NSCompositingOperationSourceOver);
            } else {
                /* Block cursor */
                [[NSColor colorWithCalibratedWhite:0.7 alpha:0.5] setFill];
                NSRectFillUsingOperation(cursorRect, NSCompositingOperationSourceOver);
                [[NSColor colorWithCalibratedWhite:0.8 alpha:0.8] setStroke];
                NSFrameRect(cursorRect);
            }
        }
    }

    termbuf_clear_dirty(_termbuf);
}

- (void)viewDidEndLiveResize {
    [super viewDidEndLiveResize];
    if (_termbuf && _charWidth > 0 && _charHeight > 0) {
        NSRect bounds = self.bounds;
        int cols = (int)(bounds.size.width / _charWidth);
        int rows = (int)(bounds.size.height / _charHeight);
        if (cols < 1) cols = 1;
        if (rows < 1) rows = 1;
        termbuf_resize(_termbuf, cols, rows);
        [self setNeedsDisplay:YES];
    }
}

- (void)keyDown:(NSEvent *)event {
    NSString *chars = event.characters;
    if (!chars.length) return;

    /* Handle special keys */
    unichar ch = [chars characterAtIndex:0];
    const char *seq = NULL;

    switch (ch) {
    case NSUpArrowFunctionKey:    seq = "\033[A"; break;
    case NSDownArrowFunctionKey:  seq = "\033[B"; break;
    case NSRightArrowFunctionKey: seq = "\033[C"; break;
    case NSLeftArrowFunctionKey:  seq = "\033[D"; break;
    case NSHomeFunctionKey:       seq = "\033[H"; break;
    case NSEndFunctionKey:        seq = "\033[F"; break;
    case NSPageUpFunctionKey:     seq = "\033[5~"; break;
    case NSPageDownFunctionKey:   seq = "\033[6~"; break;
    case NSDeleteFunctionKey:     seq = "\033[3~"; break;
    case NSF1FunctionKey:         seq = "\033OP"; break;
    case NSF2FunctionKey:         seq = "\033OQ"; break;
    case NSF3FunctionKey:         seq = "\033OR"; break;
    case NSF4FunctionKey:         seq = "\033OS"; break;
    case NSF5FunctionKey:         seq = "\033[15~"; break;
    case NSF6FunctionKey:         seq = "\033[17~"; break;
    case NSF7FunctionKey:         seq = "\033[18~"; break;
    case NSF8FunctionKey:         seq = "\033[19~"; break;
    case NSF9FunctionKey:         seq = "\033[20~"; break;
    case NSF10FunctionKey:        seq = "\033[21~"; break;
    case NSF11FunctionKey:        seq = "\033[23~"; break;
    case NSF12FunctionKey:        seq = "\033[24~"; break;
    case NSInsertFunctionKey:     seq = "\033[2~"; break;
    default: break;
    }

    if (seq && self.outputCb) {
        self.outputCb(seq, (int)strlen(seq), self.outputCtx);
    } else if (ch < 0xF700 && self.outputCb) {
        /* Handle Meta/Alt key: send ESC prefix */
        if (event.modifierFlags & NSEventModifierFlagOption) {
            const char esc = '\033';
            self.outputCb(&esc, 1, self.outputCtx);
        }
        /* Normal character - send as UTF-8 */
        const char *utf8 = [chars UTF8String];
        if (utf8) {
            self.outputCb(utf8, (int)strlen(utf8), self.outputCtx);
        }
    }
}

/* --- Mouse selection --- */

- (NSPoint)cellAtPoint:(NSPoint)pt {
    NSPoint cell;
    cell.x = (int)(pt.x / _charWidth);
    cell.y = (int)(pt.y / _charHeight);
    return cell;
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    NSPoint cell = [self cellAtPoint:pt];

    /* Check if mouse tracking is active */
    if (_termbuf && termbuf_get_mouse_mode(_termbuf) != TERMBUF_MOUSE_NONE) {
        int mods = 0;
        if (event.modifierFlags & NSEventModifierFlagShift) mods |= 1;
        if (event.modifierFlags & NSEventModifierFlagOption) mods |= 2;
        if (event.modifierFlags & NSEventModifierFlagControl) mods |= 4;
        termbuf_report_mouse(_termbuf, 0, (int)cell.x, (int)cell.y, 1, mods);
        return;
    }

    _selStartCol = (int)cell.x;
    _selStartRow = (int)cell.y;
    _selEndCol = _selStartCol;
    _selEndRow = _selStartRow;
    _selecting = YES;
    _hasSelection = NO;
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event {
    if (!_selecting) return;
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    NSPoint cell = [self cellAtPoint:pt];
    _selEndCol = (int)cell.x;
    _selEndRow = (int)cell.y;
    _hasSelection = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event {
    if (_termbuf && termbuf_get_mouse_mode(_termbuf) != TERMBUF_MOUSE_NONE) {
        NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
        NSPoint cell = [self cellAtPoint:pt];
        int mods = 0;
        if (event.modifierFlags & NSEventModifierFlagShift) mods |= 1;
        if (event.modifierFlags & NSEventModifierFlagOption) mods |= 2;
        if (event.modifierFlags & NSEventModifierFlagControl) mods |= 4;
        termbuf_report_mouse(_termbuf, 0, (int)cell.x, (int)cell.y, 0, mods);
        return;
    }
    _selecting = NO;
}

- (void)scrollWheel:(NSEvent *)event {
    if (!_termbuf) return;

    /* Check for mouse wheel tracking */
    if (termbuf_get_mouse_mode(_termbuf) != TERMBUF_MOUSE_NONE) {
        int delta = (event.scrollingDeltaY > 0) ? 64 : 65; /* wheel up/down */
        NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
        NSPoint cell = [self cellAtPoint:pt];
        termbuf_report_mouse(_termbuf, delta, (int)cell.x, (int)cell.y, 1, 0);
        return;
    }

    /* Scrollback navigation */
    int lines = (int)(-event.scrollingDeltaY / 3.0);
    if (lines == 0) lines = (event.scrollingDeltaY < 0) ? 1 : -1;
    termbuf_scroll_viewport(_termbuf, lines);
    [self setNeedsDisplay:YES];
}

/* --- Copy/Paste support --- */

- (void)copy:(id)sender {
    if (!_termbuf || !_hasSelection) return;
    char *text = termbuf_get_text(_termbuf, _selStartCol, _selStartRow, _selEndCol, _selEndRow);
    if (text) {
        tt_mac_clipboard_set_text(text);
        free(text);
    }
}

- (void)paste:(id)sender {
    if (!self.outputCb) return;
    char *text = tt_mac_clipboard_get_text();
    if (text) {
        if (_termbuf && termbuf_get_bracketed_paste(_termbuf)) {
            const char *start = "\033[200~";
            const char *end = "\033[201~";
            self.outputCb(start, (int)strlen(start), self.outputCtx);
            self.outputCb(text, (int)strlen(text), self.outputCtx);
            self.outputCb(end, (int)strlen(end), self.outputCtx);
        } else {
            self.outputCb(text, (int)strlen(text), self.outputCtx);
        }
        tt_mac_clipboard_free(text);
    }
}

- (void)selectAll:(id)sender {
    if (!_termbuf) return;
    _selStartCol = 0;
    _selStartRow = 0;
    _selEndCol = termbuf_get_cols(_termbuf) - 1;
    _selEndRow = termbuf_get_rows(_termbuf) - 1;
    _hasSelection = YES;
    [self setNeedsDisplay:YES];
}

- (void)clearSelection {
    _hasSelection = NO;
    _selecting = NO;
    [self setNeedsDisplay:YES];
}

- (termbuf_t *)termbuf {
    return _termbuf;
}

@end

/* --- Window Delegate --- */
@interface TTWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation TTWindowDelegate

- (BOOL)windowShouldClose:(id)sender {
    /* TODO: Confirm close if session active */
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
    /* TODO: Clean up terminal session */
}

@end

/* --- Window Management --- */

TTMacWindow tt_mac_window_create(int x, int y, int width, int height, const char* title) {
    @autoreleasepool {
        NSRect frame = NSMakeRect(x, y, width, height);
        NSWindowStyleMask style = NSWindowStyleMaskTitled |
                                  NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable;

        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:style
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];

        TTWindowDelegate* delegate = [[TTWindowDelegate alloc] init];
        window.delegate = delegate;

        if (title) {
            window.title = [NSString stringWithUTF8String:title];
        }

        window.minSize = NSMakeSize(320, 200);
        window.backgroundColor = [NSColor blackColor];

        return (__bridge_retained void*)window;
    }
}

void tt_mac_window_destroy(TTMacWindow win) {
    if (!win) return;
    @autoreleasepool {
        NSWindow* window = (__bridge_transfer NSWindow*)win;
        [window close];
    }
}

void tt_mac_window_show(TTMacWindow win) {
    if (!win) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        [window makeKeyAndOrderFront:nil];
    }
}

void tt_mac_window_hide(TTMacWindow win) {
    if (!win) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        [window orderOut:nil];
    }
}

void tt_mac_window_set_title(TTMacWindow win, const char* title) {
    if (!win || !title) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        window.title = [NSString stringWithUTF8String:title];
    }
}

void tt_mac_window_set_title_w(TTMacWindow win, const wchar_t* title) {
    if (!win || !title) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        NSString* str = nsstring_from_wchar(title);
        if (str) window.title = str;
    }
}

void tt_mac_window_get_rect(TTMacWindow win, int* x, int* y, int* w, int* h) {
    if (!win) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        NSRect frame = window.frame;
        if (x) *x = (int)frame.origin.x;
        if (y) *y = (int)frame.origin.y;
        if (w) *w = (int)frame.size.width;
        if (h) *h = (int)frame.size.height;
    }
}

void tt_mac_window_set_rect(TTMacWindow win, int x, int y, int w, int h) {
    if (!win) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        NSRect frame = NSMakeRect(x, y, w, h);
        [window setFrame:frame display:YES animate:NO];
    }
}

void tt_mac_window_invalidate(TTMacWindow win) {
    if (!win) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        [window.contentView setNeedsDisplay:YES];
    }
}

void tt_mac_window_set_topmost(TTMacWindow win, int topmost) {
    if (!win) return;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        window.level = topmost ? NSFloatingWindowLevel : NSNormalWindowLevel;
    }
}

/* --- Terminal View --- */

TTMacView tt_mac_termview_create(TTMacWindow win) {
    if (!win) return NULL;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)win;
        TTTerminalView* view = [[TTTerminalView alloc] initWithFrame:window.contentView.bounds];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [window.contentView addSubview:view];
        return (__bridge_retained void*)view;
    }
}

void tt_mac_termview_set_font(TTMacView v, const char* family, int size, int bold, int italic) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        NSString* fontName = family ? [NSString stringWithUTF8String:family] : @"Menlo";
        NSFont* font = [NSFont fontWithName:fontName size:(CGFloat)size];
        if (!font) {
            font = [NSFont monospacedSystemFontOfSize:(CGFloat)size weight:bold ? NSFontWeightBold : NSFontWeightRegular];
        }
        if (bold && font) {
            NSFont* boldFont = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSBoldFontMask];
            if (boldFont) font = boldFont;
        }
        if (italic && font) {
            NSFont* italicFont = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSItalicFontMask];
            if (italicFont) font = italicFont;
        }
        view.termFont = font;
        [view setNeedsDisplay:YES];
    }
}

void tt_mac_termview_set_colors(TTMacView v, unsigned int fg, unsigned int bg) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        view.fgColor = [NSColor colorWithRed:((fg >> 0) & 0xFF) / 255.0
                                       green:((fg >> 8) & 0xFF) / 255.0
                                        blue:((fg >> 16) & 0xFF) / 255.0
                                       alpha:1.0];
        view.bgColor = [NSColor colorWithRed:((bg >> 0) & 0xFF) / 255.0
                                       green:((bg >> 8) & 0xFF) / 255.0
                                        blue:((bg >> 16) & 0xFF) / 255.0
                                       alpha:1.0];
        [view setNeedsDisplay:YES];
    }
}

void tt_mac_termview_invalidate(TTMacView v) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        [view setNeedsDisplay:YES];
    }
}

void tt_mac_termview_scroll(TTMacView v, int lines) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        /* TODO: implement scrollback */
        [view setNeedsDisplay:YES];
    }
}

void tt_mac_termview_write(TTMacView v, const char *data, int len) {
    if (!v || !data || len <= 0) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        termbuf_t *tb = [view termbuf];
        if (tb) {
            termbuf_write(tb, data, len);
            if (termbuf_is_dirty(tb)) {
                [view setNeedsDisplay:YES];
            }
        }
    }
}

void tt_mac_termview_set_output_cb(TTMacView v, void (*cb)(const char*, int, void*), void *ctx) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        view.outputCb = cb;
        view.outputCtx = ctx;
        termbuf_t *tb = [view termbuf];
        if (tb) {
            termbuf_set_output_cb(tb, cb, ctx);
        }
    }
}

/* --- Drawing Context --- */

TTMacGraphicsContext tt_mac_gc_begin(TTMacView v) {
    if (!v) return NULL;
    NSGraphicsContext* ctx = [NSGraphicsContext currentContext];
    return (__bridge void*)ctx;
}

void tt_mac_gc_end(TTMacGraphicsContext gc) {
    /* Nothing to do - context managed by Cocoa draw cycle */
}

void tt_mac_gc_set_color(TTMacGraphicsContext gc, unsigned int color) {
    @autoreleasepool {
        NSColor* c = [NSColor colorWithRed:((color >> 0) & 0xFF) / 255.0
                                     green:((color >> 8) & 0xFF) / 255.0
                                      blue:((color >> 16) & 0xFF) / 255.0
                                     alpha:1.0];
        [c setFill];
        [c setStroke];
    }
}

void tt_mac_gc_set_bg_color(TTMacGraphicsContext gc, unsigned int color) {
    @autoreleasepool {
        NSColor* c = [NSColor colorWithRed:((color >> 0) & 0xFF) / 255.0
                                     green:((color >> 8) & 0xFF) / 255.0
                                      blue:((color >> 16) & 0xFF) / 255.0
                                     alpha:1.0];
        [c setFill];
    }
}

void tt_mac_gc_draw_text(TTMacGraphicsContext gc, int x, int y, const char* text, int len) {
    if (!text) return;
    @autoreleasepool {
        NSString* str = [[NSString alloc] initWithBytes:text length:len encoding:NSUTF8StringEncoding];
        if (!str) return;
        NSDictionary* attrs = @{
            NSFontAttributeName: [NSFont monospacedSystemFontOfSize:14.0 weight:NSFontWeightRegular],
            NSForegroundColorAttributeName: [NSColor whiteColor]
        };
        [str drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
    }
}

void tt_mac_gc_draw_text_w(TTMacGraphicsContext gc, int x, int y, const wchar_t* text, int len) {
    if (!text) return;
    @autoreleasepool {
        NSString* str = [[NSString alloc] initWithBytes:text
                                                 length:len * sizeof(wchar_t)
                                               encoding:NSUTF32LittleEndianStringEncoding];
        if (!str) return;
        NSDictionary* attrs = @{
            NSFontAttributeName: [NSFont monospacedSystemFontOfSize:14.0 weight:NSFontWeightRegular],
            NSForegroundColorAttributeName: [NSColor whiteColor]
        };
        [str drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
    }
}

void tt_mac_gc_fill_rect(TTMacGraphicsContext gc, int x, int y, int w, int h) {
    NSRectFill(NSMakeRect(x, y, w, h));
}

void tt_mac_gc_draw_line(TTMacGraphicsContext gc, int x1, int y1, int x2, int y2) {
    NSBezierPath* path = [NSBezierPath bezierPath];
    [path moveToPoint:NSMakePoint(x1, y1)];
    [path lineToPoint:NSMakePoint(x2, y2)];
    [path stroke];
}

/* --- Font --- */

TTMacFont tt_mac_font_create(const char* family, int size, int bold, int italic) {
    @autoreleasepool {
        NSString* fontName = family ? [NSString stringWithUTF8String:family] : @"Menlo";
        NSFont* font = [NSFont fontWithName:fontName size:(CGFloat)size];
        if (!font) {
            font = [NSFont monospacedSystemFontOfSize:(CGFloat)size weight:NSFontWeightRegular];
        }
        if (bold) {
            NSFont* bf = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSBoldFontMask];
            if (bf) font = bf;
        }
        if (italic) {
            NSFont* itf = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSItalicFontMask];
            if (itf) font = itf;
        }
        return (__bridge_retained void*)font;
    }
}

void tt_mac_font_destroy(TTMacFont f) {
    if (!f) return;
    @autoreleasepool {
        NSFont* font __attribute__((unused)) = (__bridge_transfer NSFont*)f;
    }
}

void tt_mac_font_get_metrics(TTMacFont f, int* char_width, int* char_height, int* ascent) {
    if (!f) return;
    @autoreleasepool {
        NSFont* font = (__bridge NSFont*)f;
        NSDictionary* attrs = @{NSFontAttributeName: font};
        NSSize charSize = [@"W" sizeWithAttributes:attrs];
        if (char_width)  *char_width  = (int)ceil(charSize.width);
        if (char_height) *char_height = (int)ceil(font.ascender - font.descender + font.leading);
        if (ascent)      *ascent      = (int)ceil(font.ascender);
    }
}

/* --- Menu --- */

TTMacMenu tt_mac_menu_create(void) {
    @autoreleasepool {
        NSMenu* menu = [[NSMenu alloc] init];
        return (__bridge_retained void*)menu;
    }
}

void tt_mac_menu_add_item(TTMacMenu m, const char* title, int command_id, const char* shortcut) {
    if (!m) return;
    @autoreleasepool {
        NSMenu* menu = (__bridge NSMenu*)m;
        NSString* t = title ? [NSString stringWithUTF8String:title] : @"";
        NSString* key = shortcut ? [NSString stringWithUTF8String:shortcut] : @"";
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:t action:nil keyEquivalent:key];
        item.tag = command_id;
        [menu addItem:item];
    }
}

TTMacMenu tt_mac_menu_add_submenu(TTMacMenu parent, const char* title) {
    if (!parent) return NULL;
    @autoreleasepool {
        NSMenu* pmenu = (__bridge NSMenu*)parent;
        NSString* t = title ? [NSString stringWithUTF8String:title] : @"";
        NSMenu* submenu = [[NSMenu alloc] initWithTitle:t];
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:t action:nil keyEquivalent:@""];
        item.submenu = submenu;
        [pmenu addItem:item];
        return (__bridge_retained void*)submenu;
    }
}

void tt_mac_menu_add_separator(TTMacMenu m) {
    if (!m) return;
    @autoreleasepool {
        NSMenu* menu = (__bridge NSMenu*)m;
        [menu addItem:[NSMenuItem separatorItem]];
    }
}

void tt_mac_menubar_set(TTMacMenu m) {
    if (!m) return;
    @autoreleasepool {
        NSMenu* menu = (__bridge NSMenu*)m;
        [NSApp setMainMenu:menu];
    }
}

/* --- Timer --- */

typedef struct {
    void (*callback)(void* ctx);
    void* ctx;
} TTTimerInfo;

@interface TTTimerTarget : NSObject
@property (nonatomic, assign) TTTimerInfo info;
- (void)timerFired:(NSTimer*)timer;
@end

@implementation TTTimerTarget
- (void)timerFired:(NSTimer*)timer {
    if (self.info.callback) {
        self.info.callback(self.info.ctx);
    }
}
@end

TTMacTimer tt_mac_timer_create(double interval_seconds, void (*callback)(void* ctx), void* ctx) {
    @autoreleasepool {
        TTTimerTarget* target = [[TTTimerTarget alloc] init];
        TTTimerInfo info = { callback, ctx };
        target.info = info;

        NSTimer* timer = [NSTimer scheduledTimerWithTimeInterval:interval_seconds
                                                          target:target
                                                        selector:@selector(timerFired:)
                                                        userInfo:nil
                                                         repeats:YES];
        return (__bridge_retained void*)timer;
    }
}

void tt_mac_timer_destroy(TTMacTimer t) {
    if (!t) return;
    @autoreleasepool {
        NSTimer* timer = (__bridge_transfer NSTimer*)t;
        [timer invalidate];
    }
}

/* --- Clipboard --- */

int tt_mac_clipboard_set_text(const char* text) {
    if (!text) return 0;
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        NSString* str = [NSString stringWithUTF8String:text];
        return [pb setString:str forType:NSPasteboardTypeString] ? 1 : 0;
    }
}

int tt_mac_clipboard_set_text_w(const wchar_t* text) {
    if (!text) return 0;
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        NSString* str = nsstring_from_wchar(text);
        if (!str) return 0;
        return [pb setString:str forType:NSPasteboardTypeString] ? 1 : 0;
    }
}

char* tt_mac_clipboard_get_text(void) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSString* str = [pb stringForType:NSPasteboardTypeString];
        if (!str) return NULL;
        const char* utf8 = [str UTF8String];
        return strdup(utf8);
    }
}

wchar_t* tt_mac_clipboard_get_text_w(void) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSString* str = [pb stringForType:NSPasteboardTypeString];
        if (!str) return NULL;

        NSData* data = [str dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
        size_t wlen = [data length] / sizeof(wchar_t);
        wchar_t* result = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
        if (!result) return NULL;
        memcpy(result, [data bytes], wlen * sizeof(wchar_t));
        result[wlen] = L'\0';
        return result;
    }
}

void tt_mac_clipboard_free(void* ptr) {
    free(ptr);
}

/* --- Application --- */

@interface TTAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation TTAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    /* Application ready */
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end

void tt_mac_app_init(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        TTAppDelegate* delegate = [[TTAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }
}

void tt_mac_app_run(void) {
    @autoreleasepool {
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }
}

void tt_mac_app_quit(void) {
    @autoreleasepool {
        [NSApp terminate:nil];
    }
}

/* --- Message Box --- */

int tt_mac_messagebox(TTMacWindow parent, const char* text, const char* caption, unsigned int type) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        if (text)    alert.messageText = [NSString stringWithUTF8String:text];
        if (caption) alert.informativeText = [NSString stringWithUTF8String:caption];

        /* Map Windows MB_ types to NSAlert style */
        unsigned int icon = type & 0xF0;
        if (icon == 0x10) {       /* MB_ICONERROR */
            alert.alertStyle = NSAlertStyleCritical;
        } else if (icon == 0x30) { /* MB_ICONWARNING */
            alert.alertStyle = NSAlertStyleWarning;
        } else {
            alert.alertStyle = NSAlertStyleInformational;
        }

        unsigned int buttons = type & 0x0F;
        if (buttons == 1) {       /* MB_OKCANCEL */
            [alert addButtonWithTitle:@"OK"];
            [alert addButtonWithTitle:@"Cancel"];
        } else if (buttons == 4) { /* MB_YESNO */
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];
        } else if (buttons == 3) { /* MB_YESNOCANCEL */
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];
            [alert addButtonWithTitle:@"Cancel"];
        } else {
            [alert addButtonWithTitle:@"OK"];
        }

        NSModalResponse response = [alert runModal];
        /* Map back to Windows IDOK/IDCANCEL/IDYES/IDNO */
        if (response == NSAlertFirstButtonReturn)  return 1; /* IDOK / IDYES */
        if (response == NSAlertSecondButtonReturn) return 2; /* IDCANCEL / IDNO */
        if (response == NSAlertThirdButtonReturn)  return 7; /* IDNO (for YESNOCANCEL) */
        return 0;
    }
}

/* --- Terminal View Extended API --- */

termbuf_t *tt_mac_termview_get_termbuf(TTMacView v) {
    if (!v) return NULL;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        return [view termbuf];
    }
}

void tt_mac_termview_clear_selection(TTMacView v) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        [view clearSelection];
    }
}

void tt_mac_termview_select_all(TTMacView v) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        [view selectAll:nil];
    }
}

void tt_mac_termview_copy(TTMacView v) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        [view copy:nil];
    }
}

void tt_mac_termview_paste(TTMacView v) {
    if (!v) return;
    @autoreleasepool {
        TTTerminalView* view = (__bridge TTTerminalView*)v;
        [view paste:nil];
    }
}

/* --- File Dialogs --- */

char* tt_mac_open_file_dialog(TTMacWindow parent, const char* filter, const char* initial_dir) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;

        if (initial_dir) {
            panel.directoryURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:initial_dir]];
        }

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            return strdup([[url path] UTF8String]);
        }
        return NULL;
    }
}

char* tt_mac_save_file_dialog(TTMacWindow parent, const char* filter, const char* initial_dir) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];

        if (initial_dir) {
            panel.directoryURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:initial_dir]];
        }

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            return strdup([[url path] UTF8String]);
        }
        return NULL;
    }
}
