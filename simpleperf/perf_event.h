#ifndef SIMPLE_PERF_PERF_EVENT_H_
#define SIMPLE_PERF_PERF_EVENT_H_

#if defined(USE_BIONIC_PERF_EVENT_H)

#include <libc/kernel/uapi/linux/perf_event.h>

#else

#include <linux/perf_event.h>

#endif

#endif  // SIMPLE_PERF_PERF_EVENT_H_
