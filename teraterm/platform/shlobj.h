/*
 * shlobj.h shim for non-Windows platforms
 *
 * Provides empty definitions - shell object functions are Windows-only.
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <shlobj.h>
#else
  /* CSIDL constants used for SHGetSpecialFolderPath */
  #ifndef CSIDL_DESKTOP
    #define CSIDL_DESKTOP           0x0000
    #define CSIDL_APPDATA           0x001a
    #define CSIDL_LOCAL_APPDATA     0x001c
    #define CSIDL_PERSONAL          0x0005
    #define CSIDL_PROFILE           0x0028
  #endif
#endif
