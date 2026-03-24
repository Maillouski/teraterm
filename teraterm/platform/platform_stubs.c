/*
 * Copyright (C) 2024 TeraTerm Project
 * All rights reserved.
 *
 * Stub/no-op implementations for Windows-only features on macOS.
 * These allow the codebase to link without implementing every
 * Windows API function that is referenced.
 */

#if defined(__APPLE__) || defined(__linux__)

#include "platform_macos_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/* --- Registry stubs (Tera Term uses INI files, registry rarely used) --- */

LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, DWORD samDesired, HKEY* phkResult) {
    (void)hKey; (void)lpSubKey; (void)ulOptions; (void)samDesired;
    if (phkResult) *phkResult = NULL;
    return 2; /* ERROR_FILE_NOT_FOUND */
}

LONG RegCloseKey(HKEY hKey) {
    (void)hKey;
    return 0;
}

LONG RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved,
                      LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    (void)hKey; (void)lpValueName; (void)lpReserved; (void)lpType; (void)lpData; (void)lpcbData;
    return 2; /* ERROR_FILE_NOT_FOUND */
}

/* --- DDE stubs (DDE not available on macOS) --- */

int DdeInitializeW(void* pidInst, void* pfnCallback, DWORD afCmd, DWORD ulRes) {
    (void)pidInst; (void)pfnCallback; (void)afCmd; (void)ulRes;
    return 1; /* DMLERR_DLL_USAGE - indicate DDE unavailable */
}

int DdeUninitialize(DWORD idInst) {
    (void)idInst;
    return 1;
}

/* --- GDI+ stubs --- */
HRESULT GdiplusStartup(void* token, void* input, void* output) {
    (void)token; (void)input; (void)output;
    return S_OK;
}

void GdiplusShutdown(void* token) {
    (void)token;
}

/* --- HTML Help stub --- */
HWND HtmlHelpW(HWND hwndCaller, LPCWSTR pszFile, UINT uCommand, DWORD_PTR dwData) {
    (void)hwndCaller; (void)pszFile; (void)uCommand; (void)dwData;
    return NULL;
}

/* --- Print stubs --- */
HDC CreateDCW(LPCWSTR pwszDriver, LPCWSTR pwszDevice, LPCWSTR pszPort, const void* pdm) {
    (void)pwszDriver; (void)pwszDevice; (void)pszPort; (void)pdm;
    return NULL;
}

BOOL DeleteDC(HDC hdc) {
    (void)hdc;
    return TRUE;
}

int StartDocW(HDC hdc, const void* lpdi) {
    (void)hdc; (void)lpdi;
    return -1; /* Error - printing not supported */
}

int EndDoc(HDC hdc) {
    (void)hdc;
    return 0;
}

int StartPage(HDC hdc) {
    (void)hdc;
    return -1;
}

int EndPage(HDC hdc) {
    (void)hdc;
    return 0;
}

/* --- Shared memory stubs (use mmap/shm on macOS) --- */
HANDLE CreateFileMappingW(HANDLE hFile, void* lpAttributes, DWORD flProtect,
                          DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName) {
    (void)hFile; (void)lpAttributes; (void)flProtect;
    (void)dwMaximumSizeHigh; (void)dwMaximumSizeLow; (void)lpName;
    /* TODO: implement with shm_open + mmap */
    return NULL;
}

LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
                     DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, size_t dwNumberOfBytesToMap) {
    (void)hFileMappingObject; (void)dwDesiredAccess;
    (void)dwFileOffsetHigh; (void)dwFileOffsetLow; (void)dwNumberOfBytesToMap;
    return NULL;
}

BOOL UnmapViewOfFile(LPCVOID lpBaseAddress) {
    (void)lpBaseAddress;
    return TRUE;
}

/* --- WinSock async stubs --- */
HANDLE WSAAsyncGetHostByName(HWND hWnd, UINT wMsg, const char* name, char* buf, int buflen) {
    (void)hWnd; (void)wMsg; (void)name; (void)buf; (void)buflen;
    return NULL;
}

int WSAStartup(WORD wVersionRequested, void* lpWSAData) {
    (void)wVersionRequested; (void)lpWSAData;
    return 0;
}

int WSACleanup(void) {
    return 0;
}

/* --- Window management stubs --- */
HWND CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
                     DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                     HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    (void)dwExStyle; (void)lpClassName; (void)lpWindowName; (void)dwStyle;
    (void)X; (void)Y; (void)nWidth; (void)nHeight;
    (void)hWndParent; (void)hMenu; (void)hInstance; (void)lpParam;
    /* TODO: Bridge to Cocoa window creation */
    return NULL;
}

BOOL DestroyWindow(HWND hWnd) {
    (void)hWnd;
    return TRUE;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow) {
    (void)hWnd; (void)nCmdShow;
    return TRUE;
}

BOOL UpdateWindow(HWND hWnd) {
    (void)hWnd;
    return TRUE;
}

LRESULT SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    (void)hWnd; (void)Msg; (void)wParam; (void)lParam;
    return 0;
}

LRESULT DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    (void)hWnd; (void)Msg; (void)wParam; (void)lParam;
    return 0;
}

/* --- Timer --- */
UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc) {
    (void)hWnd; (void)uElapse; (void)lpTimerFunc;
    /* TODO: Bridge to macOS timer */
    return nIDEvent;
}

BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent) {
    (void)hWnd; (void)uIDEvent;
    return TRUE;
}

/* --- Message box --- */
int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    (void)hWnd; (void)uType;
    /* Simple console fallback - will be replaced by Cocoa dialog */
    if (lpCaption) fwprintf(stderr, L"%ls: ", lpCaption);
    if (lpText) fwprintf(stderr, L"%ls\n", lpText);
    return 1; /* IDOK */
}

/* --- GetModuleFileName --- */
DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    (void)hModule;
    if (!lpFilename || nSize == 0) return 0;
    /* Get executable path using _NSGetExecutablePath on macOS */
    char path[1024];
    uint32_t size = sizeof(path);
    extern int _NSGetExecutablePath(char* buf, uint32_t* bufsize);
    if (_NSGetExecutablePath(path, &size) == 0) {
        mbstowcs(lpFilename, path, nSize);
        return (DWORD)wcslen(lpFilename);
    }
    lpFilename[0] = L'\0';
    return 0;
}

/* --- File operations --- */
DWORD GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer) {
    if (!lpBuffer) return 0;
    const char* tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    mbstowcs(lpBuffer, tmp, nBufferLength);
    return (DWORD)wcslen(lpBuffer);
}

BOOL CreateDirectoryW(LPCWSTR lpPathName, void* lpSecurityAttributes) {
    (void)lpSecurityAttributes;
    if (!lpPathName) return FALSE;
    char path[1024];
    wcstombs(path, lpPathName, sizeof(path));
    return mkdir(path, 0755) == 0 ? TRUE : FALSE;
}

BOOL PathFileExistsW(LPCWSTR pszPath) {
    if (!pszPath) return FALSE;
    char path[1024];
    wcstombs(path, pszPath, sizeof(path));
    struct stat st;
    return stat(path, &st) == 0 ? TRUE : FALSE;
}

/* --- String resources (stub) --- */
int LoadStringW(HINSTANCE hInstance, UINT uID, LPWSTR lpBuffer, int cchBufferMax) {
    (void)hInstance; (void)uID;
    if (lpBuffer && cchBufferMax > 0) lpBuffer[0] = L'\0';
    return 0;
}

/* --- INI file functions (basic implementation) --- */
DWORD GetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
    LPCWSTR lpDefault, LPWSTR lpReturnedString, DWORD nSize, LPCWSTR lpFileName) {
    (void)lpAppName; (void)lpKeyName; (void)lpFileName;
    if (lpReturnedString && nSize > 0) {
        if (lpDefault) {
            wcsncpy(lpReturnedString, lpDefault, nSize);
            lpReturnedString[nSize - 1] = L'\0';
            return (DWORD)wcslen(lpReturnedString);
        }
        lpReturnedString[0] = L'\0';
    }
    return 0;
}

UINT GetPrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
    INT nDefault, LPCWSTR lpFileName) {
    (void)lpAppName; (void)lpKeyName; (void)lpFileName;
    return (UINT)nDefault;
}

BOOL WritePrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName,
    LPCWSTR lpString, LPCWSTR lpFileName) {
    (void)lpAppName; (void)lpKeyName; (void)lpString; (void)lpFileName;
    /* TODO: implement INI file writing */
    return FALSE;
}

/* --- File attributes --- */
DWORD GetFileAttributesW(LPCWSTR lpFileName) {
    if (!lpFileName) return INVALID_FILE_ATTRIBUTES;
    char path[1024];
    wcstombs(path, lpFileName, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) return INVALID_FILE_ATTRIBUTES;
    DWORD attrs = 0;
    if (S_ISDIR(st.st_mode)) attrs |= FILE_ATTRIBUTE_DIRECTORY;
    return attrs;
}

/* --- Code page --- */
UINT GetACP(void) {
    return 65001; /* CP_UTF8 - macOS uses UTF-8 */
}

/* --- Codepage conversion (using iconv or wcstombs) --- */
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
    int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar) {
    (void)CodePage; (void)dwFlags;
    if (!lpMultiByteStr) return 0;
    if (cbMultiByte == -1) cbMultiByte = (int)strlen(lpMultiByteStr) + 1;
    if (cchWideChar == 0) {
        /* Return required buffer size */
        return (int)mbstowcs(NULL, lpMultiByteStr, 0) + 1;
    }
    size_t result = mbstowcs(lpWideCharStr, lpMultiByteStr, (size_t)cchWideChar);
    if (result == (size_t)-1) return 0;
    return (int)result + (cbMultiByte > 0 && lpMultiByteStr[cbMultiByte - 1] == '\0' ? 1 : 0);
}

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr,
    int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte,
    LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar) {
    (void)CodePage; (void)dwFlags; (void)lpDefaultChar; (void)lpUsedDefaultChar;
    if (!lpWideCharStr) return 0;
    if (cchWideChar == -1) cchWideChar = (int)wcslen(lpWideCharStr) + 1;
    if (cbMultiByte == 0) {
        /* Return required buffer size */
        return (int)wcstombs(NULL, lpWideCharStr, 0) + 1;
    }
    size_t result = wcstombs(lpMultiByteStr, lpWideCharStr, (size_t)cbMultiByte);
    if (result == (size_t)-1) return 0;
    return (int)result + (cchWideChar > 0 && lpWideCharStr[cchWideChar - 1] == L'\0' ? 1 : 0);
}

/* --- Window functions (stubs) --- */
LONG_PTR GetWindowLongPtrW(HWND hWnd, int nIndex) {
    (void)hWnd; (void)nIndex;
    return 0;
}

LONG_PTR SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    (void)hWnd; (void)nIndex; (void)dwNewLong;
    return 0;
}

HWND GetDlgItem(HWND hDlg, int nIDDlgItem) {
    (void)hDlg; (void)nIDDlgItem;
    return NULL;
}

int GetClassNameA(HWND hWnd, LPSTR lpClassName, int nMaxCount) {
    (void)hWnd;
    if (lpClassName && nMaxCount > 0) lpClassName[0] = '\0';
    return 0;
}

BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    (void)hWnd; (void)Msg; (void)wParam; (void)lParam;
    return FALSE;
}

HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile,
    LPCSTR lpParameters, LPCSTR lpDirectory, int nShowCmd) {
    (void)hwnd; (void)lpOperation; (void)lpFile;
    (void)lpParameters; (void)lpDirectory; (void)nShowCmd;
    return NULL;
}

LRESULT SendDlgItemMessageW(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam) {
    (void)hDlg; (void)nIDDlgItem; (void)Msg; (void)wParam; (void)lParam;
    return 0;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect) {
    (void)hWnd;
    if (lpRect) { lpRect->left = 0; lpRect->top = 0; lpRect->right = 0; lpRect->bottom = 0; }
    return TRUE;
}

BOOL PtInRect(const RECT *lprc, POINT pt) {
    if (!lprc) return FALSE;
    return (pt.x >= lprc->left && pt.x < lprc->right &&
            pt.y >= lprc->top && pt.y < lprc->bottom);
}

BOOL InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase) {
    (void)hWnd; (void)lpRect; (void)bErase;
    return TRUE;
}

LRESULT CallWindowProcW(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    (void)lpPrevWndFunc; (void)hWnd; (void)Msg; (void)wParam; (void)lParam;
    return 0;
}

HANDLE LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad) {
    (void)hInst; (void)name; (void)type; (void)cx; (void)cy; (void)fuLoad;
    return NULL;
}

HCURSOR SetCursor(HCURSOR hCursor) {
    (void)hCursor;
    return NULL;
}

BOOL SetCapture(HWND hWnd) {
    (void)hWnd;
    return TRUE;
}

BOOL ReleaseCapture(void) {
    return TRUE;
}

BOOL ScreenToClient(HWND hWnd, LPPOINT lpPoint) {
    (void)hWnd; (void)lpPoint;
    return TRUE;
}

BOOL GetCursorPos(LPPOINT lpPoint) {
    if (lpPoint) { lpPoint->x = 0; lpPoint->y = 0; }
    return TRUE;
}

HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint) {
    (void)hWnd;
    if (lpPaint) memset(lpPaint, 0, sizeof(*lpPaint));
    return NULL;
}

BOOL EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint) {
    (void)hWnd; (void)lpPaint;
    return TRUE;
}

int FillRect(HDC hDC, const RECT *lprc, HBRUSH hbr) {
    (void)hDC; (void)lprc; (void)hbr;
    return 1;
}

BOOL DrawFocusRect(HDC hDC, const RECT *lprc) {
    (void)hDC; (void)lprc;
    return TRUE;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h) {
    (void)hdc; (void)h;
    return NULL;
}

int SetBkMode(HDC hdc, int mode) {
    (void)hdc; (void)mode;
    return 0;
}

BOOL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c) {
    (void)hdc; (void)x; (void)y; (void)lpString; (void)c;
    return TRUE;
}

HWND GetParent(HWND hWnd) {
    (void)hWnd;
    return NULL;
}

HWND GetFocus(void) {
    return NULL;
}

int GetWindowTextA(HWND hWnd, LPSTR lpString, int nMaxCount) {
    (void)hWnd;
    if (lpString && nMaxCount > 0) lpString[0] = '\0';
    return 0;
}

BOOL SetSystemCursor(HCURSOR hcur, DWORD id) {
    (void)hcur; (void)id;
    return TRUE;
}

BOOL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) {
    (void)uiAction; (void)uiParam; (void)pvParam; (void)fWinIni;
    return TRUE;
}

HBRUSH GetSysColorBrush(int nIndex) {
    (void)nIndex;
    return NULL;
}

COLORREF GetSysColor(int nIndex) {
    (void)nIndex;
    return 0;
}

void OutputDebugStringA(LPCSTR lpOutputString) {
    if (lpOutputString) fprintf(stderr, "%s", lpOutputString);
}

COLORREF SetTextColor(HDC hdc, COLORREF color) {
    (void)hdc; (void)color;
    return 0;
}

BOOL DeleteObject(HGDIOBJ ho) {
    (void)ho;
    return TRUE;
}

HFONT CreateFontIndirectA(const LOGFONTA *lplf) {
    (void)lplf;
    return NULL;
}

int DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format) {
    (void)hdc; (void)lpchText; (void)cchText; (void)lprc; (void)format;
    return 0;
}

int GetObjectA(HANDLE h, int c, LPVOID pv) {
    (void)h; (void)c; (void)pv;
    return 0;
}

HDC GetDC(HWND hWnd) {
    (void)hWnd;
    return NULL;
}

int ReleaseDC(HWND hWnd, HDC hDC) {
    (void)hWnd; (void)hDC;
    return 0;
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect) {
    (void)hWnd;
    if (lpRect) { lpRect->left = 0; lpRect->top = 0; lpRect->right = 0; lpRect->bottom = 0; }
    return TRUE;
}

BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint) {
    (void)hWnd; (void)X; (void)Y; (void)nWidth; (void)nHeight; (void)bRepaint;
    return TRUE;
}

DWORD GetTabbedTextExtentW(HDC hdc, LPCWSTR lpString, int chCount, int nTabPositions, const int *lpnTabStopPositions) {
    (void)hdc; (void)lpString; (void)chCount; (void)nTabPositions; (void)lpnTabStopPositions;
    return 0;
}

int GetDeviceCaps(HDC hdc, int index) {
    (void)hdc; (void)index;
    return 96; /* Default DPI */
}

BOOL SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString) {
    (void)hDlg; (void)nIDDlgItem; (void)lpString;
    return TRUE;
}

BOOL SetWindowTextW(HWND hWnd, LPCWSTR lpString) {
    (void)hWnd; (void)lpString;
    return TRUE;
}

#endif /* __APPLE__ || __linux__ */
