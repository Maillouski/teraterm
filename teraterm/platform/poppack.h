/*
 * poppack.h shim for non-Windows platforms
 * Restores structure packing.
 */
#if defined(_WIN32) || defined(_WIN64)
  #include_next <poppack.h>
#else
  #pragma pack(pop)
#endif
