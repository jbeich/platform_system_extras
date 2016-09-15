/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef SIMPLE_PERF_PERF_CLOCK_H_
#define SIMPLE_PERF_PERF_CLOCK_H_

#include <stdint.h>

// Perf clock is the clock used by the kernel to generate timestamps in perf
// event records. We use a separate init function because it allocates a mapped
// buffer for perf event file, which might not be available during profiling.
// And we'd better call the init function before profiling.
bool InitPerfClock();

bool GetPerfClock(uint64_t* time_in_ns);

#endif  // SIMPLE_PERF_PERF_CLOCK_H_
