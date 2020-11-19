/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SIMPLE_PERF_KALLSYMS_H_
#define SIMPLE_PERF_KALLSYMS_H_

#include <string>

namespace simpleperf {

struct KernelSymbol {
  uint64_t addr;
  char type;
  const char* name;
  const char* module;  // If nullptr, the symbol is not in a kernel module.
};

// Parses symbol_data as the content of /proc/kallsyms, calling the callback for
// each symbol that is found. Stops the parsing if the callback returns true.
bool ProcessKernelSymbols(std::string& symbol_data,
                          const std::function<bool(const KernelSymbol&)>& callback);

// Returns the start address of the kernel. It uses /proc/kallsyms to find this
// address. Returns 0 if unknown.
uint64_t GetKernelStartAddress();

// Loads the /proc/kallsyms file, requesting access if required. The value of
// kptr_restrict might be modified during the process. Its original value will
// be restored. This usually requires root privileges.
// In some cases, the process might have enough permission to send a request to
// init to change the value of kptr_restrict, using the system property
// security.lower_kptr_restrict. For this scenario, the use_property
// argument should be set to true.
bool LoadKernelSymbols(std::string* kallsyms, bool use_property = false);

}  // namespace simpleperf

#endif  // SIMPLE_PERF_KALLSYMS_H_
