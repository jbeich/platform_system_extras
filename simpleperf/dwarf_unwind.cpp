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

#include "dwarf_unwind.h"

#include <libunwind.h>

#include <functional>

#include <base/logging.h>
#include "read_elf.h"
#include "thread_tree.h"

static bool UnwindRegToPerfReg(size_t unwind_reg, size_t* perf_reg) {
  static int unwind_to_perf_reg_map[64];
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    memset(unwind_to_perf_reg_map, -1, sizeof(unwind_to_perf_reg_map));
#if defined(__i386__)
    unwind_to_perf_reg_map[UNW_X86_EAX] = PERF_REG_X86_AX;
    unwind_to_perf_reg_map[UNW_X86_EDX] = PERF_REG_X86_DX;
    unwind_to_perf_reg_map[UNW_X86_ECX] = PERF_REG_X86_CX;
    unwind_to_perf_reg_map[UNW_X86_EBX] = PERF_REG_X86_BX;
    unwind_to_perf_reg_map[UNW_X86_ESI] = PERF_REG_X86_SI;
    unwind_to_perf_reg_map[UNW_X86_EDI] = PERF_REG_X86_DI;
    unwind_to_perf_reg_map[UNW_X86_EBP] = PERF_REG_X86_BP;
    unwind_to_perf_reg_map[UNW_X86_ESP] = PERF_REG_X86_SP;
    unwind_to_perf_reg_map[UNW_X86_EIP] = PERF_REG_X86_IP;
    unwind_to_perf_reg_map[UNW_X86_EFLAGS] = PERF_REG_X86_FLAGS;
    unwind_to_perf_reg_map[UNW_X86_GS] = PERF_REG_X86_GS;
    unwind_to_perf_reg_map[UNW_X86_FS] = PERF_REG_X86_FS;
    unwind_to_perf_reg_map[UNW_X86_ES] = PERF_REG_X86_ES;
    unwind_to_perf_reg_map[UNW_X86_DS] = PERF_REG_X86_DS;
    unwind_to_perf_reg_map[UNW_X86_SS] = PERF_REG_X86_SS;
    unwind_to_perf_reg_map[UNW_X86_CS] = PERF_REG_X86_CS;
#elif defined(__x86_64__)
    unwind_to_perf_reg_map[UNW_X86_64_RAX] = PERF_REG_X86_AX;
    unwind_to_perf_reg_map[UNW_X86_64_RDX] = PERF_REG_X86_DX;
    unwind_to_perf_reg_map[UNW_X86_64_RCX] = PERF_REG_X86_CX;
    unwind_to_perf_reg_map[UNW_X86_64_RBX] = PERF_REG_X86_BX;
    unwind_to_perf_reg_map[UNW_X86_64_RSI] = PERF_REG_X86_SI;
    unwind_to_perf_reg_map[UNW_X86_64_RDI] = PERF_REG_X86_DI;
    unwind_to_perf_reg_map[UNW_X86_64_RBP] = PERF_REG_X86_BP;
    unwind_to_perf_reg_map[UNW_X86_64_RSP] = PERF_REG_X86_SP;
    unwind_to_perf_reg_map[UNW_X86_64_R8] = PERF_REG_X86_R8;
    unwind_to_perf_reg_map[UNW_X86_64_R9] = PERF_REG_X86_R9;
    unwind_to_perf_reg_map[UNW_X86_64_R10] = PERF_REG_X86_R10;
    unwind_to_perf_reg_map[UNW_X86_64_R11] = PERF_REG_X86_R11;
    unwind_to_perf_reg_map[UNW_X86_64_R12] = PERF_REG_X86_R12;
    unwind_to_perf_reg_map[UNW_X86_64_R13] = PERF_REG_X86_R13;
    unwind_to_perf_reg_map[UNW_X86_64_R14] = PERF_REG_X86_R14;
    unwind_to_perf_reg_map[UNW_X86_64_R15] = PERF_REG_X86_R15;
    unwind_to_perf_reg_map[UNW_X86_64_RIP] = PERF_REG_X86_IP;
#elif defined(__aarch64__)
    unwind_to_perf_reg_map[UNW_AARCH64_X0] = PERF_REG_ARM64_X0;
    unwind_to_perf_reg_map[UNW_AARCH64_X1] = PERF_REG_ARM64_X1;
    unwind_to_perf_reg_map[UNW_AARCH64_X2] = PERF_REG_ARM64_X2;
    unwind_to_perf_reg_map[UNW_AARCH64_X3] = PERF_REG_ARM64_X3;
    unwind_to_perf_reg_map[UNW_AARCH64_X4] = PERF_REG_ARM64_X4;
    unwind_to_perf_reg_map[UNW_AARCH64_X5] = PERF_REG_ARM64_X5;
    unwind_to_perf_reg_map[UNW_AARCH64_X6] = PERF_REG_ARM64_X6;
    unwind_to_perf_reg_map[UNW_AARCH64_X7] = PERF_REG_ARM64_X7;
    unwind_to_perf_reg_map[UNW_AARCH64_X8] = PERF_REG_ARM64_X8;
    unwind_to_perf_reg_map[UNW_AARCH64_X9] = PERF_REG_ARM64_X9;
    unwind_to_perf_reg_map[UNW_AARCH64_X10] = PERF_REG_ARM64_X10;
    unwind_to_perf_reg_map[UNW_AARCH64_X11] = PERF_REG_ARM64_X11;
    unwind_to_perf_reg_map[UNW_AARCH64_X12] = PERF_REG_ARM64_X12;
    unwind_to_perf_reg_map[UNW_AARCH64_X13] = PERF_REG_ARM64_X13;
    unwind_to_perf_reg_map[UNW_AARCH64_X14] = PERF_REG_ARM64_X14;
    unwind_to_perf_reg_map[UNW_AARCH64_X15] = PERF_REG_ARM64_X15;
    unwind_to_perf_reg_map[UNW_AARCH64_X16] = PERF_REG_ARM64_X16;
    unwind_to_perf_reg_map[UNW_AARCH64_X17] = PERF_REG_ARM64_X17;
    unwind_to_perf_reg_map[UNW_AARCH64_X18] = PERF_REG_ARM64_X18;
    unwind_to_perf_reg_map[UNW_AARCH64_X19] = PERF_REG_ARM64_X19;
    unwind_to_perf_reg_map[UNW_AARCH64_X20] = PERF_REG_ARM64_X20;
    unwind_to_perf_reg_map[UNW_AARCH64_X21] = PERF_REG_ARM64_X21;
    unwind_to_perf_reg_map[UNW_AARCH64_X22] = PERF_REG_ARM64_X22;
    unwind_to_perf_reg_map[UNW_AARCH64_X23] = PERF_REG_ARM64_X23;
    unwind_to_perf_reg_map[UNW_AARCH64_X24] = PERF_REG_ARM64_X24;
    unwind_to_perf_reg_map[UNW_AARCH64_X25] = PERF_REG_ARM64_X25;
    unwind_to_perf_reg_map[UNW_AARCH64_X26] = PERF_REG_ARM64_X26;
    unwind_to_perf_reg_map[UNW_AARCH64_X27] = PERF_REG_ARM64_X27;
    unwind_to_perf_reg_map[UNW_AARCH64_X28] = PERF_REG_ARM64_X28;
    unwind_to_perf_reg_map[UNW_AARCH64_X29] = PERF_REG_ARM64_X29;
    unwind_to_perf_reg_map[UNW_AARCH64_X30] = PERF_REG_ARM64_LR;
    unwind_to_perf_reg_map[UNW_AARCH64_SP] = PERF_REG_ARM64_SP;
    unwind_to_perf_reg_map[UNW_AARCH64_PC] = PERF_REG_ARM64_PC;
#elif defined(__arm__)
    unwind_to_perf_reg_map[UNW_ARM_R0] = PERF_REG_ARM_R0;
    unwind_to_perf_reg_map[UNW_ARM_R1] = PERF_REG_ARM_R1;
    unwind_to_perf_reg_map[UNW_ARM_R2] = PERF_REG_ARM_R2;
    unwind_to_perf_reg_map[UNW_ARM_R3] = PERF_REG_ARM_R3;
    unwind_to_perf_reg_map[UNW_ARM_R4] = PERF_REG_ARM_R4;
    unwind_to_perf_reg_map[UNW_ARM_R5] = PERF_REG_ARM_R5;
    unwind_to_perf_reg_map[UNW_ARM_R6] = PERF_REG_ARM_R6;
    unwind_to_perf_reg_map[UNW_ARM_R7] = PERF_REG_ARM_R7;
    unwind_to_perf_reg_map[UNW_ARM_R8] = PERF_REG_ARM_R8;
    unwind_to_perf_reg_map[UNW_ARM_R9] = PERF_REG_ARM_R9;
    unwind_to_perf_reg_map[UNW_ARM_R10] = PERF_REG_ARM_R10;
    unwind_to_perf_reg_map[UNW_ARM_R11] = PERF_REG_ARM_FP;
    unwind_to_perf_reg_map[UNW_ARM_R12] = PERF_REG_ARM_IP;  // Intra prodecure call register.
    unwind_to_perf_reg_map[UNW_ARM_R13] = PERF_REG_ARM_SP;
    unwind_to_perf_reg_map[UNW_ARM_R14] = PERF_REG_ARM_LR;
    unwind_to_perf_reg_map[UNW_ARM_R15] = PERF_REG_ARM_PC;
#endif
  }
  if (static_cast<size_t>(unwind_reg) < sizeof(unwind_to_perf_reg_map)) {
    int result = unwind_to_perf_reg_map[unwind_reg];
    if (result != -1) {
      *perf_reg = result;
      return true;
    }
  }
  LOG(ERROR) << "unmatched perf_reg for unwind_reg " << unwind_reg;
  return false;
}

DwarfUnwindAdapter* DwarfUnwindAdapter::GetInstance() {
  static DwarfUnwindAdapter adapter;
  return &adapter;
}

DebugFrameInfo* DwarfUnwindAdapter::GetDebugFrameInFile(const std::string& filename) {
  auto it = debug_frames_.find(filename);
  if (it != debug_frames_.end()) {
    return it->second.get();
  }
  if (debug_frame_missing_files_.find(filename) != debug_frame_missing_files_.end()) {
    return nullptr;
  }
  std::vector<ElfFileSection> sections(3);
  sections[0].name = ".eh_frame_hdr";
  sections[1].name = ".eh_frame";
  sections[2].name = ".debug_frame";
  if (!ReadSectionsFromElfFile(filename, &sections)) {
    return nullptr;
  }
  DebugFrameInfo* debug_frame = nullptr;
  if (!sections[0].data.empty() && !sections[1].data.empty()) {
    std::vector<ElfFileProgramHeader> program_headers;
    if (!ReadProgramHeadersFromElfFile(filename, &program_headers)) {
      LOG(ERROR) << "failed to read program headers in " << filename;
      goto ret;
    }

    debug_frame = new DebugFrameInfo;
    debug_frame->is_eh_frame = true;
    debug_frame->eh_frame.eh_frame_hdr_vaddr = sections[0].vaddr;
    debug_frame->eh_frame.eh_frame_vaddr = sections[1].vaddr;

    // libbacktrace will parse and store fde_table_offset_in_eh_frame_hdr.
    debug_frame->eh_frame.fde_table_offset_in_eh_frame_hdr = 0;
    debug_frame->eh_frame.eh_frame_hdr_data = std::move(sections[0].data);
    debug_frame->eh_frame.eh_frame_data = std::move(sections[1].data);
    for (auto& header : program_headers) {
      BacktraceOfflineCallbacks::DebugFrameInfo::EhFrame::ProgramHeader h;
      h.vaddr = header.vaddr;
      h.file_offset = header.file_offset;
      h.file_size = header.file_size;
      debug_frame->eh_frame.program_headers.push_back(h);
    }
  } else if (!sections[2].data.empty()) {
    debug_frame = new DebugFrameInfo;
    debug_frame->is_eh_frame = false;
  } else {
    LOG(ERROR) << "no debug_frame/eh_frame in " << filename;
  }

ret:
  if (debug_frame == nullptr) {
    debug_frame_missing_files_.insert(filename);
    return nullptr;
  }
  debug_frames_.emplace(filename, std::unique_ptr<DebugFrameInfo>(debug_frame));
  return debug_frame;
}

bool DwarfUnwindAdapter::ReadReg(size_t unwind_reg, uint64_t* value) {
  size_t perf_reg;
  bool result = UnwindRegToPerfReg(unwind_reg, &perf_reg);
  if (result) {
    result = GetRegValue(*regs_, perf_reg, value);
  }
  return result;
}

size_t DwarfUnwindAdapter::ReadStack(uint64_t offset, uint8_t* buffer, size_t size) {
  if (offset < stack_->size()) {
    size_t read_size = std::min(static_cast<size_t>(stack_->size() - offset), size);
    memcpy(buffer, stack_->data() + offset, read_size);
    return read_size;
  }
  return 0;
}

std::vector<uint64_t> DwarfUnwindAdapter::UnwindCallChain(const ThreadEntry& thread,
                                                          const RegSet& regs,
                                                          const std::vector<char>& stack) {
  std::vector<uint64_t> result;
  if (GetCurrentArch() != GetBuildArch()) {
    LOG(ERROR) << "can't unwind data recorded on a different architecture";
    return result;
  }

  regs_ = &regs;
  stack_ = &stack;
  std::vector<backtrace_map_t> bt_maps(thread.maps.size());
  size_t map_index = 0;
  for (auto& map : thread.maps) {
    backtrace_map_t& bt_map = bt_maps[map_index++];
    bt_map.start = map->start_addr;
    bt_map.end = map->start_addr + map->len;
    bt_map.offset = map->pgoff;
    bt_map.name = map->dso->GetRedirectedPath();
  }

  struct BacktraceOfflineCallbacks callbacks;
  callbacks.GetDebugFrameInfo =
      std::bind(&DwarfUnwindAdapter::GetDebugFrameInFile, this, std::placeholders::_1);
  callbacks.ReadStack = std::bind(&DwarfUnwindAdapter::ReadStack, this, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
  callbacks.ReadReg =
      std::bind(&DwarfUnwindAdapter::ReadReg, this, std::placeholders::_1, std::placeholders::_2);

  std::unique_ptr<BacktraceMap> backtrace_map(BacktraceMap::Create(thread.pid, bt_maps));
  std::unique_ptr<Backtrace> backtrace(
      Backtrace::Create(thread.pid, thread.tid, backtrace_map.get(), callbacks));
  if (backtrace->Unwind(0)) {
    for (auto it = backtrace->begin(); it != backtrace->end(); ++it) {
      result.push_back(it->pc);
    }
  }
  return result;
}
