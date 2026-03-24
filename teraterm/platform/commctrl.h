/*
 * commctrl.h shim for non-Windows platforms
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <commctrl.h>
#else
  #include "platform_macos_types.h"

  /* Common control styles */
  #define PBS_SMOOTH        0x01
  #define PBS_MARQUEE       0x08
  #define PBM_SETPOS        (WM_USER + 2)
  #define PBM_SETRANGE32    (WM_USER + 6)
  #define PBM_SETMARQUEE    (WM_USER + 10)

  /* Tab control */
  #define TCN_SELCHANGE     (-551)
  #define TCM_GETCURSEL     0x130B
  #define TCM_SETCURSEL     0x130C

  /* List view */
  #define LVS_REPORT        0x0001
  #define LVM_INSERTCOLUMNW 0x1061

  /* Tooltip */
  #define TTS_ALWAYSTIP     0x01
  #define TTS_BALLOON       0x40

  /* InitCommonControlsEx */
  typedef struct tagINITCOMMONCONTROLSEX {
      DWORD dwSize;
      DWORD dwICC;
  } INITCOMMONCONTROLSEX, *LPINITCOMMONCONTROLSEX;

  #define ICC_WIN95_CLASSES  0x000000FF
#endif
