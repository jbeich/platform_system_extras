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

#include <stdint.h>

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef unsigned long long __u64;

typedef int8_t __s8;
typedef int16_t __s16;
typedef int32_t __s32;
typedef long long __s64;

#include "kernel-headers/perf_event.h"

#endif  // SIMPLE_PERF_PERF_EVENT_H_
