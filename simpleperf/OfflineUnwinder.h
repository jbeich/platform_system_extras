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

#ifndef SIMPLE_PERF_OFFLINE_UNWINDER_H_
#define SIMPLE_PERF_OFFLINE_UNWINDER_H_

#include <vector>

#include "perf_regs.h"

namespace simpleperf {
struct ThreadEntry;

struct UnwindingStat {
  // time used for unwinding, in ns.
  uint64_t used_time;
  enum {
    UNKNOWN_REASON,
    UNW_STEP_STOPPED,
    MAX_FRAMES_LIMIT,
    ACCESS_REG_FAILED,
    ACCESS_STACK_FAILED,
    ACCESS_MEM_FAILED,
    FIND_PROC_INFO_FAILED,
    EXECUTE_DWARF_INSTRUCTION_FAILED,
    DIFFERENT_ARCH,
  } stop_reason;
  union {
    // For ACCESS_REG_FAILED
    uint64_t regno;
    // For ACCESS_MEM_FAILED
    uint64_t addr;
    // For EXECUTE_DWARF_INSTRUCTION_FAILED
    uint64_t execute_result;
  } stop_info;
};

class OfflineUnwinder {
 public:
  OfflineUnwinder(bool strict_arch_check, bool collect_stat)
      : strict_arch_check_(strict_arch_check), collect_stat_(collect_stat) {
  }

  bool UnwindCallChain(int abi, const ThreadEntry& thread, const RegSet& regs,
                       const char* stack, size_t stack_size,
                       std::vector<uint64_t>* ips, std::vector<uint64_t>* sps);

  bool HasStat() const {
    return collect_stat_;
  }

  const UnwindingStat& Stat() const {
    return stat_;
  }

 private:
  bool strict_arch_check_;
  bool collect_stat_;
  UnwindingStat stat_;
};

} // namespace simpleperf

#endif  // SIMPLE_PERF_OFFLINE_UNWINDER_H_
