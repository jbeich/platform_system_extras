/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SIMPLE_PERF_READ_DEX_FILE_H_
#define SIMPLE_PERF_READ_DEX_FILE_H_

#include <inttypes.h>

#include <functional>
#include <string>

struct DexFileSymbol {
  uint64_t offset;  // Offset relative to the data section
  uint64_t len;
  std::string name;
};

bool ParseSymbolsFromDexFile(const std::string& filepath, uint64_t offset,
                             const std::function<void (DexFileSymbol&)>& callback);

bool GetDataSectionRangeOfDexFile(const std::string& filepath, uint64_t offset,
                                  uint64_t* data_section_start, uint64_t* data_section_end);

#endif  // SIMPLE_PERF_READ_DEX_FILE_H_
