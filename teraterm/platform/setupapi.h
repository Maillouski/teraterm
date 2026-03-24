/*
 * setupapi.h shim for non-Windows platforms
 *
 * Windows device setup API - not available on macOS/Linux.
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <setupapi.h>
#endif
