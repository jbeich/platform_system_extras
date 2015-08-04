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
#include <unordered_set>
#include <vector>

#include "perf_regs.h"
#include "read_elf.h"

struct ThreadEntry;

class DwarfUnwindAdapter {
 private:
  struct Space {
    uint64_t start;
    uint64_t end;
    const uint8_t* data;

    Space() {
      Clear();
    }

    void Clear() {
      start = 0;
      end = 0;
      data = nullptr;
    }
  };

  // It stores necessary info of .eh_frame/.eh_frame_hdr or .debug_frame for unwinding.
  struct DebugFrame {
    bool is_eh_frame;
    struct {
      uint64_t eh_frame_hdr_vaddr;
      uint64_t eh_frame_vaddr;
      uint64_t fde_table_offset_in_eh_frame_hdr;
      std::vector<uint8_t> eh_frame_hdr_data;
      std::vector<uint8_t> eh_frame_data;
      std::vector<ElfFileProgramHeader> program_headers;
    } eh_frame;
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
  const DebugFrame* GetDebugFrameInFile(const std::string& filename);

  const ThreadEntry* thread_;
  const RegSet* regs_;
  Space eh_frame_hdr_space_;
  Space eh_frame_space_;
  Space stack_;
  std::unordered_map<std::string, std::unique_ptr<DebugFrame> > debug_frames_;
  std::unordered_set<std::string> debug_frame_missing_files_;
};

#endif  // defined(NO_LIBUNWIND)

#endif  // SIMPLE_PERF_DWARF_UNWIND_H_
