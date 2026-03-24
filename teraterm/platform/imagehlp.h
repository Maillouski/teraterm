/*
 * imagehlp.h shim for non-Windows platforms
 *
 * Provides empty definitions - image helper functions are Windows-only.
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <imagehlp.h>
#endif
