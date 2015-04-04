#ifndef SIMPLE_PERF_TRACE_H_
#define SIMPLE_PERF_TRACE_H_

#if defined(DEBUG)
  #include <stdio.h>

  #define TRACE(...) printf(__VA_ARGS__)
#else
  #define TRACE(...)
#endif

#endif  // SIMPLE_PERF_TRACE_H_
