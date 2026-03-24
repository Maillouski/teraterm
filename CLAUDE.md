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

## Architecture

### Connection Flow (Windows reference)

1. User opens app → "New Connection" dialog appears
2. User selects connection type (Serial, TCP/IP) and parameters
3. `CommOpen()` in `commlib.c` initializes the connection:
   - Serial: opens COM port, configures DCB (baud, parity, flow control)
   - TCP/IP: async DNS resolution → socket → connect
4. `WM_USER_COMMOPEN` → socket/port ready
5. `WM_USER_COMMSTART` → Telnet/SSH negotiation if applicable
6. Main loop: `CommReceive()` → VT parser (`vtterm.c`) → display (`vtdisp.c`)
7. Keyboard input → `CommSend()` → socket/serial

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
| `teraterm/teraterm/commlib.c` | CommOpen, CommReceive, CommSend — connection manager |
| `teraterm/teraterm/telnet.c` | Telnet protocol negotiation |
| `teraterm/teraterm/vtterm.c` | VT100/VT220 escape sequence parser |
| `teraterm/teraterm/vtdisp.c` | Terminal display rendering (Windows GDI) |
| `teraterm/teraterm/buffer.c` | Screen buffer management |
| `teraterm/common/tttypes.h` | Core type definitions (TTTSet, TComVar) |
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

- XMODEM, YMODEM, ZMODEM, Kermit
- Send/receive files over serial or network connections

### Plugin System

Extensible via TTX plugins (DLLs on Windows):
- TTXSSH — SSH2 client
- TTProxy — Proxy/firewall support
- TTXKanjiMenu — Character encoding menu

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
- `macos/macos_window.m` — Cocoa window/view (TTTerminalView with NSView)
- `macos/macos_serial.c` — Serial port via POSIX termios (`/dev/tty.*`)
- `macos/macos_net.c` — BSD sockets (replacing Winsock)
- `macos/macos_pty.c` — PTY support (for local shell, low priority)
- `macos/macos_termbuf.c` — Temporary VT100 screen buffer (standalone)

### Current State & Next Steps

**Done:**
- CMake cross-platform build system
- Platform type shims (Windows types → POSIX)
- GDI function stubs
- Basic Cocoa window with NSView terminal rendering
- Standalone VT100 parser + screen buffer (macos_termbuf.c)
- Serial port backend (macos_serial.c via termios)
- Network backend (macos_net.c via BSD sockets)

**TODO — Critical path to working serial terminal:**
1. Replace auto-launch-zsh with a **New Connection dialog** (serial/TCP)
2. Wire `CommOpen()` / `CommReceive()` / `CommSend()` to macOS backends
3. Connect `vtterm.c` (the real VT parser) to the display pipeline
4. Implement real GDI→CoreGraphics bridge (BeginPaint/TextOut/etc.)
   so vtdisp.c renders properly, OR keep macos_termbuf.c as renderer
5. Serial port enumeration UI (list `/dev/tty.*` devices)
6. Telnet connection support via BSD sockets

## Coding Guidelines

- C11 for .c files, C++17 for .cpp files, Objective-C with ARC for .m files
- Keep Windows code unchanged; use `#ifdef TT_PLATFORM_MACOS` for macOS paths
- Platform shims go in `teraterm/platform/`, not in core source files
- Minimize dependencies — use system frameworks (Cocoa, IOKit, Security)
- Test compile changes with: `clang -std=gnu11 -I teraterm/platform -I teraterm/platform/macos -I teraterm/common -I teraterm/teraterm -DTT_PLATFORM_MACOS=1 -c <file>`
