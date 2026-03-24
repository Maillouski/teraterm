/*
 * pshpack1.h shim for non-Windows platforms
 * Sets structure packing to 1 byte.
 */
#if defined(_WIN32) || defined(_WIN64)
  #include_next <pshpack1.h>
#else
  #pragma pack(push, 1)
#endif
