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

#ifndef SIMPLE_PERF_DWARF_UNWIND_H_
#define SIMPLE_PERF_DWARF_UNWIND_H_

#if defined(NO_LIBUNWIND)

#include <vector>
#include "perf_regs.h"

struct ThreadEntry;

class DwarfUnwindAdapter {
 public:
  static DwarfUnwindAdapter* GetInstance();

  std::vector<uint64_t> UnwindCallChain(const ThreadEntry& thread, const RegSet& regs,
                                        const std::vector<char>& stack);
};

#else

#include <sys/types.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <backtrace/Backtrace.h>

#include "perf_regs.h"
#include "read_elf.h"

struct ThreadEntry;

using DebugFrameInfo = BacktraceOfflineCallbacks::DebugFrameInfo;

class DwarfUnwindAdapter {
 private:
 public:
  static DwarfUnwindAdapter* GetInstance();

  std::vector<uint64_t> UnwindCallChain(const ThreadEntry& thread, const RegSet& regs,
                                        const std::vector<char>& stack);

 private:
  DwarfUnwindAdapter() : regs_(nullptr), stack_(nullptr) {
  }

  DebugFrameInfo* GetDebugFrameInFile(const std::string& filename);
  bool ReadReg(size_t unwind_reg, uint64_t* value);
  size_t ReadStack(uint64_t offset, uint8_t* buffer, size_t size);

  const RegSet* regs_;
  const std::vector<char>* stack_;
  std::unordered_map<std::string, std::unique_ptr<DebugFrameInfo> > debug_frames_;
  std::unordered_set<std::string> debug_frame_missing_files_;
};

#endif  // defined(NO_LIBUNWIND)

#endif  // SIMPLE_PERF_DWARF_UNWIND_H_
