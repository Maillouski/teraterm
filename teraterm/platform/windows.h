/*
 * windows.h shim for non-Windows platforms
 *
 * Redirects #include <windows.h> to the platform abstraction layer
 * so that existing source files compile without modification.
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <windows.h>
#else
  #include "platform_macos_types.h"
#endif
