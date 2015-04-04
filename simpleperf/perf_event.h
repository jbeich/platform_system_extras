/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
