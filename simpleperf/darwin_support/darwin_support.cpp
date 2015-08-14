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

// Add fake functions to build successfully on darwin.
#include <base/logging.h>
#include "dwarf_unwind.h"
#include "environment.h"
#include "perf_regs.h"

DwarfUnwindAdapter* DwarfUnwindAdapter::GetInstance() {
  static DwarfUnwindAdapter adapter;
  return &adapter;
}

std::vector<uint64_t> DwarfUnwindAdapter::UnwindCallChain(const ThreadEntry&, const RegSet& regs,
                                                          const std::vector<char>&) {
  uint64_t ip;
  if (!GetIpRegValue(regs, &ip)) {
    LOG(ERROR) << "can't read IP reg value";
    return std::vector<uint64_t>();
  }
  return std::vector<uint64_t>(1, ip);
}

bool ProcessKernelSymbols(const std::string&, std::function<bool(const KernelSymbol&)>) {
  return false;
}

bool GetKernelBuildId(BuildId*) {
  return false;
}
