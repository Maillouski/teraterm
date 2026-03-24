/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * macOS main entry point for Tera Term.
 * Implements the Cocoa application lifecycle, New Connection dialog,
 * and bridges serial/TCP connections to the terminal emulator view.
 */

#import <Cocoa/Cocoa.h>
#include "macos_window.h"
#include "macos_connection.h"
#include "macos_serial.h"
#include "macos_net.h"

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
@end

@implementation TTTerminalSession

- (instancetype)init {
    self = [super init];
    if (self) {
        _window = NULL;
        _termView = NULL;
        _connection = NULL;
    }
    return self;
}

- (void)createWindowWithTitle:(NSString *)title cols:(int)cols rows:(int)rows {
    TTMacFont font = tt_mac_font_create("Menlo", 14, 0, 0);
    int charWidth = 0, charHeight = 0, ascent = 0;
    tt_mac_font_get_metrics(font, &charWidth, &charHeight, &ascent);
    tt_mac_font_destroy(font);

    int width = cols * charWidth + 20;
    /* Extra 24px for status bar at bottom */
    int height = rows * charHeight + 20 + 24;

    self.window = tt_mac_window_create(100, 100, width, height, [title UTF8String]);

    /* Create the terminal view (fills window minus status bar) */
    self.termView = tt_mac_termview_create(self.window);
    tt_mac_termview_set_font(self.termView, "Menlo", 14, 0, 0);
    tt_mac_termview_set_colors(self.termView, 0x00FFFFFF, 0x00000000);

    /* Status bar at bottom of window */
    NSWindow *nsWin = (__bridge NSWindow *)self.window;
    NSRect contentBounds = nsWin.contentView.bounds;
    self.statusField = [[NSTextField alloc] initWithFrame:
        NSMakeRect(0, contentBounds.size.height - 22, contentBounds.size.width, 20)];
    self.statusField.editable = NO;
    self.statusField.bordered = NO;
    self.statusField.drawsBackground = YES;
    self.statusField.backgroundColor = [NSColor colorWithCalibratedWhite:0.15 alpha:1.0];
    self.statusField.textColor = [NSColor colorWithCalibratedWhite:0.7 alpha:1.0];
    self.statusField.font = [NSFont systemFontOfSize:11];
    self.statusField.stringValue = @"Not connected";
    self.statusField.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [nsWin.contentView addSubview:self.statusField];
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

- (void)connectWith:(TTMacConnection)conn {
    if (!conn) return;
    self.connection = conn;

    /* Set up keyboard → connection output path */
    tt_mac_termview_set_output_cb(self.termView, connection_output_callback,
                                   (__bridge void *)self);

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
    [fileMenu addItemWithTitle:@"Send Break"
                        action:@selector(sendBreak:)
                 keyEquivalent:@"b"];
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
    [editMenu addItemWithTitle:@"Select All"
                        action:@selector(selectAll:)
                 keyEquivalent:@"a"];
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
    setupMenuItem.submenu = setupMenu;
    [mainMenu addItem:setupMenuItem];

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
    /* TODO: Implement logging */
}

- (void)setupTerminal:(id)sender { /* TODO */ }
- (void)setupSerial:(id)sender { /* TODO */ }
- (void)setupFont:(id)sender { /* TODO */ }
- (void)showHelp:(id)sender { /* TODO */ }

/* --- Validate menu items --- */
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if (menuItem.action == @selector(disconnectSession:) ||
        menuItem.action == @selector(sendBreak:)) {
        return (self.session.connection != NULL);
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
