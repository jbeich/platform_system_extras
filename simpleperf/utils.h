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

#ifndef SIMPLE_PERF_UTILS_H_
#define SIMPLE_PERF_UTILS_H_

#include <stddef.h>

void LogError(const char* fmt, ...);
void LogInfo(const char* fmt, ...);
void LogErrorWithErrno(const char* fmt, ...);

#define LOGE(fmt, ...) LogError(fmt, ##__VA_ARGS__)
#define SLOGE(fmt, ...) LogErrorWithErrno(fmt, ##__VA_ARGS__)

#if defined(DEBUG)
#define LOGW(fmt, ...) LogError(fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LogInfo(fmt, ##__VA_ARGS__)
#define SLOGW(fmt, ...) LogErrorWithErrno(fmt, ##__VA_ARGS__)
#else
#define LOGW(fmt, ...)
#define LOGI(fmt, ...)
#define SLOGW(fmt, ...)
#endif

void PrintIndented(size_t indent, const char* fmt, ...);

#endif  // SIMPLE_PERF_UTILS_H_
