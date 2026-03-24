/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * macOS main entry point for Tera Term.
 * Implements the Cocoa application lifecycle, New Connection dialog,
 * and bridges serial/TCP connections to the terminal emulator view.
 */

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "macos_window.h"
#include "macos_termbuf.h"
#include "macos_connection.h"
#include "macos_serial.h"
#include "macos_net.h"
#include <time.h>

/* ===================================================================
 * Session Logger
 * =================================================================== */

@interface TTSessionLogger : NSObject
@property (nonatomic, assign) FILE *logFile;
@property (nonatomic, copy) NSString *logPath;
@property (nonatomic, assign) BOOL paused;
@property (nonatomic, assign) BOOL addTimestamps;
@property (nonatomic, assign) BOOL needTimestamp;
@end

@implementation TTSessionLogger

- (void)dealloc {
    [self stop];
}

- (BOOL)startWithPath:(NSString *)path append:(BOOL)append timestamps:(BOOL)ts {
    if (_logFile) [self stop];
    _logFile = fopen(path.UTF8String, append ? "ab" : "wb");
    if (!_logFile) return NO;
    _logPath = path;
    _paused = NO;
    _addTimestamps = ts;
    _needTimestamp = ts;

    /* Write header */
    time_t now = time(NULL);
    char buf[128];
    strftime(buf, sizeof(buf), "[Log started: %Y-%m-%d %H:%M:%S]\n", localtime(&now));
    fwrite(buf, 1, strlen(buf), _logFile);
    fflush(_logFile);
    return YES;
}

- (void)stop {
    if (_logFile) {
        time_t now = time(NULL);
        char buf[128];
        strftime(buf, sizeof(buf), "\n[Log stopped: %Y-%m-%d %H:%M:%S]\n", localtime(&now));
        fwrite(buf, 1, strlen(buf), _logFile);
        fclose(_logFile);
        _logFile = NULL;
    }
    _logPath = nil;
}

- (void)writeData:(const char *)data length:(int)len {
    if (!_logFile || _paused || !data || len <= 0) return;
    if (_addTimestamps) {
        for (int i = 0; i < len; i++) {
            if (_needTimestamp) {
                time_t now = time(NULL);
                char ts[32];
                strftime(ts, sizeof(ts), "[%H:%M:%S] ", localtime(&now));
                fwrite(ts, 1, strlen(ts), _logFile);
                _needTimestamp = NO;
            }
            fputc(data[i], _logFile);
            if (data[i] == '\n') _needTimestamp = YES;
        }
    } else {
        fwrite(data, 1, len, _logFile);
    }
    fflush(_logFile);
}

- (void)addComment:(NSString *)comment {
    if (!_logFile) return;
    time_t now = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "[%H:%M:%S] ", localtime(&now));
    fprintf(_logFile, "\n%s[Comment: %s]\n", ts, comment.UTF8String);
    fflush(_logFile);
}

@end

/* ===================================================================
 * New Connection Dialog
 * =================================================================== */

@interface TTConnectionDialog : NSObject
@property (nonatomic, strong) NSWindow *dialogWindow;
@property (nonatomic, strong) NSTabView *tabView;
/* Serial tab */
@property (nonatomic, strong) NSPopUpButton *serialPortPopup;
@property (nonatomic, strong) NSPopUpButton *baudRatePopup;
@property (nonatomic, strong) NSPopUpButton *dataBitsPopup;
@property (nonatomic, strong) NSPopUpButton *parityPopup;
@property (nonatomic, strong) NSPopUpButton *stopBitsPopup;
@property (nonatomic, strong) NSPopUpButton *flowControlPopup;
/* TCP tab */
@property (nonatomic, strong) NSTextField *hostField;
@property (nonatomic, strong) NSTextField *portField;
@property (nonatomic, strong) NSButton *telnetCheckbox;
/* Result */
@property (nonatomic, assign) BOOL accepted;
@property (nonatomic, assign) int selectedTab; /* 0=serial, 1=tcp */
@end

@implementation TTConnectionDialog

- (instancetype)init {
    self = [super init];
    if (self) {
        _accepted = NO;
        _selectedTab = 0;
        [self buildUI];
    }
    return self;
}

- (void)buildUI {
    /* Main dialog window */
    NSRect frame = NSMakeRect(0, 0, 460, 340);
    self.dialogWindow = [[NSWindow alloc] initWithContentRect:frame
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
        backing:NSBackingStoreBuffered defer:NO];
    self.dialogWindow.title = @"New Connection";
    [self.dialogWindow center];

    NSView *content = self.dialogWindow.contentView;

    /* Tab view */
    self.tabView = [[NSTabView alloc] initWithFrame:NSMakeRect(10, 60, 440, 270)];

    /* === Serial Port Tab === */
    NSTabViewItem *serialTab = [[NSTabViewItem alloc] initWithIdentifier:@"serial"];
    serialTab.label = @"Serial Port";

    NSView *sv = serialTab.view;

    [self addLabel:@"Port:" at:NSMakePoint(20, 190) to:sv];
    self.serialPortPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(120, 188, 290, 26) pullsDown:NO];
    [sv addSubview:self.serialPortPopup];

    /* Refresh button */
    NSButton *refreshBtn = [NSButton buttonWithTitle:@"Refresh" target:self action:@selector(refreshSerialPorts:)];
    refreshBtn.frame = NSMakeRect(340, 158, 70, 24);
    refreshBtn.bezelStyle = NSBezelStyleRounded;
    [sv addSubview:refreshBtn];

    [self addLabel:@"Baud Rate:" at:NSMakePoint(20, 155) to:sv];
    self.baudRatePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(120, 153, 200, 26) pullsDown:NO];
    NSArray *bauds = @[@"300", @"1200", @"2400", @"4800", @"9600", @"19200",
                       @"38400", @"57600", @"115200", @"230400", @"460800", @"921600"];
    for (NSString *b in bauds) [self.baudRatePopup addItemWithTitle:b];
    [self.baudRatePopup selectItemWithTitle:@"9600"];
    [sv addSubview:self.baudRatePopup];

    [self addLabel:@"Data Bits:" at:NSMakePoint(20, 120) to:sv];
    self.dataBitsPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(120, 118, 100, 26) pullsDown:NO];
    for (NSString *d in @[@"5", @"6", @"7", @"8"]) [self.dataBitsPopup addItemWithTitle:d];
    [self.dataBitsPopup selectItemWithTitle:@"8"];
    [sv addSubview:self.dataBitsPopup];

    [self addLabel:@"Parity:" at:NSMakePoint(230, 120) to:sv];
    self.parityPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(290, 118, 120, 26) pullsDown:NO];
    [self.parityPopup addItemWithTitle:@"None"];
    [self.parityPopup addItemWithTitle:@"Odd"];
    [self.parityPopup addItemWithTitle:@"Even"];
    [sv addSubview:self.parityPopup];

    [self addLabel:@"Stop Bits:" at:NSMakePoint(20, 85) to:sv];
    self.stopBitsPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(120, 83, 100, 26) pullsDown:NO];
    [self.stopBitsPopup addItemWithTitle:@"1"];
    [self.stopBitsPopup addItemWithTitle:@"2"];
    [sv addSubview:self.stopBitsPopup];

    [self addLabel:@"Flow Ctrl:" at:NSMakePoint(230, 85) to:sv];
    self.flowControlPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(290, 83, 120, 26) pullsDown:NO];
    [self.flowControlPopup addItemWithTitle:@"None"];
    [self.flowControlPopup addItemWithTitle:@"XON/XOFF"];
    [self.flowControlPopup addItemWithTitle:@"RTS/CTS"];
    [sv addSubview:self.flowControlPopup];

    [self.tabView addTabViewItem:serialTab];

    /* === TCP/IP Tab === */
    NSTabViewItem *tcpTab = [[NSTabViewItem alloc] initWithIdentifier:@"tcp"];
    tcpTab.label = @"TCP/IP";

    NSView *tv = tcpTab.view;

    [self addLabel:@"Host:" at:NSMakePoint(20, 180) to:tv];
    self.hostField = [[NSTextField alloc] initWithFrame:NSMakeRect(120, 178, 290, 24)];
    self.hostField.placeholderString = @"hostname or IP address";
    [tv addSubview:self.hostField];

    [self addLabel:@"Port:" at:NSMakePoint(20, 145) to:tv];
    self.portField = [[NSTextField alloc] initWithFrame:NSMakeRect(120, 143, 100, 24)];
    self.portField.placeholderString = @"23";
    self.portField.stringValue = @"23";
    [tv addSubview:self.portField];

    self.telnetCheckbox = [NSButton checkboxWithTitle:@"Use Telnet protocol" target:nil action:nil];
    self.telnetCheckbox.frame = NSMakeRect(120, 110, 250, 24);
    self.telnetCheckbox.state = NSControlStateValueOn;
    [tv addSubview:self.telnetCheckbox];

    [self.tabView addTabViewItem:tcpTab];
    [content addSubview:self.tabView];

    /* Buttons */
    NSButton *connectBtn = [NSButton buttonWithTitle:@"Connect" target:self action:@selector(connectClicked:)];
    connectBtn.frame = NSMakeRect(350, 15, 90, 32);
    connectBtn.bezelStyle = NSBezelStyleRounded;
    connectBtn.keyEquivalent = @"\r"; /* Enter key */
    [content addSubview:connectBtn];

    NSButton *cancelBtn = [NSButton buttonWithTitle:@"Cancel" target:self action:@selector(cancelClicked:)];
    cancelBtn.frame = NSMakeRect(250, 15, 90, 32);
    cancelBtn.bezelStyle = NSBezelStyleRounded;
    cancelBtn.keyEquivalent = @"\033"; /* Escape key */
    [content addSubview:cancelBtn];

    /* Populate serial ports */
    [self refreshSerialPorts:nil];
}

- (void)addLabel:(NSString *)text at:(NSPoint)pt to:(NSView *)view {
    NSTextField *label = [NSTextField labelWithString:text];
    label.frame = NSMakeRect(pt.x, pt.y, 95, 18);
    label.alignment = NSTextAlignmentRight;
    [view addSubview:label];
}

- (void)refreshSerialPorts:(id)sender {
    [self.serialPortPopup removeAllItems];

    char ports[64][256];
    int count = tt_mac_serial_enumerate(ports, 64);

    if (count == 0) {
        [self.serialPortPopup addItemWithTitle:@"(no serial ports found)"];
        [self.serialPortPopup setEnabled:NO];
    } else {
        [self.serialPortPopup setEnabled:YES];
        for (int i = 0; i < count; i++) {
            [self.serialPortPopup addItemWithTitle:[NSString stringWithUTF8String:ports[i]]];
        }
    }
}

- (void)connectClicked:(id)sender {
    self.accepted = YES;
    NSTabViewItem *selected = [self.tabView selectedTabViewItem];
    self.selectedTab = [[selected identifier] isEqual:@"serial"] ? 0 : 1;
    [NSApp stopModal];
}

- (void)cancelClicked:(id)sender {
    self.accepted = NO;
    [NSApp stopModal];
}

- (TTSerialParams)serialParams {
    TTSerialParams p;
    memset(&p, 0, sizeof(p));
    NSString *port = self.serialPortPopup.titleOfSelectedItem;
    if (port) strncpy(p.device, port.UTF8String, sizeof(p.device) - 1);
    p.baud_rate = self.baudRatePopup.titleOfSelectedItem.intValue;
    p.data_bits = self.dataBitsPopup.titleOfSelectedItem.intValue;
    p.stop_bits = self.stopBitsPopup.titleOfSelectedItem.intValue;
    p.parity = (int)[self.parityPopup indexOfSelectedItem]; /* 0=none, 1=odd, 2=even */
    p.flow_control = (int)[self.flowControlPopup indexOfSelectedItem]; /* 0=none, 1=xon, 2=rts */
    return p;
}

- (TTTcpParams)tcpParams {
    TTTcpParams p;
    memset(&p, 0, sizeof(p));
    NSString *host = self.hostField.stringValue;
    if (host) strncpy(p.host, host.UTF8String, sizeof(p.host) - 1);
    p.port = self.portField.intValue;
    if (p.port <= 0) p.port = 23;
    p.telnet = (self.telnetCheckbox.state == NSControlStateValueOn) ? 1 : 0;
    return p;
}

@end

/* ===================================================================
 * Terminal Session Controller
 * =================================================================== */

@class TTMacAppDelegate;

@interface TTTerminalSession : NSObject
@property (nonatomic, assign) TTMacWindow window;
@property (nonatomic, assign) TTMacView termView;
@property (nonatomic, assign) TTMacConnection connection;
@property (nonatomic, strong) NSTimer *readTimer;
@property (nonatomic, strong) NSTextField *statusField;
@property (nonatomic, weak) TTMacAppDelegate *appDelegate;
@property (nonatomic, strong) TTSessionLogger *logger;
/* Settings */
@property (nonatomic, copy) NSString *fontFamily;
@property (nonatomic, assign) int fontSize;
@property (nonatomic, assign) int termCols;
@property (nonatomic, assign) int termRows;
@end

@implementation TTTerminalSession

- (instancetype)init {
    self = [super init];
    if (self) {
        _window = NULL;
        _termView = NULL;
        _connection = NULL;
        _fontFamily = @"Menlo";
        _fontSize = 14;
        _termCols = 80;
        _termRows = 24;
    }
    return self;
}

- (void)createWindowWithTitle:(NSString *)title cols:(int)cols rows:(int)rows {
    static const CGFloat kStatusBarHeight = 20;

    TTMacFont font = tt_mac_font_create("Menlo", 14, 0, 0);
    int charWidth = 0, charHeight = 0, ascent = 0;
    tt_mac_font_get_metrics(font, &charWidth, &charHeight, &ascent);
    tt_mac_font_destroy(font);

    /* Window content = exact terminal grid + status bar */
    int width = cols * charWidth;
    int height = rows * charHeight + (int)kStatusBarHeight;

    self.window = tt_mac_window_create(100, 100, width, height, [title UTF8String]);

    NSWindow *nsWin = (__bridge NSWindow *)self.window;
    NSRect contentBounds = nsWin.contentView.bounds;

    /* Status bar at top of window (Cocoa coords: y increases upward) */
    self.statusField = [[NSTextField alloc] initWithFrame:
        NSMakeRect(0, contentBounds.size.height - kStatusBarHeight,
                   contentBounds.size.width, kStatusBarHeight)];
    self.statusField.editable = NO;
    self.statusField.bordered = NO;
    self.statusField.drawsBackground = YES;
    self.statusField.backgroundColor = [NSColor colorWithCalibratedWhite:0.15 alpha:1.0];
    self.statusField.textColor = [NSColor colorWithCalibratedWhite:0.7 alpha:1.0];
    self.statusField.font = [NSFont systemFontOfSize:11];
    self.statusField.stringValue = @"Not connected";
    self.statusField.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [nsWin.contentView addSubview:self.statusField];

    /* Terminal view fills content area below status bar */
    self.termView = tt_mac_termview_create_inset(self.window,
        (int)kStatusBarHeight, 0, 0, 0);
    tt_mac_termview_set_font(self.termView, "Menlo", 14, 0, 0);
    tt_mac_termview_set_colors(self.termView, 0x00FFFFFF, 0x00000000);
}

- (void)updateStatus:(NSString *)status {
    dispatch_async(dispatch_get_main_queue(), ^{
        self.statusField.stringValue = status;
    });
}

static void connection_output_callback(const char *data, int len, void *ctx) {
    TTTerminalSession *session = (__bridge TTTerminalSession *)ctx;
    if (session.connection && data && len > 0) {
        tt_conn_write(session.connection, data, len);
    }
}

static void session_title_callback(const char *title, void *ctx) {
    TTTerminalSession *session = (__bridge TTTerminalSession *)ctx;
    if (title && session.window) {
        NSString *t = [NSString stringWithFormat:@"Tera Term - %s", title];
        dispatch_async(dispatch_get_main_queue(), ^{
            tt_mac_window_set_title(session.window, t.UTF8String);
        });
    }
}

static void session_bell_callback(void *ctx) {
    NSBeep();
}

static void session_log_callback(const char *data, int len, void *ctx) {
    TTTerminalSession *session = (__bridge TTTerminalSession *)ctx;
    if (session.logger) {
        [session.logger writeData:data length:len];
    }
}

- (void)connectWith:(TTMacConnection)conn {
    if (!conn) return;
    self.connection = conn;

    /* Set up keyboard → connection output path */
    tt_mac_termview_set_output_cb(self.termView, connection_output_callback,
                                   (__bridge void *)self);

    /* Set up title, bell, and log callbacks on termbuf */
    termbuf_t *tb = tt_mac_termview_get_termbuf(self.termView);
    if (tb) {
        termbuf_set_title_cb(tb, session_title_callback, (__bridge void *)self);
        termbuf_set_bell_cb(tb, session_bell_callback, (__bridge void *)self);
        termbuf_set_log_cb(tb, session_log_callback, (__bridge void *)self);
    }

    /* Update window title and status */
    const char *desc = tt_conn_get_description(conn);
    NSString *title = [NSString stringWithFormat:@"Tera Term - %s", desc];
    tt_mac_window_set_title(self.window, title.UTF8String);
    [self updateStatus:[NSString stringWithUTF8String:desc]];

    /* Start read timer */
    self.readTimer = [NSTimer scheduledTimerWithTimeInterval:0.005
                                                      target:self
                                                    selector:@selector(readFromConnection:)
                                                    userInfo:nil
                                                     repeats:YES];
    /* Ensure timer fires during tracking loops (window resize, menu) */
    [[NSRunLoop currentRunLoop] addTimer:self.readTimer forMode:NSRunLoopCommonModes];
}

- (void)readFromConnection:(NSTimer *)timer {
    if (!self.connection || !tt_conn_is_open(self.connection)) {
        [self disconnect];
        return;
    }

    char buffer[4096];
    int n = tt_conn_read(self.connection, buffer, sizeof(buffer));
    if (n > 0) {
        tt_mac_termview_write(self.termView, buffer, n);
    } else if (n < 0) {
        [self disconnect];
    }
}

- (void)disconnect {
    if (self.readTimer) {
        [self.readTimer invalidate];
        self.readTimer = nil;
    }
    if (self.connection) {
        tt_conn_close(self.connection);
        self.connection = NULL;
    }
    tt_mac_termview_set_output_cb(self.termView, NULL, NULL);

    tt_mac_window_set_title(self.window, "Tera Term");
    [self updateStatus:@"Disconnected"];

    /* Write disconnect message to terminal */
    const char *msg = "\r\n\033[31m--- Disconnected ---\033[0m\r\n";
    tt_mac_termview_write(self.termView, msg, (int)strlen(msg));
}

- (void)sendBreak {
    if (self.connection) tt_conn_send_break(self.connection);
}

- (void)dealloc {
    [self disconnect];
    if (self.window) tt_mac_window_destroy(self.window);
}

@end

/* ===================================================================
 * Application Delegate
 * =================================================================== */

@interface TTMacAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) TTTerminalSession *session;
@end

@implementation TTMacAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [self setupMenuBar];

    /* Create terminal window */
    self.session = [[TTTerminalSession alloc] init];
    self.session.appDelegate = self;
    [self.session createWindowWithTitle:@"Tera Term" cols:80 rows:24];
    tt_mac_window_show(self.session.window);

    /* Show New Connection dialog on startup */
    [self performSelector:@selector(showNewConnectionDialog) withObject:nil afterDelay:0.1];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)showNewConnectionDialog {
    TTConnectionDialog *dialog = [[TTConnectionDialog alloc] init];

    /* Run as modal */
    [dialog.dialogWindow makeKeyAndOrderFront:nil];
    [NSApp runModalForWindow:dialog.dialogWindow];
    [dialog.dialogWindow orderOut:nil];

    if (!dialog.accepted) return;

    /* Disconnect existing session if any */
    if (self.session.connection) {
        [self.session disconnect];
    }

    if (dialog.selectedTab == 0) {
        /* Serial connection */
        TTSerialParams params = [dialog serialParams];
        if (!params.device[0]) {
            tt_mac_messagebox(self.session.window,
                "No serial port selected.", "Connection Error", 0);
            return;
        }
        TTMacConnection conn = tt_conn_open_serial(&params);
        if (!conn) {
            tt_mac_messagebox(self.session.window,
                "Failed to open serial port.\nCheck that the device is connected and not in use.",
                "Connection Error", 0);
            return;
        }
        [self.session connectWith:conn];
    } else {
        /* TCP connection */
        TTTcpParams params = [dialog tcpParams];
        if (!params.host[0]) {
            tt_mac_messagebox(self.session.window,
                "No host specified.", "Connection Error", 0);
            return;
        }

        /* Show connecting status */
        [self.session updateStatus:
            [NSString stringWithFormat:@"Connecting to %s:%d...", params.host, params.port]];

        /* TCP connect (blocking for now — could be made async) */
        TTMacConnection conn = tt_conn_open_tcp(&params);
        if (!conn) {
            [self.session updateStatus:@"Connection failed"];
            tt_mac_messagebox(self.session.window,
                "Failed to connect.\nCheck the hostname and port.",
                "Connection Error", 0);
            return;
        }
        [self.session connectWith:conn];
    }
}

/* --- Menu Bar --- */

- (void)setupMenuBar {
    NSMenu *mainMenu = [[NSMenu alloc] init];

    /* Application menu */
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    NSMenu *appMenu = [[NSMenu alloc] init];
    [appMenu addItemWithTitle:@"About Tera Term"
                       action:@selector(showAbout:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit Tera Term"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    appMenuItem.submenu = appMenu;
    [mainMenu addItem:appMenuItem];

    /* File menu */
    NSMenuItem *fileMenuItem = [[NSMenuItem alloc] init];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenu addItemWithTitle:@"New Connection..."
                        action:@selector(newConnection:)
                 keyEquivalent:@"n"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Disconnect"
                        action:@selector(disconnectSession:)
                 keyEquivalent:@"d"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Send File..."
                        action:@selector(sendFile:)
                 keyEquivalent:@""];
    [fileMenu addItemWithTitle:@"Receive File..."
                        action:@selector(receiveFile:)
                 keyEquivalent:@""];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Log..."
                        action:@selector(startLog:)
                 keyEquivalent:@"l"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Close"
                        action:@selector(performClose:)
                 keyEquivalent:@"w"];
    fileMenuItem.submenu = fileMenu;
    [mainMenu addItem:fileMenuItem];

    /* Edit menu */
    NSMenuItem *editMenuItem = [[NSMenuItem alloc] init];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItemWithTitle:@"Copy"
                        action:@selector(copy:)
                 keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste"
                        action:@selector(paste:)
                 keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Paste with CR"
                        action:@selector(pasteCR:)
                 keyEquivalent:@""];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:@"Select All"
                        action:@selector(selectAll:)
                 keyEquivalent:@"a"];
    [editMenu addItemWithTitle:@"Select Screen"
                        action:@selector(selectScreen:)
                 keyEquivalent:@""];
    [editMenu addItemWithTitle:@"Cancel Selection"
                        action:@selector(cancelSelection:)
                 keyEquivalent:@""];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:@"Clear Screen"
                        action:@selector(clearScreen:)
                 keyEquivalent:@""];
    [editMenu addItemWithTitle:@"Clear Buffer"
                        action:@selector(clearBuffer:)
                 keyEquivalent:@""];
    editMenuItem.submenu = editMenu;
    [mainMenu addItem:editMenuItem];

    /* Setup menu */
    NSMenuItem *setupMenuItem = [[NSMenuItem alloc] init];
    NSMenu *setupMenu = [[NSMenu alloc] initWithTitle:@"Setup"];
    [setupMenu addItemWithTitle:@"Terminal..."
                         action:@selector(setupTerminal:)
                  keyEquivalent:@""];
    [setupMenu addItemWithTitle:@"Serial Port..."
                         action:@selector(setupSerial:)
                  keyEquivalent:@""];
    [setupMenu addItemWithTitle:@"Font..."
                         action:@selector(setupFont:)
                  keyEquivalent:@""];
    [setupMenu addItem:[NSMenuItem separatorItem]];
    [setupMenu addItemWithTitle:@"Save Setup..."
                         action:@selector(saveSetup:)
                  keyEquivalent:@""];
    [setupMenu addItemWithTitle:@"Restore Setup..."
                         action:@selector(restoreSetup:)
                  keyEquivalent:@""];
    setupMenuItem.submenu = setupMenu;
    [mainMenu addItem:setupMenuItem];

    /* Control menu */
    NSMenuItem *controlMenuItem = [[NSMenuItem alloc] init];
    NSMenu *controlMenu = [[NSMenu alloc] initWithTitle:@"Control"];
    [controlMenu addItemWithTitle:@"Reset Terminal"
                           action:@selector(resetTerminal:)
                    keyEquivalent:@""];
    [controlMenu addItemWithTitle:@"Reset Remote Title"
                           action:@selector(resetRemoteTitle:)
                    keyEquivalent:@""];
    [controlMenu addItem:[NSMenuItem separatorItem]];
    [controlMenu addItemWithTitle:@"Are You There"
                           action:@selector(areYouThere:)
                    keyEquivalent:@""];
    [controlMenu addItemWithTitle:@"Send Break"
                           action:@selector(sendBreak:)
                    keyEquivalent:@"b"];
    [controlMenu addItemWithTitle:@"Reset Port"
                           action:@selector(resetPort:)
                    keyEquivalent:@""];
    controlMenuItem.submenu = controlMenu;
    [mainMenu addItem:controlMenuItem];

    /* Help menu */
    NSMenuItem *helpMenuItem = [[NSMenuItem alloc] init];
    NSMenu *helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];
    [helpMenu addItemWithTitle:@"Tera Term Help"
                        action:@selector(showHelp:)
                 keyEquivalent:@"?"];
    helpMenuItem.submenu = helpMenu;
    [mainMenu addItem:helpMenuItem];

    [NSApp setMainMenu:mainMenu];
}

/* --- Menu Actions --- */

- (void)showAbout:(id)sender {
    tt_mac_messagebox(NULL,
        "Tera Term for macOS\n"
        "Serial & Network Terminal Emulator\n\n"
        "Based on Tera Term Project\n"
        "https://teratermproject.github.io/",
        "About Tera Term", 0);
}

- (void)newConnection:(id)sender {
    [self showNewConnectionDialog];
}

- (void)disconnectSession:(id)sender {
    [self.session disconnect];
}

- (void)sendBreak:(id)sender {
    [self.session sendBreak];
}

- (void)startLog:(id)sender {
    if (self.session.logger && self.session.logger.logFile) {
        /* Already logging — offer to stop */
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"Logging is active";
        alert.informativeText = [NSString stringWithFormat:@"Log file: %@", self.session.logger.logPath];
        [alert addButtonWithTitle:@"Stop Log"];
        [alert addButtonWithTitle:@"Pause/Resume"];
        [alert addButtonWithTitle:@"Add Comment"];
        [alert addButtonWithTitle:@"Cancel"];
        NSModalResponse resp = [alert runModal];
        if (resp == NSAlertFirstButtonReturn) {
            [self.session.logger stop];
            self.session.logger = nil;
            [self.session updateStatus:@"Logging stopped"];
        } else if (resp == NSAlertSecondButtonReturn) {
            self.session.logger.paused = !self.session.logger.paused;
            [self.session updateStatus:self.session.logger.paused ? @"Log paused" : @"Logging"];
        } else if (resp == NSAlertThirdButtonReturn) {
            NSAlert *ca = [[NSAlert alloc] init];
            ca.messageText = @"Add Comment to Log";
            NSTextField *input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)];
            ca.accessoryView = input;
            [ca addButtonWithTitle:@"OK"];
            [ca addButtonWithTitle:@"Cancel"];
            if ([ca runModal] == NSAlertFirstButtonReturn) {
                [self.session.logger addComment:input.stringValue];
            }
        }
        return;
    }

    /* Start new log */
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = @"teraterm.log";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"log"]];

    if ([panel runModal] == NSModalResponseOK) {
        self.session.logger = [[TTSessionLogger alloc] init];
        BOOL ok = [self.session.logger startWithPath:panel.URL.path append:NO timestamps:YES];
        if (ok) {
            [self.session updateStatus:[NSString stringWithFormat:@"Logging to %@",
                                        panel.URL.lastPathComponent]];
        } else {
            tt_mac_messagebox(self.session.window, "Failed to open log file.", "Error", 0);
            self.session.logger = nil;
        }
    }
}

- (void)setupTerminal:(id)sender {
    NSAlert *dialog = [[NSAlert alloc] init];
    dialog.messageText = @"Terminal Setup";

    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 130)];

    [self addLabel:@"Columns:" at:NSMakePoint(10, 100) to:view];
    NSTextField *colsField = [[NSTextField alloc] initWithFrame:NSMakeRect(120, 98, 60, 24)];
    colsField.intValue = self.session.termCols;
    [view addSubview:colsField];

    [self addLabel:@"Rows:" at:NSMakePoint(10, 70) to:view];
    NSTextField *rowsField = [[NSTextField alloc] initWithFrame:NSMakeRect(120, 68, 60, 24)];
    rowsField.intValue = self.session.termRows;
    [view addSubview:rowsField];

    [self addLabel:@"Scrollback:" at:NSMakePoint(10, 40) to:view];
    NSTextField *sbField = [[NSTextField alloc] initWithFrame:NSMakeRect(120, 38, 80, 24)];
    sbField.intValue = 10000;
    [view addSubview:sbField];

    dialog.accessoryView = view;
    [dialog addButtonWithTitle:@"OK"];
    [dialog addButtonWithTitle:@"Cancel"];

    if ([dialog runModal] == NSAlertFirstButtonReturn) {
        int newCols = colsField.intValue;
        int newRows = rowsField.intValue;
        if (newCols >= 10 && newCols <= 500 && newRows >= 5 && newRows <= 200) {
            self.session.termCols = newCols;
            self.session.termRows = newRows;
            termbuf_t *tb = tt_mac_termview_get_termbuf(self.session.termView);
            if (tb) {
                termbuf_resize(tb, newCols, newRows);
                termbuf_set_scrollback_size(tb, sbField.intValue);
            }
            tt_mac_termview_invalidate(self.session.termView);
        }
    }
}

- (void)addLabel:(NSString *)text at:(NSPoint)pt to:(NSView *)view {
    NSTextField *label = [NSTextField labelWithString:text];
    label.frame = NSMakeRect(pt.x, pt.y, 100, 18);
    label.alignment = NSTextAlignmentRight;
    [view addSubview:label];
}

- (void)setupSerial:(id)sender {
    /* Show current serial parameters if connected, or let user change defaults */
    NSAlert *dialog = [[NSAlert alloc] init];
    dialog.messageText = @"Serial Port Setup";

    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 100)];

    [self addLabel:@"Baud Rate:" at:NSMakePoint(10, 70) to:view];
    NSPopUpButton *baudPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(120, 68, 150, 26) pullsDown:NO];
    for (NSString *b in @[@"9600", @"19200", @"38400", @"57600", @"115200", @"230400", @"460800", @"921600"])
        [baudPopup addItemWithTitle:b];
    [baudPopup selectItemWithTitle:@"9600"];
    [view addSubview:baudPopup];

    [self addLabel:@"Flow Control:" at:NSMakePoint(10, 35) to:view];
    NSPopUpButton *flowPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(120, 33, 150, 26) pullsDown:NO];
    [flowPopup addItemWithTitle:@"None"];
    [flowPopup addItemWithTitle:@"XON/XOFF"];
    [flowPopup addItemWithTitle:@"RTS/CTS"];
    [view addSubview:flowPopup];

    dialog.accessoryView = view;
    [dialog addButtonWithTitle:@"OK"];
    [dialog addButtonWithTitle:@"Cancel"];
    [dialog runModal];
}

- (void)setupFont:(id)sender {
    NSFontManager *fm = [NSFontManager sharedFontManager];
    NSFont *current = [NSFont fontWithName:self.session.fontFamily size:self.session.fontSize];
    if (!current) current = [NSFont monospacedSystemFontOfSize:14 weight:NSFontWeightRegular];
    [fm setSelectedFont:current isMultiple:NO];
    [fm orderFrontFontPanel:self];
}

- (void)changeFont:(id)sender {
    NSFont *newFont = [sender convertFont:[NSFont fontWithName:self.session.fontFamily
                                                          size:self.session.fontSize]];
    if (newFont) {
        self.session.fontFamily = newFont.familyName;
        self.session.fontSize = (int)newFont.pointSize;
        tt_mac_termview_set_font(self.session.termView,
                                  newFont.familyName.UTF8String,
                                  (int)newFont.pointSize, 0, 0);
    }
}

- (void)showHelp:(id)sender {
    NSURL *url = [NSURL URLWithString:@"https://teratermproject.github.io/"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

/* --- Edit menu actions --- */

- (void)pasteCR:(id)sender {
    char *text = tt_mac_clipboard_get_text();
    if (text && self.session.connection) {
        /* Append CR to each line */
        for (char *p = text; *p; p++) {
            char ch = *p;
            if (ch == '\n') {
                const char cr = '\r';
                tt_conn_write(self.session.connection, &cr, 1);
            }
            tt_conn_write(self.session.connection, &ch, 1);
        }
        tt_mac_clipboard_free(text);
    }
}

- (void)selectScreen:(id)sender {
    tt_mac_termview_select_all(self.session.termView);
}

- (void)cancelSelection:(id)sender {
    tt_mac_termview_clear_selection(self.session.termView);
}

- (void)clearScreen:(id)sender {
    termbuf_t *tb = tt_mac_termview_get_termbuf(self.session.termView);
    if (tb) termbuf_clear_screen(tb);
    tt_mac_termview_invalidate(self.session.termView);
}

- (void)clearBuffer:(id)sender {
    termbuf_t *tb = tt_mac_termview_get_termbuf(self.session.termView);
    if (tb) termbuf_clear_buffer(tb);
    tt_mac_termview_invalidate(self.session.termView);
}

/* --- Control menu actions --- */

- (void)resetTerminal:(id)sender {
    termbuf_t *tb = tt_mac_termview_get_termbuf(self.session.termView);
    if (tb) termbuf_reset(tb);
    tt_mac_window_set_title(self.session.window, "Tera Term");
    tt_mac_termview_invalidate(self.session.termView);
}

- (void)resetRemoteTitle:(id)sender {
    tt_mac_window_set_title(self.session.window, "Tera Term");
}

- (void)areYouThere:(id)sender {
    if (self.session.connection) {
        /* Telnet AYT: IAC AYT */
        const unsigned char ayt[] = { 0xFF, 0xF6 };
        tt_conn_write(self.session.connection, ayt, 2);
    }
}

- (void)resetPort:(id)sender {
    if (self.session.connection) {
        [self.session disconnect];
        /* Small delay then reconnect dialog */
        [self performSelector:@selector(showNewConnectionDialog) withObject:nil afterDelay:0.2];
    }
}

/* --- File transfer actions (basic send/receive) --- */

- (void)sendFile:(id)sender {
    if (!self.session.connection) return;

    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;

    if ([panel runModal] == NSModalResponseOK) {
        NSData *data = [NSData dataWithContentsOfURL:panel.URL];
        if (data) {
            /* Send raw file data in chunks */
            const char *bytes = data.bytes;
            NSUInteger remaining = data.length;
            NSUInteger offset = 0;
            while (remaining > 0) {
                int chunk = (remaining > 4096) ? 4096 : (int)remaining;
                tt_conn_write(self.session.connection, bytes + offset, chunk);
                offset += chunk;
                remaining -= chunk;
            }
            [self.session updateStatus:[NSString stringWithFormat:@"Sent %lu bytes",
                                        (unsigned long)data.length]];
        }
    }
}

- (void)receiveFile:(id)sender {
    /* For raw receive, we just enable logging to a file temporarily */
    tt_mac_messagebox(self.session.window,
        "Use File > Log to capture received data to a file.\n"
        "For protocol transfers (XMODEM/ZMODEM), use the appropriate protocol command from the remote host.",
        "Receive File", 0);
}

/* --- Setup save/restore --- */

- (void)saveSetup:(id)sender {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = @"teraterm.ini";
    if ([panel runModal] == NSModalResponseOK) {
        /* Write basic INI-style config */
        NSMutableString *ini = [NSMutableString string];
        [ini appendString:@"[Tera Term]\n"];
        [ini appendFormat:@"TerminalCols=%d\n", self.session.termCols];
        [ini appendFormat:@"TerminalRows=%d\n", self.session.termRows];
        [ini appendFormat:@"FontFamily=%@\n", self.session.fontFamily];
        [ini appendFormat:@"FontSize=%d\n", self.session.fontSize];
        [ini writeToURL:panel.URL atomically:YES encoding:NSUTF8StringEncoding error:nil];
        [self.session updateStatus:@"Setup saved"];
    }
}

- (void)restoreSetup:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    if ([panel runModal] == NSModalResponseOK) {
        NSString *content = [NSString stringWithContentsOfURL:panel.URL encoding:NSUTF8StringEncoding error:nil];
        if (content) {
            for (NSString *line in [content componentsSeparatedByString:@"\n"]) {
                NSArray *parts = [line componentsSeparatedByString:@"="];
                if (parts.count != 2) continue;
                NSString *key = [parts[0] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
                NSString *val = [parts[1] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
                if ([key isEqualToString:@"TerminalCols"]) self.session.termCols = val.intValue;
                else if ([key isEqualToString:@"TerminalRows"]) self.session.termRows = val.intValue;
                else if ([key isEqualToString:@"FontFamily"]) self.session.fontFamily = val;
                else if ([key isEqualToString:@"FontSize"]) self.session.fontSize = val.intValue;
            }
            /* Apply settings */
            termbuf_t *tb = tt_mac_termview_get_termbuf(self.session.termView);
            if (tb) termbuf_resize(tb, self.session.termCols, self.session.termRows);
            tt_mac_termview_set_font(self.session.termView,
                                      self.session.fontFamily.UTF8String,
                                      self.session.fontSize, 0, 0);
            tt_mac_termview_invalidate(self.session.termView);
            [self.session updateStatus:@"Setup restored"];
        }
    }
}

/* --- Validate menu items --- */
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;
    BOOL connected = (self.session.connection != NULL);

    if (action == @selector(disconnectSession:) ||
        action == @selector(sendBreak:) ||
        action == @selector(areYouThere:) ||
        action == @selector(resetPort:) ||
        action == @selector(sendFile:)) {
        return connected;
    }
    return YES;
}

@end

/* ===================================================================
 * Main Entry Point
 * =================================================================== */

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];

        TTMacAppDelegate *delegate = [[TTMacAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        tt_mac_net_init();

        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];

        tt_mac_net_cleanup();
    }
    return 0;
}
