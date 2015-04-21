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

struct read_format {
  uint64_t nr;            // The number of events.
  uint64_t time_enabled;  // If PERF_FORMAT_TOTAL_TIME_ENABLED.
  uint64_t time_running;  // If PERF_FORMAT_TOTAL_TIME_RUNNING.
  uint64_t id;            // If PERF_FORMAT_ID.
};

#endif  // SIMPLE_PERF_PERF_EVENT_H_
