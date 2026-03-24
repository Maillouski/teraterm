/*
 * process.h shim for non-Windows platforms
 *
 * Maps Windows process/threading functions to POSIX equivalents.
 */
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include_next <process.h>
#else
  #include <unistd.h>
  #include <pthread.h>
  #include <stdlib.h>

  #define _getpid() getpid()

  /* _beginthreadex stub - maps to pthread_create */
  typedef unsigned (__stdcall *_beginthreadex_proc_type)(void *);

  static inline uintptr_t _beginthreadex(
      void *security, unsigned stack_size,
      _beginthreadex_proc_type start_address,
      void *arglist, unsigned initflag, unsigned *thrdaddr)
  {
      (void)security; (void)stack_size; (void)initflag; (void)thrdaddr;
      pthread_t thread;
      int ret = pthread_create(&thread, NULL, (void*(*)(void*))start_address, arglist);
      if (ret != 0) return 0;
      return (uintptr_t)thread;
  }
#endif
