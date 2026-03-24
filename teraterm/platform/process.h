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

  struct _beginthreadex_wrapper_args {
      _beginthreadex_proc_type func;
      void *arg;
  };

  static inline void *_beginthreadex_wrapper(void *ctx)
  {
      struct _beginthreadex_wrapper_args args = *(struct _beginthreadex_wrapper_args *)ctx;
      free(ctx);
      args.func(args.arg);
      return NULL;
  }

  static inline uintptr_t _beginthreadex(
      void *security, unsigned stack_size,
      _beginthreadex_proc_type start_address,
      void *arglist, unsigned initflag, unsigned *thrdaddr)
  {
      (void)security; (void)stack_size; (void)initflag; (void)thrdaddr;
      struct _beginthreadex_wrapper_args *ctx = malloc(sizeof(*ctx));
      if (!ctx) return 0;
      ctx->func = start_address;
      ctx->arg = arglist;
      pthread_t thread;
      int ret = pthread_create(&thread, NULL, _beginthreadex_wrapper, ctx);
      if (ret != 0) { free(ctx); return 0; }
      return (uintptr_t)thread;
  }
#endif
