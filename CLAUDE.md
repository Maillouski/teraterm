# CLAUDE.md - Tera Term macOS Port

## Project Overview

**Tera Term** is a free, open-source **serial/network terminal emulator** for Windows,
originally created in 1994 by T. Teranishi. This fork is porting it to **macOS (ARM64/x86_64)**.

Tera Term is **NOT** a local shell terminal (like Terminal.app or iTerm2).
It is a **client for remote/serial connections**:

### Primary Connection Types (by priority)

1. **Serial (RS-232)** — COM/tty port connections (the core use case)
   - Configurable baud rate, parity, data bits, stop bits, flow control
   - On macOS: `/dev/tty.*` and `/dev/cu.*` devices
2. **Telnet** — RFC 854 telnet protocol with option negotiation
3. **SSH** — Via TTXSSH plugin (OpenSSL/LibreSSL, optional, build with `-DTTXSSH=ON`)
4. **Raw TCP/IP** — Direct socket connections
5. **Named Pipes** — Windows-only, not relevant for macOS

### What it is NOT

- It is NOT a local shell terminal. CygTerm (Cygwin shell bridge) was a Windows
  convenience feature, not a core function.
- The macOS port should open with a **"New Connection" dialog** asking for
  host/port/serial-device, NOT auto-launch zsh.

## Windows Feature Inventory & macOS Parity Status

### File Menu Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| New Connection dialog | `ttdialog.c` | ✅ Done (`teraterm_macos.m` TTConnectionDialog) |
| Duplicate Session | `ttdialog.c` | ❌ Not implemented |
| Cygwin Connection | Windows-only | N/A (skip) |
| **Log** — start/stop/pause/comment/view | `ttfileio.c`, `filesys_win32.cpp` | ✅ Done (TTSessionLogger) |
| Log rotation (size-based) | `ttfileio.c` | ❌ Not implemented |
| Log timestamps (local/UTC/elapsed) | `ttfileio.c` | ✅ Done (local time) |
| **Send File** (raw text) | `filesys_win32.cpp` | ✅ Done (raw send) |
| **Receive File** | `filesys_win32.cpp` | ⚠️ Via logging |
| **XMODEM** send/receive | `ttpfile/xmodem.c` | ❌ Not implemented |
| **YMODEM** send/receive | `ttpfile/ymodem.c` | ❌ Not implemented |
| **ZMODEM** send/receive | `ttpfile/zmodem.c` | ❌ Not implemented |
| **Kermit** send/receive/get/finish | `ttpfile/kermit.c` | ❌ Not implemented |
| **B-Plus** send/receive | `ttpfile/bplus.c` | ❌ Not implemented |
| **Quick-VAN** send/receive | `ttpfile/quickvan.c` | ❌ Not implemented |
| Change Directory | `ttdialog.c` | ❌ Not implemented |
| Replay Log | `ttfileio.c` | ❌ Not implemented |
| Print | Windows GDI printing | N/A (skip on macOS) |
| Disconnect | `commlib.c` | ✅ Done |
| Exit / Exit All | window management | ✅ / ❌ |

### Edit Menu Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Copy | `clipboar.c` | ✅ Done (text selection → clipboard) |
| Copy Table | `clipboar.c` | ❌ Not implemented |
| Paste | `clipboar.c` | ✅ Done (with bracketed paste support) |
| Paste with CR | `clipboar.c` | ✅ Done |
| Clear Screen | `vtdisp.c`, `buffer.c` | ✅ Done |
| Clear Buffer | `buffer.c` | ✅ Done |
| Cancel Selection | `clipboar.c` | ✅ Done |
| Select Screen | `clipboar.c` | ✅ Done |
| Select All | `clipboar.c` | ✅ Done |

### Setup Menu Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Terminal setup dialog | `ttdialog.c` | ✅ Done (cols/rows/scrollback) |
| Window setup dialog | `ttdialog.c` | ❌ Not implemented |
| Font setup dialog | `ttdialog.c` | ✅ Done (NSFontPanel) |
| Keyboard setup dialog | `ttdialog.c` | ❌ Not implemented |
| Serial Port setup dialog | `ttdialog.c` | ✅ Done (baud/flow) |
| TCP/IP setup dialog | `ttdialog.c` | ❌ Not implemented |
| General setup dialog | `ttdialog.c` | ❌ Not implemented |
| Additional Settings dialog | `ttdialog.c` | ❌ Not implemented |
| **Save Setup** (INI file) | `ttsetup.c` | ✅ Done (basic INI) |
| **Restore Setup** (INI file) | `ttsetup.c` | ✅ Done (basic INI) |
| Load Key Map | `keyboard.c` | ❌ Not implemented |

### Control Menu Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Reset Terminal | `vtterm.c` | ✅ Done |
| Reset Remote Title | `vtterm.c` | ✅ Done |
| Are You There (Telnet) | `telnet.c` | ✅ Done (IAC AYT) |
| Send Break | `commlib.c` | ✅ Done |
| Reset Port | `commlib.c` | ✅ Done (disconnect + reconnect) |
| Broadcast Command | `ttwinman.c` | ❌ Not implemented (multi-window) |
| Open TEK Window | `ttwinman.c` | ❌ Not implemented |
| Close TEK Window | `ttwinman.c` | ❌ Not implemented |
| Macro (run/show) | `ttpmacro/` | ❌ Not implemented |

### Help Menu Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Help Index | CHM help system | ✅ Opens project website |
| About | dialog | ✅ Done |

### Terminal Emulation Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| VT100 basic (cursor, erase, attributes) | `vtterm.c` | ✅ Basic in `macos_termbuf.c` |
| VT220 extensions | `vtterm.c` | ⚠️ Partial |
| VT320/VT420 extensions | `vtterm.c` | ❌ Not implemented |
| xterm extensions | `vtterm.c` | ⚠️ Partial (256-color) |
| **Scrollback buffer** | `buffer.c` | ✅ Done (10K lines ring buffer) |
| **Mouse tracking** (X10, VT200, SGR, etc.) | `vtterm.c` | ✅ Done |
| **Bracketed paste mode** | `vtterm.c` | ✅ Done |
| Window title setting (OSC 0/2) | `vtterm.c` | ✅ Done |
| Tektronix 4010 graphics (TEK) | `ttptek/` | ❌ Not implemented |
| Alternate screen buffer | `buffer.c` | ✅ Done |
| DEC special graphics charset | `vtterm.c` | ❌ Not implemented |
| ISO 2022 charset switching | `vtterm.c` | ❌ Not implemented |
| 8-bit control characters | `vtterm.c` | ❌ Not implemented |
| **Cursor shapes** (block/beam/underline) | `vtterm.c`, `vtdisp.c` | ✅ Done (DECSCUSR) |
| **Beep** (audible/visual) | `vtterm.c` | ✅ Done (audible via NSBeep) |
| **Auto-wrap** | `vtterm.c` | ✅ Done |
| **Scroll regions** | `vtterm.c` | ✅ Done |
| **Saved cursor** (DECSC/DECRC) | `vtterm.c` | ✅ Done |
| **Tab stops** (HTS/TBC) | `vtterm.c` | ✅ Done |
| **Insert/Replace mode** | `vtterm.c` | ✅ Done (IRM) |
| **Line mode** | `vtterm.c` | ❌ Not implemented |

### Keyboard/Input Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Basic key input | `keyboard.c` | ✅ Done |
| Function keys (F1-F12) | `keyboard.c` | ✅ Done (F1-F12, Insert, etc.) |
| Keypad modes (application/numeric) | `keyboard.c` | ❌ Not implemented |
| Custom key mapping (KEYBOARD.CNF) | `keyboard.c` | ❌ Not implemented |
| Meta key (Alt → ESC prefix) | `keyboard.c` | ✅ Done |
| IME support | `ttime.c` | ❌ Not implemented (macOS uses native input) |
| Paste confirmation dialog | `clipboar.c` | ❌ Not implemented |
| Paste delay (per char/line) | `clipboar.c` | ❌ Not implemented |

### Window/Display Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Basic terminal window | `vtdisp.c` | ✅ Done (NSView) |
| Window resize → terminal resize | `vtdisp.c` | ⚠️ Partial |
| **Alpha blending / transparency** | `vtdisp.c` | ❌ Not implemented |
| Background image | `vtdisp.c` | ❌ Not implemented |
| **Scrollbar** | `vtdisp.c` | ⚠️ Mouse wheel scrollback works, no visible scrollbar widget |
| Status bar | Windows status bar | ✅ Done (custom NSTextField) |
| Full-screen mode | `vtdisp.c` | ❌ Not implemented |
| Multi-window management | `ttwinman.c` | ❌ Not implemented |
| Window cascade/tile/stack | `ttwinman.c` | ❌ Not implemented |
| Clickable URLs | `vtdisp.c` | ❌ Not implemented |
| Drag & drop file send | `vtdisp.c` | ❌ Not implemented |

### Configuration/Persistence

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| **TERATERM.INI** save/load | `ttsetup.c` | ✅ Done (basic INI format) |
| **KEYBOARD.CNF** key mapping | `keyboard.c` | ❌ Not implemented |
| Command-line arguments | `teraterm.cpp` | ❌ Not implemented |
| Host history list | `ttsetup.c` | ❌ Not implemented |
| Setup file auto-backup | `ttsetup.c` | ❌ Not implemented |
| Jump list (recent connections) | `winjump.c` | ❌ Not implemented (use macOS Recent Items) |

### Macro System

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| TTL macro language | `ttpmacro/` | ❌ Not implemented |
| Macro file execution | `ttpmacro/` | ❌ Not implemented |
| Macro window (show/hide) | `ttpmacro/` | ❌ Not implemented |

### Network Protocol Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Telnet protocol negotiation | `telnet.c` | ⚠️ Basic TCP works, negotiation partial |
| Telnet option handling (ECHO, SGA, NAWS, etc.) | `telnet.c` | ❌ Not implemented |
| Telnet binary mode | `telnet.c` | ❌ Not implemented |
| Telnet keep-alive | `telnet.c` | ❌ Not implemented |
| SSH (via TTXSSH plugin) | `TTXSsh/` | ❌ Not implemented |
| SOCKS/HTTP proxy (via TTProxy) | `TTProxy/` | ❌ Not implemented |

### Serial Port Features

| Feature | Windows Source | macOS Status |
|---------|---------------|--------------|
| Port enumeration | `commlib.c` | ✅ Done (glob `/dev/tty.*`) |
| Baud rate (300-921600) | `commlib.c` | ✅ Done |
| Data bits (5-8) | `commlib.c` | ✅ Done |
| Parity (none/odd/even/mark/space) | `commlib.c` | ✅ Done (none/odd/even) |
| Stop bits (1/2) | `commlib.c` | ✅ Done |
| Flow control (none/XON-XOFF/RTS-CTS/DSR-DTR) | `commlib.c` | ✅ Done (none/XON/RTS) |
| Send break signal | `commlib.c` | ✅ Done |
| DTR/RTS control signals | `commlib.c` | ✅ Done |
| CTS/DSR reading | `commlib.c` | ✅ Done |
| Auto-reconnect on disconnect | `commlib.c` | ❌ Not implemented |
| Clear COM buffer on open | `commlib.c` | ❌ Not implemented |

## Architecture

### macOS Architecture (Diverged from Windows)

The macOS port **replaces** Windows-specific code with native macOS equivalents
rather than wrapping the original Windows code. This is a key architectural decision.

```
┌─────────────────────────────────────────────────────┐
│              teraterm_macos.m (Entry Point)          │
│   NSApplication + TTMacAppDelegate + Menu Bar       │
│   TTConnectionDialog (New Connection UI)            │
│   TTTerminalSession (session lifecycle)             │
├─────────────────────────────────────────────────────┤
│                  Connection Layer                    │
│   macos_connection.c  (unified serial/TCP/telnet)   │
│   ├── macos_serial.c  (termios)                     │
│   ├── macos_net.c     (BSD sockets)                 │
│   └── macos_pty.c     (forkpty, low priority)       │
├─────────────────────────────────────────────────────┤
│              Terminal Emulation Layer                │
│   macos_termbuf.c  (VT100/VT220/xterm parser)      │
│   - Standalone parser (not using vtterm.c)          │
│   - 256-color, scroll regions, saved cursor         │
│   - Scrollback buffer (ring buffer, 10K lines)      │
│   - Alternate screen buffer (CSI ?1049h/l)          │
│   - Mouse tracking (X10/VT200/SGR encoding)         │
│   - Bracketed paste, cursor shapes, insert mode     │
│   - OSC title, bell, logging callbacks              │
├─────────────────────────────────────────────────────┤
│                  Display Layer                       │
│   macos_window.m  (Cocoa NSView rendering)          │
│   - TTTerminalView: character cell grid → NSView    │
│   - Font management (Menlo default, font chooser)   │
│   - Keyboard input → output callback (F1-F12, Meta) │
│   - Text selection (mouse drag) + clipboard         │
│   - Scrollback viewport (mouse wheel navigation)    │
│   - Cursor shapes (block/beam/underline)            │
│   - Mouse event tracking → connection               │
├─────────────────────────────────────────────────────┤
│              Platform Abstraction Layer              │
│   platform_macos_types.h  (Windows type shims)      │
│   platform_stubs.c        (safe no-ops for unused)  │
│   platform_macos_compat.c (timer, sleep, threads)   │
│   macos_gui.h             (GUI API stubs)           │
└─────────────────────────────────────────────────────┘
```

### Connection Flow (macOS)

1. User opens app → `TTMacAppDelegate` creates 80x24 terminal window
2. New Connection dialog appears (modal, serial/TCP tabs)
3. User selects connection type and parameters
4. `TTTerminalSession` opens connection via `tt_conn_open_serial()` or `tt_conn_open_tcp()`
5. Timer-based polling (5ms) reads data: `tt_conn_read()` → `tt_mac_termview_write()` → VT parse → render
6. Keyboard input → `connection_output_callback()` → `tt_conn_write()`
7. Disconnect: invalidate timer, close connection, update status

### Key Data Structures (`tttypes.h`)

- **TTTSet** — Session configuration (terminal settings, port settings, window prefs)
- **TComVar (cv)** — Runtime communication state:
  - `PortType`: IdTCPIP (1), IdSerial (2), IdFile (3), IdNamedPipe (4)
  - `InBuff[16KB]` / `OutBuff[16KB]` — I/O buffers
  - `s` — Socket handle (TCP/IP)
  - `ComID` — COM port handle (Serial)
  - Telnet state flags, thread locks, etc.

### Key Source Files

| File | Purpose |
|------|---------|
| `teraterm/teraterm/commlib.c` | CommOpen, CommReceive, CommSend — Windows connection manager |
| `teraterm/teraterm/telnet.c` | Telnet protocol negotiation (Windows) |
| `teraterm/teraterm/vtterm.c` | VT100/VT220 escape sequence parser (Windows, not used on macOS yet) |
| `teraterm/teraterm/vtdisp.c` | Terminal display rendering (Windows GDI) |
| `teraterm/teraterm/buffer.c` | Screen buffer management (Windows) |
| `teraterm/teraterm/clipboar.c` | Clipboard operations (Windows) |
| `teraterm/teraterm/keyboard.c` | Keyboard handling & key mapping (Windows) |
| `teraterm/teraterm/ttsetup.c` | INI file read/write (Windows) |
| `teraterm/teraterm/ttdialog.c` | Setup dialogs (Windows) |
| `teraterm/teraterm/ttdde.c` | DDE communication (Windows-only) |
| `teraterm/teraterm/ttwinman.c` | Multi-window management (Windows) |
| `teraterm/teraterm/ttfileio.c` | File I/O and logging (Windows) |
| `teraterm/common/tttypes.h` | Core type definitions (TTTSet, TComVar) |
| `teraterm/ttpfile/xmodem.c` | XMODEM file transfer protocol |
| `teraterm/ttpfile/ymodem.c` | YMODEM file transfer protocol |
| `teraterm/ttpfile/zmodem.c` | ZMODEM file transfer protocol |
| `teraterm/ttpfile/kermit.c` | Kermit file transfer protocol |
| `teraterm/ttpfile/bplus.c` | B-Plus file transfer protocol |
| `teraterm/ttpfile/quickvan.c` | Quick-VAN file transfer protocol |
| `teraterm/ttpmacro/` | TTL macro language engine |
| `teraterm/ttptek/` | Tektronix 4010 graphics emulation |
| `teraterm/platform/` | macOS/cross-platform abstraction layer |
| `teraterm/platform/macos/` | macOS-specific implementations |
| `teraterm/teraterm/teraterm_macos.m` | macOS app entry point |

### Terminal Emulation

- VT100/VT220 compatible terminal emulator
- Tektronix 4010 graphics mode (TEK)
- ANSI colors (256-color), bold, underline, reverse, blink
- Japanese character set support (Shift-JIS, EUC-JP, UTF-8)
- Mouse tracking, window title setting, etc.

### File Transfer Protocols

- XMODEM, YMODEM, ZMODEM, Kermit, B-Plus, Quick-VAN
- Send/receive files over serial or network connections
- Protocol-specific options (CRC modes, window sizes, escape control chars)

### Plugin System

Extensible via TTX plugins (DLLs on Windows):
- TTXSSH — SSH2 client
- TTProxy — Proxy/firewall support
- TTXKanjiMenu — Character encoding menu
- On macOS: plugin system not yet ported (use dylib or built-in)

## macOS Port Status

### Build

```bash
mkdir build && cd build
cmake .. -G Ninja \
  -DARCHITECTURE=arm64 \
  -DCMAKE_TOOLCHAIN_FILE=../macos.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DTTXSSH=OFF \
  -DENABLE_TTXSAMPLES=OFF
cmake --build .
```

Or use: `./rebuild_macos.sh`

### Platform Layer (`teraterm/platform/`)

Maps Windows APIs to macOS equivalents:
- `platform_macos_types.h` — Windows type definitions (HWND, HDC, RECT, etc.)
- `platform_stubs.c` — Stub implementations for Windows GUI/GDI functions
- `platform_macos_compat.c` — Timer, sleep, thread ID, error tracking
- `macos/macos_window.m` — Cocoa window/view (TTTerminalView with NSView)
- `macos/macos_serial.c` — Serial port via POSIX termios (`/dev/tty.*`)
- `macos/macos_net.c` — BSD sockets (replacing Winsock)
- `macos/macos_connection.c` — Unified connection manager (serial/TCP/telnet)
- `macos/macos_pty.c` — PTY support (for local shell, low priority)
- `macos/macos_termbuf.c` — VT100 screen buffer + parser (standalone)
- `macos/macos_gui.h` — GUI API stub definitions

### Current State Summary

**Complete (production-ready):**
- CMake cross-platform build system with Ninja
- Platform type shims (Windows types → POSIX)
- Serial port backend (termios, 300-921600 baud, full config)
- Network/TCP backend (BSD sockets, async DNS)
- Unified connection manager
- Cocoa window with NSView terminal rendering
- VT100/VT220/xterm parser + screen buffer with 256-color support
- New Connection dialog (serial port / TCP tabs)
- Full application menu bar (File, Edit, Setup, Control, Help)
- Session lifecycle (connect, read timer, disconnect)
- Status bar
- **Session logging** (start/stop/pause, timestamps, comment-to-log)
- **Clipboard** (copy/paste with selection, paste with CR, bracketed paste)
- **Text selection** (mouse drag, select all, select screen)
- **Scrollback buffer** (10000 lines default, mouse wheel scroll)
- **Alternate screen buffer** (CSI ?1049h/l)
- **Cursor shapes** (block/beam/underline via DECSCUSR)
- **Mouse tracking** (X10, VT200, button event, all event, SGR encoding)
- **Function keys** (F1-F12, Insert, Delete, Home, End, PageUp/Down)
- **Meta/Alt key** (ESC prefix)
- **Setup dialogs** (Terminal size/scrollback, Serial Port, Font chooser)
- **Control menu** (Reset Terminal, Reset Remote Title, Are You There, Send Break, Reset Port)
- **INI file save/restore** (basic settings persistence)
- **Send File** (raw file send over connection)
- **Bell** (NSBeep on BEL character)
- **Window title** via OSC 0/2 escape sequences

**Partially implemented / needs enhancement:**
- VT220+ (some sequences missing: DCS, charset switching)
- Telnet protocol negotiation (basic TCP works, option negotiation not done)
- Window resize → terminal resize + SIGWINCH
- Scrollbar widget (scrollback works via mouse wheel, no visible scrollbar yet)

**Not implemented (priority order):**
1. File transfer protocols (XMODEM, YMODEM, ZMODEM, Kermit, B-Plus)
2. Telnet option negotiation (ECHO, SGA, NAWS, etc.)
3. Multi-window support
4. Clickable URLs
5. Alpha blending / window transparency
6. DEC special graphics charset
7. ISO 2022 charset switching
8. Host history list
9. Keypad modes (application/numeric)
10. Custom key mapping (KEYBOARD.CNF)
11. Macro system (TTL language)
12. SSH support
13. Tektronix 4010 graphics (TEK)
14. Drag & drop file send

## Coding Guidelines

- C11 for .c files, C++17 for .cpp files, Objective-C with ARC for .m files
- Keep Windows code unchanged; use `#ifdef TT_PLATFORM_MACOS` for macOS paths
- Platform shims go in `teraterm/platform/`, not in core source files
- New macOS features go in `teraterm/platform/macos/` or `teraterm_macos.m`
- Minimize dependencies — use system frameworks (Cocoa, IOKit, Security)
- Test compile changes with: `clang -std=gnu11 -I teraterm/platform -I teraterm/platform/macos -I teraterm/common -I teraterm/teraterm -DTT_PLATFORM_MACOS=1 -c <file>`
