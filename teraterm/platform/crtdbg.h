/*
 * crtdbg.h shim for non-Windows platforms
 *
 * Provides no-op definitions for the Windows C runtime debug macros.
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <crtdbg.h>
#else
  /* Map CRT debug macros to standard equivalents */
  #ifndef _CRTDBG_MAP_ALLOC
    #define _CRTDBG_MAP_ALLOC
  #endif
  #ifndef _TRUNCATE
    #define _TRUNCATE ((size_t)-1)
  #endif
  #ifndef _countof
    #define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
  #endif
  /* Secure CRT function mappings */
  #include <stdio.h>
  #include <wchar.h>
  #ifndef _vsnprintf_s
    #define _vsnprintf_s(buf, sz, trunc, fmt, ap) vsnprintf(buf, sz, fmt, ap)
  #endif
  #ifndef _vsnwprintf_s
    #define _vsnwprintf_s(buf, sz, trunc, fmt, ap) vswprintf(buf, sz, fmt, ap)
  #endif
  #ifndef _wcsdup
    #define _wcsdup wcsdup
  #endif
  #ifndef _CrtSetDbgFlag
    #define _CrtSetDbgFlag(x)   ((void)0)
  #endif
  #ifndef _CrtSetReportMode
    #define _CrtSetReportMode(t, f) ((void)0)
  #endif
  #ifndef _CrtDumpMemoryLeaks
    #define _CrtDumpMemoryLeaks() ((void)0)
  #endif
  #ifndef _ASSERT
    #define _ASSERT(expr)       ((void)0)
  #endif
  #ifndef _ASSERTE
    #define _ASSERTE(expr)      ((void)0)
  #endif
  #ifndef _RPT0
    #define _RPT0(rpttype, msg) ((void)0)
  #endif
  /* CRT debug flags */
  #ifndef _CRTDBG_ALLOC_MEM_DF
    #define _CRTDBG_ALLOC_MEM_DF    0x01
  #endif
  #ifndef _CRTDBG_LEAK_CHECK_DF
    #define _CRTDBG_LEAK_CHECK_DF   0x20
  #endif
#endif
