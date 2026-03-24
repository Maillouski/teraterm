/*
 * intrin.h shim for non-Windows platforms
 *
 * MSVC intrinsics header - not needed on Clang/GCC.
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <intrin.h>
#endif
