/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * Windows API type definitions for macOS/POSIX platforms.
 *
 * Provides the fundamental Windows types (HWND, HINSTANCE, BOOL, DWORD, etc.)
 * as POSIX-compatible equivalents so that Tera Term source code can compile
 * on non-Windows platforms.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Basic integer types --- */
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef unsigned int        UINT;
typedef int                 INT;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef char                CHAR;
typedef unsigned char       UCHAR;
typedef wchar_t             WCHAR;
typedef float               FLOAT;

typedef int64_t             LONGLONG;
typedef uint64_t            ULONGLONG;
typedef intptr_t            LONG_PTR;
typedef uintptr_t           ULONG_PTR;
typedef uintptr_t           DWORD_PTR;
typedef intptr_t            INT_PTR;
typedef uintptr_t           UINT_PTR;

typedef LONG_PTR            LRESULT;
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;

typedef LONG                LSTATUS;
typedef int                 errno_t;
typedef void*               PVOID;

typedef BOOL*               PBOOL;

/* --- Pointer types --- */
typedef char*               LPSTR;
typedef char*               PCHAR;
typedef const char*         LPCSTR;
typedef wchar_t*            LPWSTR;
typedef const wchar_t*      LPCWSTR;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef BYTE*               LPBYTE;
typedef WORD*               LPWORD;
typedef DWORD*              LPDWORD;
typedef BOOL*               LPBOOL;

/* --- Handle types (opaque pointers on non-Windows) --- */
typedef void*               HANDLE;
typedef void*               HWND;
typedef void*               HDC;
typedef void*               HINSTANCE;
typedef void*               HMODULE;
typedef void*               HGLOBAL;
typedef void*               HLOCAL;
typedef void*               HBITMAP;
typedef void*               HBRUSH;
typedef void*               HFONT;
typedef void*               HICON;
typedef void*               HCURSOR;
typedef void*               HPEN;
typedef void*               HRGN;
typedef void*               HPALETTE;
typedef void*               HMENU;
typedef void*               HACCEL;
typedef void*               HGDIOBJ;
typedef void*               HKEY;
typedef void*               HRSRC;
typedef void*               HMONITOR;

/* --- Special constants --- */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL  ((void*)0)
#endif

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

/* --- Calling conventions (no-op on non-Windows) --- */
#define WINAPI
#define CALLBACK
#define APIENTRY
#define PASCAL
#define CDECL
#define __stdcall
#define _stdcall
#define __cdecl
#define __fastcall
#define DECLSPEC_IMPORT
#define __declspec(x)

/* --- Text macros --- */
#ifdef UNICODE
  typedef WCHAR             TCHAR;
  #define _T(x)             L##x
  #define TEXT(x)            L##x
#else
  typedef CHAR              TCHAR;
  #define _T(x)             x
  #define TEXT(x)            x
#endif

typedef TCHAR*              LPTSTR;
typedef const TCHAR*        LPCTSTR;

/* --- RECT, POINT, SIZE --- */
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;
typedef const RECT* LPCRECT;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *LPPOINT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *LPSIZE;

/* --- PAINTSTRUCT --- */
typedef struct tagPAINTSTRUCT {
    HDC  hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *LPPAINTSTRUCT;

/* --- LOGFONT --- */
#define LF_FACESIZE 32
typedef struct tagLOGFONTW {
    LONG  lfHeight;
    LONG  lfWidth;
    LONG  lfEscapement;
    LONG  lfOrientation;
    LONG  lfWeight;
    BYTE  lfItalic;
    BYTE  lfUnderline;
    BYTE  lfStrikeOut;
    BYTE  lfCharSet;
    BYTE  lfOutPrecision;
    BYTE  lfClipPrecision;
    BYTE  lfQuality;
    BYTE  lfPitchAndFamily;
    WCHAR lfFaceName[LF_FACESIZE];
} LOGFONTW, *LPLOGFONTW;

typedef struct tagLOGFONTA {
    LONG  lfHeight;
    LONG  lfWidth;
    LONG  lfEscapement;
    LONG  lfOrientation;
    LONG  lfWeight;
    BYTE  lfItalic;
    BYTE  lfUnderline;
    BYTE  lfStrikeOut;
    BYTE  lfCharSet;
    BYTE  lfOutPrecision;
    BYTE  lfClipPrecision;
    BYTE  lfQuality;
    BYTE  lfPitchAndFamily;
    CHAR  lfFaceName[LF_FACESIZE];
} LOGFONTA, *LPLOGFONTA;

typedef LOGFONTA* PLOGFONTA;
typedef LOGFONTW* PLOGFONTW;

#ifdef UNICODE
typedef LOGFONTW* PLOGFONT;
#else
typedef LOGFONTA* PLOGFONT;
#endif

#ifdef UNICODE
typedef LOGFONTW LOGFONT;
#else
typedef LOGFONTA LOGFONT;
#endif

/* --- COLORREF --- */
typedef DWORD COLORREF;
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb) >> 16))

/* --- MSG --- */
typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *LPMSG;

/* --- Common Windows message constants --- */
#define WM_NULL           0x0000
#define WM_CREATE         0x0001
#define WM_DESTROY        0x0002
#define WM_MOVE           0x0003
#define WM_SIZE           0x0005
#define WM_ACTIVATE       0x0006
#define WM_SETFOCUS       0x0007
#define WM_KILLFOCUS      0x0008
#define WM_ENABLE         0x000A
#define WM_PAINT          0x000F
#define WM_CLOSE          0x0010
#define WM_QUIT           0x0012
#define WM_ERASEBKGND     0x0014
#define WM_SHOWWINDOW     0x0018
#define WM_SETTEXT        0x000C
#define WM_GETTEXT        0x000D
#define WM_GETTEXTLENGTH  0x000E
#define WM_TIMER          0x0113
#define WM_HSCROLL        0x0114
#define WM_VSCROLL        0x0115
#define WM_COMMAND        0x0111
#define WM_SYSCOMMAND     0x0112
#define WM_CHAR           0x0102
#define WM_KEYDOWN        0x0100
#define WM_KEYUP          0x0101
#define WM_SYSKEYDOWN     0x0104
#define WM_SYSKEYUP       0x0105
#define WM_MOUSEMOVE      0x0200
#define WM_LBUTTONDOWN    0x0201
#define WM_LBUTTONUP      0x0202
#define WM_LBUTTONDBLCLK  0x0203
#define WM_RBUTTONDOWN    0x0204
#define WM_RBUTTONUP      0x0205
#define WM_RBUTTONDBLCLK  0x0206
#define WM_MBUTTONDOWN    0x0207
#define WM_MBUTTONUP      0x0208
#define WM_MOUSEWHEEL     0x020A
#define WM_DROPFILES      0x0233
#define WM_USER           0x0400
#define WM_COPYDATA       0x004A
#define WM_NOTIFY         0x004E
#define WM_INITDIALOG     0x0110
#define WM_CTLCOLORDLG    0x0136
#define WM_CTLCOLORSTATIC 0x0138
#define WM_SETFONT        0x0030
#define WM_GETFONT        0x0031
#define WM_DRAWITEM       0x002B
#define WM_MEASUREITEM    0x002C
#define WM_DELETEITEM     0x002D
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED  0x0047
#define WM_NCCREATE       0x0081
#define WM_NCDESTROY      0x0082
#define WM_NCHITTEST      0x0084
#define WM_NCPAINT        0x0085
#define WM_GETMINMAXINFO  0x0024
#define WM_ENTERSIZEMOVE  0x0231
#define WM_EXITSIZEMOVE   0x0232
#define WM_SETTINGCHANGE  0x001A
#define WM_DPICHANGED     0x02E0
#define WM_IME_COMPOSITION 0x010F
#define WM_IME_STARTCOMPOSITION 0x010D
#define WM_IME_ENDCOMPOSITION   0x010E
#define WM_IME_NOTIFY     0x0282

/* --- Window Styles --- */
#define WS_OVERLAPPED     0x00000000L
#define WS_POPUP          0x80000000L
#define WS_CHILD          0x40000000L
#define WS_MINIMIZE       0x20000000L
#define WS_VISIBLE        0x10000000L
#define WS_DISABLED       0x08000000L
#define WS_CLIPSIBLINGS   0x04000000L
#define WS_CLIPCHILDREN   0x02000000L
#define WS_MAXIMIZE       0x01000000L
#define WS_CAPTION        0x00C00000L
#define WS_BORDER         0x00800000L
#define WS_DLGFRAME       0x00400000L
#define WS_VSCROLL        0x00200000L
#define WS_HSCROLL        0x00100000L
#define WS_SYSMENU        0x00080000L
#define WS_THICKFRAME     0x00040000L
#define WS_GROUP          0x00020000L
#define WS_TABSTOP        0x00010000L
#define WS_MINIMIZEBOX    0x00020000L
#define WS_MAXIMIZEBOX    0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

/* --- Extended Window Styles --- */
#define WS_EX_TOPMOST     0x00000008L
#define WS_EX_ACCEPTFILES 0x00000010L
#define WS_EX_TRANSPARENT 0x00000020L

/* --- Window show commands --- */
#define SW_HIDE           0
#define SW_SHOWNORMAL     1
#define SW_NORMAL         1
#define SW_SHOWMINIMIZED  2
#define SW_SHOWMAXIMIZED  3
#define SW_MAXIMIZE       3
#define SW_SHOW           5
#define SW_MINIMIZE       6
#define SW_RESTORE        9

/* --- SetWindowPos flags --- */
#define SWP_NOSIZE        0x0001
#define SWP_NOMOVE        0x0002
#define SWP_NOZORDER      0x0004
#define SWP_NOACTIVATE    0x0010
#define SWP_SHOWWINDOW    0x0040
#define SWP_HIDEWINDOW    0x0080
#define HWND_TOP          ((HWND)0)
#define HWND_TOPMOST      ((HWND)-1)
#define HWND_NOTOPMOST    ((HWND)-2)

/* --- VK_ Virtual Key codes --- */
#define VK_BACK           0x08
#define VK_TAB            0x09
#define VK_RETURN         0x0D
#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14
#define VK_ESCAPE         0x1B
#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B

/* --- GDI constants --- */
#define TRANSPARENT       1
#define OPAQUE            2
#define DEFAULT_CHARSET   1
#define SYMBOL_CHARSET    2
#define SHIFTJIS_CHARSET  128
#define ANSI_CHARSET      0
#define FW_NORMAL         400
#define FW_BOLD           700

/* --- Clipboard formats --- */
#define CF_TEXT           1
#define CF_UNICODETEXT    13

/* --- SOCKET types (BSD sockets) --- */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

typedef int SOCKET;
#define INVALID_SOCKET  ((SOCKET)-1)
#define SOCKET_ERROR    (-1)
#define closesocket(s)  close(s)
#define WSAGetLastError() errno
#define WSAEWOULDBLOCK  EWOULDBLOCK
#define WSAEINPROGRESS  EINPROGRESS

/* --- Handle types (additional) --- */
typedef void*               HDROP;
typedef void*               HDEVINFO;

/* --- Dialog types --- */
typedef INT_PTR (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct { DWORD style; DWORD dwExtendedStyle; WORD cdit; short x; short y; short cx; short cy; } DLGTEMPLATE;
typedef const DLGTEMPLATE*  LPCDLGTEMPLATE;

/* --- Device info --- */
typedef struct _SP_DEVINFO_DATA {
    DWORD cbSize;
    DWORD ClassGuid[4];
    DWORD DevInst;
    ULONG_PTR Reserved;
} SP_DEVINFO_DATA, *PSP_DEVINFO_DATA;

typedef struct _DEVPROPKEY {
    DWORD fmtid[4];
    DWORD pid;
} DEVPROPKEY;

/* --- DECLARE_HANDLE macro --- */
#ifndef DECLARE_HANDLE
#define DECLARE_HANDLE(name) typedef void* name
#endif

/* --- Code page constants --- */
#define CP_ACP            0
#define CP_UTF8           65001

/* --- MultiByteToWideChar / WideCharToMultiByte flags --- */
#define MB_ERR_INVALID_CHARS  0x00000008
#define MB_PRECOMPOSED        0x00000001
#define WC_COMPOSITECHECK     0x00000200

/* --- File attribute constants --- */
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL   0x00000080
#define FILE_SHARE_READ         0x00000001
#define FILE_SHARE_WRITE        0x00000002
#define GENERIC_READ            0x80000000L
#define GENERIC_WRITE           0x40000000L
#define CREATE_ALWAYS           2
#define OPEN_EXISTING           3
#define INFINITE                0xFFFFFFFF

/* --- Dialog constants --- */
#define IDOK                1
#define IDCANCEL            2
#define IDYES               6
#define IDNO                7
#define MB_OK               0x00000000L
#define MB_OKCANCEL         0x00000001L
#define MB_YESNO            0x00000004L
#define MB_ICONEXCLAMATION  0x00000030L
#define MB_ICONINFORMATION  0x00000040L
#define MB_ICONQUESTION     0x00000020L
#define MAKEINTRESOURCE(i)  ((LPCSTR)(ULONG_PTR)(WORD)(i))

/* --- Secure string macros --- */
#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif
#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* --- _wcsdup mapping --- */
#ifndef _wcsdup
#define _wcsdup wcsdup
#endif

/* --- Secure string functions --- */
#include <limits.h>
#ifndef strncpy_s
static inline int strncpy_s_impl(char *dest, size_t destsz, const char *src, size_t count) {
    if (!dest || destsz == 0) return -1;
    if (!src) { dest[0] = '\0'; return -1; }
    size_t to_copy = (count == (size_t)-1) ? destsz - 1 : ((count < destsz - 1) ? count : destsz - 1);
    size_t i;
    for (i = 0; i < to_copy && src[i] != '\0'; i++) dest[i] = src[i];
    dest[i] = '\0';
    return 0;
}
#define strncpy_s(dest, destsz, src, count) strncpy_s_impl(dest, destsz, src, count)
#endif
#ifndef strncat_s
static inline int strncat_s_impl(char *dest, size_t destsz, const char *src, size_t count) {
    if (!dest || destsz == 0) return -1;
    size_t dlen = strlen(dest);
    if (dlen >= destsz) return -1;
    size_t remaining = destsz - dlen - 1;
    size_t to_copy = (count == (size_t)-1) ? remaining : ((count < remaining) ? count : remaining);
    size_t i;
    for (i = 0; i < to_copy && src[i] != '\0'; i++) dest[dlen + i] = src[i];
    dest[dlen + i] = '\0';
    return 0;
}
#define strncat_s(dest, destsz, src, count) strncat_s_impl(dest, destsz, src, count)
#endif
#ifndef _snprintf_s
#define _snprintf_s(buf, sz, trunc, ...) snprintf(buf, sz, __VA_ARGS__)
#endif

/* --- Locale-specific CRT functions --- */
#include <locale.h>
#if defined(__APPLE__)
  #include <xlocale.h>
  #ifndef _locale_t
  typedef locale_t _locale_t;
  #endif
  static inline _locale_t _create_locale(int category, const char *locale_name) {
      (void)category;
      return newlocale(LC_ALL_MASK, locale_name, (locale_t)0);
  }
  static inline void _free_locale(_locale_t locale) {
      if (locale) freelocale(locale);
  }
#else
  /* Linux: locale_t requires _GNU_SOURCE; provide simple stub */
  typedef void* _locale_t;
  static inline _locale_t _create_locale(int category, const char *locale_name) {
      (void)category; (void)locale_name;
      return NULL;
  }
  static inline void _free_locale(_locale_t locale) { (void)locale; }
#endif
/* _snprintf_s_l: locale-aware snprintf - just use snprintf on POSIX */
#define _snprintf_s_l(buf, sz, trunc, fmt, loc, ...) snprintf(buf, sz, fmt, ##__VA_ARGS__)

/* --- File API compatibility --- */
#define MAX_PATH          260

/* --- Critical section (mapped to pthread mutex) --- */
#include <pthread.h>
typedef pthread_mutex_t CRITICAL_SECTION;
#define InitializeCriticalSection(cs)  pthread_mutex_init(cs, NULL)
#define DeleteCriticalSection(cs)      pthread_mutex_destroy(cs)
#define EnterCriticalSection(cs)       pthread_mutex_lock(cs)
#define LeaveCriticalSection(cs)       pthread_mutex_unlock(cs)

/* --- min/max macros --- */
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* --- String functions compatibility --- */
#define _stricmp    strcasecmp
#define _strnicmp   strncasecmp
#define _wcsicmp    wcscasecmp
#define _wcsnicmp   wcsncasecmp
#define _strdup     strdup
#define _wcsdup     wcsdup
#define _snprintf   snprintf
#define _snwprintf  swprintf
#define sprintf_s   snprintf
#define _vsnprintf  vsnprintf
#define _vsnwprintf vswprintf
#define _vsnprintf_s(buf, sz, trunc, fmt, ap) vsnprintf(buf, sz, fmt, ap)
#define _vsnwprintf_s(buf, sz, trunc, fmt, ap) vswprintf(buf, sz, fmt, ap)
#define ZeroMemory(p, sz) memset((p), 0, (sz))
#define CopyMemory(d, s, sz) memcpy((d), (s), (sz))
#define MoveMemory(d, s, sz) memmove((d), (s), (sz))
#define FillMemory(p, sz, v) memset((p), (v), (sz))

/* --- HRESULT --- */
typedef LONG HRESULT;
#define S_OK        ((HRESULT)0L)
#define S_FALSE     ((HRESULT)1L)
#define E_FAIL      ((HRESULT)0x80004005L)
#define E_NOTIMPL   ((HRESULT)0x80004001L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) < 0)

/* --- DLL loading (mapped to dlopen/dlsym) --- */
#include <dlfcn.h>
#define LoadLibraryA(name)       dlopen(name, RTLD_LAZY)
#define LoadLibraryW(name)       dlopen_wide(name)
#define FreeLibrary(h)           dlclose(h)
#define GetProcAddress(h, name)  dlsym(h, name)

/* --- Misc --- */
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)(((DWORD_PTR)(w) >> 8) & 0xff))
#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
#define MAKEWORD(a, b) ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xff)) | ((WORD)((BYTE)((DWORD_PTR)(b) & 0xff))) << 8))

/* --- Monitor types --- */
typedef BOOL (CALLBACK *MONITORENUMPROC)(HMONITOR, HDC, LPRECT, LPARAM);

/* --- Cursor / Icon constants --- */
#define IDC_HAND          ((LPCSTR)32649)
#define IDC_ARROW         ((LPCSTR)32512)
#define IMAGE_CURSOR      2
#define IMAGE_ICON        1
#define LR_SHARED         0x8000
#define LR_DEFAULTSIZE    0x0040

/* Cursor macros */
#define CopyCursor(hcur)  ((HCURSOR)(hcur))
#define DestroyCursor(hcur) ((void)0)

/* --- GDI types --- */
typedef struct _BLENDFUNCTION {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
} BLENDFUNCTION;

/* --- Window function types --- */
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

/* --- Window API constants --- */
#define GWLP_USERDATA     (-21)
#define GWLP_WNDPROC      (-4)
#define GWL_STYLE         (-16)
#define GWL_EXSTYLE       (-20)

/* --- Listbox / Combobox messages --- */
#define CB_ADDSTRING      0x0143
#define CB_GETCURSEL      0x0147
#define CB_SETCURSEL      0x014E
#define CB_GETLBTEXT      0x0148
#define CB_GETCOUNT        0x0146
#define LB_ADDSTRING      0x0180
#define LB_GETCURSEL      0x0188
#define LB_SETCURSEL      0x0186
#define LB_GETTEXT        0x0189
#define LB_GETCOUNT       0x018B
#define LB_ERR            (-1)
#define CB_ERR            (-1)
#define CB_GETCOUNT       0x0146
#define CB_RESETCONTENT   0x014B
#define LB_RESETCONTENT   0x0184

/* --- Error codes --- */
#define NO_ERROR                    0L
#define ERROR_SUCCESS               0L
#define ERROR_INSUFFICIENT_BUFFER   122L

/* --- Secure CRT function mappings --- */
#ifndef _snwprintf_s
#define _snwprintf_s(buf, sz, trunc, ...) swprintf(buf, sz, __VA_ARGS__)
#endif
#ifndef _wfopen_s
static inline errno_t _wfopen_s_impl(FILE **pf, const wchar_t *filename, const wchar_t *mode) {
    char fn[1024], md[32];
    wcstombs(fn, filename, sizeof(fn));
    wcstombs(md, mode, sizeof(md));
    *pf = fopen(fn, md);
    return *pf ? 0 : errno;
}
#define _wfopen_s(pf, fn, mode) _wfopen_s_impl(pf, fn, mode)
#endif
#ifndef fopen_s
static inline errno_t fopen_s_impl(FILE **pf, const char *filename, const char *mode) {
    *pf = fopen(filename, mode);
    return *pf ? 0 : errno;
}
#define fopen_s(pf, fn, mode) fopen_s_impl(pf, fn, mode)
#endif
#ifndef swscanf_s
#define swscanf_s swscanf
#endif

/* --- Window functions (stubs) --- */
LONG_PTR GetWindowLongPtrW(HWND hWnd, int nIndex);
LONG_PTR SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
HWND GetDlgItem(HWND hDlg, int nIDDlgItem);
int GetClassNameA(HWND hWnd, LPSTR lpClassName, int nMaxCount);
BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile,
    LPCSTR lpParameters, LPCSTR lpDirectory, int nShowCmd);
LRESULT SendDlgItemMessageW(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL PtInRect(const RECT *lprc, POINT pt);
BOOL InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase);
LRESULT CallWindowProcW(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
HANDLE LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad);
HCURSOR SetCursor(HCURSOR hCursor);
BOOL SetCapture(HWND hWnd);
BOOL ReleaseCapture(void);
BOOL ScreenToClient(HWND hWnd, LPPOINT lpPoint);
BOOL GetCursorPos(LPPOINT lpPoint);
UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);
BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent);
void OutputDebugStringA(LPCSTR lpOutputString);
LRESULT SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#define SendMessage SendMessageW
HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
BOOL EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint);
int FillRect(HDC hDC, const RECT *lprc, HBRUSH hbr);
BOOL DrawFocusRect(HDC hDC, const RECT *lprc);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h);
int SetBkMode(HDC hdc, int mode);
BOOL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c);
#define TextOut TextOutA
HWND GetParent(HWND hWnd);
HWND GetFocus(void);
int GetWindowTextA(HWND hWnd, LPSTR lpString, int nMaxCount);
#define GetWindowText GetWindowTextA
#define lstrlen strlen
BOOL SetSystemCursor(HCURSOR hcur, DWORD id);
BOOL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
HBRUSH GetSysColorBrush(int nIndex);
COLORREF GetSysColor(int nIndex);
#define SPI_SETCURSORS    0x0057
#define OCR_NORMAL        32512
#define COLOR_HOTLIGHT    26
#define DT_LEFT           0x00000000
#define DT_CALCRECT       0x00000400

/* GDI functions */
COLORREF SetTextColor(HDC hdc, COLORREF color);
BOOL DeleteObject(HGDIOBJ ho);
HFONT CreateFontIndirectA(const LOGFONTA *lplf);
int DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format);
#define SystemParametersInfo SystemParametersInfoA
#define CreateFontIndirect CreateFontIndirectA
#define DrawText DrawTextA

/* Static control styles */
#define SS_NOTIFY         0x0100
#define SS_OWNERDRAW      0x000D

/* Button messages */
#define BM_GETCHECK       0x00F0
#define BST_CHECKED       0x0001

/* More GDI/Window functions */
int GetObjectA(HANDLE h, int c, LPVOID pv);
#define GetObject GetObjectA
HDC GetDC(HWND hWnd);
int ReleaseDC(HWND hWnd, HDC hDC);
BOOL GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
DWORD GetTabbedTextExtentW(HDC hdc, LPCWSTR lpString, int chCount, int nTabPositions, const int *lpnTabStopPositions);
int GetDeviceCaps(HDC hdc, int index);
#define LOGPIXELSY        90

BOOL SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString);
BOOL SetWindowTextW(HWND hWnd, LPCWSTR lpString);
void PostQuitMessage(int nExitCode);
BOOL UpdateWindow(HWND hWnd);
BOOL IsDBCSLeadByte(BYTE TestChar);
HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile,
    LPCWSTR lpParameters, LPCWSTR lpDirectory, int nShowCmd);
int StartPage(HDC hdc);
int EndPage(HDC hdc);

/* File handle operations */
HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    void *lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten, void *lpOverlapped);
BOOL CloseHandle(HANDLE hObject);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
BOOL DeleteFileW(LPCWSTR lpFileName);

/* Dialog functions */
HWND CreateDialogW(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc);
#define CreateDialog CreateDialogW
INT_PTR DialogBoxW(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc);
BOOL EndDialog(HWND hDlg, INT_PTR nResult);
BOOL DestroyWindow(HWND hWnd);
BOOL MessageBeep(UINT uType);

/* Forward declarations for platform-specific functions */
void* dlopen_wide(const wchar_t* name);
DWORD GetTickCount(void);
void Sleep(DWORD dwMilliseconds);
DWORD GetLastError(void);
void SetLastError(DWORD dwErrCode);
DWORD GetCurrentThreadId(void);
DWORD GetCurrentProcessId(void);

/* INI file functions */
DWORD GetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
    LPCWSTR lpDefault, LPWSTR lpReturnedString, DWORD nSize, LPCWSTR lpFileName);
UINT GetPrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
    INT nDefault, LPCWSTR lpFileName);
BOOL WritePrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
    LPCWSTR lpString, LPCWSTR lpFileName);

/* File functions */
DWORD GetFileAttributesW(LPCWSTR lpFileName);
UINT GetACP(void);

/* Codepage conversion */
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
    int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr,
    int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte,
    LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);

#ifdef __cplusplus
}
#endif
