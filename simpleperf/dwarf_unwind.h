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

#include <libunwind.h>
#include <memory>
#include <unordered_map>
#include <vector>

#include "perf_regs.h"

struct ThreadEntry;

class DwarfUnwindAdapter {
 private:
  struct Space {
    uint64_t start;
    uint64_t end;
    const uint8_t* data;

    Space() : start(0), end(0), data(nullptr) {
    }
  };

  // Keep this structure compatible with table_entry in libunwind.
  struct FdeIndex {
    uint32_t start_ip_offset;
    uint32_t fde_offset;
  };

  // Represents .eh_frame or .debug_frame section in elf file.
  struct DebugFrame {
    bool is_eh_frame;
    uint64_t file_offset;
    std::vector<uint8_t> data;
    std::vector<FdeIndex> fde_table;
  };

 public:
  static DwarfUnwindAdapter* GetInstance();

  std::vector<uint64_t> UnwindCallChain(const ThreadEntry& thread, const RegSet& regs,
                                        const std::vector<char>& stack);

  bool FindProcInfo(unw_addr_space_t addr_space, uint64_t ip, unw_proc_info_t* proc_info,
                    int need_unwind_info);
  bool ReadReg(size_t perf_reg, uint64_t* value);
  bool ReadMem(uint64_t addr, uint64_t* value);

 private:
  DwarfUnwindAdapter() : thread_(nullptr), regs_(nullptr) {
  }
  bool ReadFdeTableFromEhFrameHdrSection(uint64_t file_offset,
                                  const std::vector<uint8_t>& data,
                                  std::vector<FdeIndex>* fde_table);
  bool ReadFdeTableFromDebugFrameSection(const std::vector<uint8_t>& data,
                                         std::vector<FdeIndex>* fde_table);
  bool IsIntersected(uint64_t start, uint64_t end, const Space& space);
  const DebugFrame* GetDebugFrameInFile(const std::string& filename);
  bool GetFdeOffsetByIp(const std::vector<FdeIndex>& fde_table, uint32_t ip_offset,
                        uint32_t* fde_offset);

  const ThreadEntry* thread_;
  const RegSet* regs_;
  Space stack_;
  std::unordered_map<std::string, std::unique_ptr<DebugFrame> > debug_frames_;
};

#endif  // defined(NO_LIBUNWIND)

#endif  // SIMPLE_PERF_DWARF_UNWIND_H_
