#ifndef SIMPLE_PERF_PERF_EVENT_H_
#define SIMPLE_PERF_PERF_EVENT_H_

#include <linux/perf_event.h>

// Fix up some new macros missing in old <linux/perf_event.h>.
#if !defined(PERF_SAMPLE_IDENTIFIER)
  #define PERF_SAMPLE_IDENTIFIER  (1U << 17)
#endif

#if !defined(PERF_RECORD_MISC_MMAP_DATA)
  #define PERF_RECORD_MISC_MMAP_DATA  (1 << 13)
#endif

#endif  // SIMPLE_PERF_PERF_EVENT_H_
