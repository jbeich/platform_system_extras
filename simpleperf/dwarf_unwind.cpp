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

extern "C" {
#define UNW_REMOTE_ONLY
#include <dwarf.h>
}

#include <base/logging.h>
#include "read_elf.h"
#include "thread_tree.h"

bool UnwindRegToPerfReg(unw_regnum_t unwind_reg, size_t* perf_reg) {
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
    unwind_to_perf_reg_map[UNW_X86_GS] = PERF_REG_X86_CS;
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

static int FindProcInfo(unw_addr_space_t addr_space, unw_word_t ip, unw_proc_info_t* proc_info,
                        int need_unwind_info, void* arg) {
  DwarfUnwindAdapter* adapter = reinterpret_cast<DwarfUnwindAdapter*>(arg);
  bool result = adapter->FindProcInfo(addr_space, ip, proc_info, need_unwind_info);
  return result ? 0 : -UNW_EINVAL;
}

static void PutUnwindInfo(unw_addr_space_t, unw_proc_info_t*, void*) {
}

static int GetDynInfoListAddr(unw_addr_space_t, unw_word_t*, void*) {
  return -UNW_ENOINFO;
}

static int AccessMem(unw_addr_space_t, unw_word_t addr, unw_word_t* value, int write, void* arg) {
  CHECK_EQ(0, write);
  DwarfUnwindAdapter* adapter = reinterpret_cast<DwarfUnwindAdapter*>(arg);
  uint64_t mem_value;
  bool result = adapter->ReadMem(addr, &mem_value);
  if (result) {
    *value = static_cast<unw_word_t>(mem_value);
  }
  return result ? 0 : -UNW_EINVAL;
}

static int AccessReg(unw_addr_space_t, unw_regnum_t unwind_reg, unw_word_t* value, int write,
                     void* arg) {
  CHECK_EQ(0, write);
  DwarfUnwindAdapter* adapter = reinterpret_cast<DwarfUnwindAdapter*>(arg);
  size_t perf_reg;
  bool result = UnwindRegToPerfReg(unwind_reg, &perf_reg);
  if (result) {
    uint64_t reg_value;
    result = adapter->ReadReg(perf_reg, &reg_value);
    if (result) {
      *value = reg_value;
    }
  }
  return result ? 0 : -UNW_EINVAL;
}

static int AccessFpReg(unw_addr_space_t, unw_regnum_t, unw_fpreg_t*, int, void*) {
  return -UNW_EINVAL;
}

static int Resume(unw_addr_space_t, unw_cursor_t*, void*) {
  return -UNW_EINVAL;
}

static int GetProcName(unw_addr_space_t, unw_word_t, char*, size_t, unw_word_t*, void*) {
  return -UNW_EINVAL;
}

static unw_accessors_t accessors = {
    .find_proc_info = FindProcInfo,
    .put_unwind_info = PutUnwindInfo,
    .get_dyn_info_list_addr = GetDynInfoListAddr,
    .access_mem = AccessMem,
    .access_reg = AccessReg,
    .access_fpreg = AccessFpReg,
    .resume = Resume,
    .get_proc_name = GetProcName,
};

DwarfUnwindAdapter* DwarfUnwindAdapter::GetInstance() {
  static DwarfUnwindAdapter adapter;
  return &adapter;
}

static bool OmitEncodedValue(uint8_t encode, const uint8_t*& p) {
  if (encode == DW_EH_PE_omit) {
    return 0;
  }
  uint8_t format = encode & 0x0f;
  switch (format) {
    case DW_EH_PE_ptr:
      p += sizeof(unw_word_t);
      break;
    case DW_EH_PE_uleb128:
    case DW_EH_PE_sleb128:
      while ((*p & 0x80) != 0) {
        ++p;
      }
      ++p;
      break;
    case DW_EH_PE_udata2:
    case DW_EH_PE_sdata2:
      p += 2;
      break;
    case DW_EH_PE_udata4:
    case DW_EH_PE_sdata4:
      p += 4;
      break;
    case DW_EH_PE_udata8:
    case DW_EH_PE_sdata8:
      p += 8;
      break;
    default:
      LOG(ERROR) << "unhandled encode: " << std::hex << static_cast<int>(encode);
      return false;
  }
  return true;
}

static bool GetFdeTableOffsetInEhFrameHdr(const std::vector<uint8_t>& data,
                                          uint64_t* table_offset_in_eh_frame_hdr) {
  const uint8_t* p = data.data();
  const uint8_t* end = p + data.size();
  CHECK_LE(p + 4, end);
  uint8_t version = *p++;
  CHECK_EQ(1, version);
  uint8_t eh_frame_ptr_encode = *p++;
  uint8_t fde_count_encode = *p++;
  uint8_t fde_table_encode = *p++;

  if (fde_table_encode != (DW_EH_PE_datarel | DW_EH_PE_sdata4)) {
    LOG(DEBUG) << "The binary search table in .eh_frame_hdr is using unsupported encode: "
               << std::hex << static_cast<int>(fde_table_encode);
    return false;
  }

  if (!OmitEncodedValue(eh_frame_ptr_encode, p) || !OmitEncodedValue(fde_count_encode, p)) {
    return false;
  }
  CHECK_LT(p, end);
  *table_offset_in_eh_frame_hdr = p - data.data();
  return true;
}

static bool FileOffsetToVaddr(const std::vector<ElfFileProgramHeader>& program_headers,
                              uint64_t file_offset, uint64_t* vaddr) {
  for (auto& header : program_headers) {
    if (file_offset >= header.file_offset && file_offset < header.file_offset + header.file_size) {
      *vaddr = file_offset - header.file_offset + header.vaddr;
      return true;
    }
  }
  return false;
}

const DwarfUnwindAdapter::DebugFrame* DwarfUnwindAdapter::GetDebugFrameInFile(
    const std::string& filename) {
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
  DebugFrame* debug_frame = nullptr;
  if (!sections[0].data.empty() && !sections[1].data.empty()) {
    uint64_t fde_table_offset;
    if (!GetFdeTableOffsetInEhFrameHdr(sections[0].data, &fde_table_offset)) {
      LOG(ERROR) << "failed to get fde table offset from .eh_frame_hdr in " << filename;
      goto ret;
    }
    std::vector<ElfFileProgramHeader> program_headers;
    if (!ReadProgramHeadersFromElfFile(filename, &program_headers)) {
      LOG(ERROR) << "failed to read program headers in " << filename;
      goto ret;
    }
    uint64_t eh_frame_hdr_vaddr;
    uint64_t eh_frame_vaddr;
    if (!FileOffsetToVaddr(program_headers, sections[0].offset, &eh_frame_hdr_vaddr) ||
        !FileOffsetToVaddr(program_headers, sections[1].offset, &eh_frame_vaddr)) {
      LOG(ERROR) << ".eh_frame_hdr/.eh_frame doesn't appear in program headers in " << filename;
      goto ret;
    }
    debug_frame = new DebugFrame;
    debug_frame->is_eh_frame = true;
    debug_frame->eh_frame.eh_frame_hdr_vaddr = eh_frame_hdr_vaddr;
    debug_frame->eh_frame.eh_frame_vaddr = eh_frame_vaddr;
    debug_frame->eh_frame.fde_table_offset_in_eh_frame_hdr = fde_table_offset;
    debug_frame->eh_frame.eh_frame_hdr_data = std::move(sections[0].data);
    debug_frame->eh_frame.eh_frame_data = std::move(sections[1].data);
    debug_frame->eh_frame.program_headers = std::move(program_headers);
  } else if (!sections[2].data.empty()) {
    debug_frame = new DebugFrame;
    debug_frame->is_eh_frame = false;
  } else {
    LOG(ERROR) << "no debug_frame/eh_frame in " << filename;
  }

ret:
  if (debug_frame == nullptr) {
    debug_frame_missing_files_.insert(filename);
    return nullptr;
  }
  debug_frames_.insert(std::make_pair(filename, std::unique_ptr<DebugFrame>(debug_frame)));
  return debug_frame;
}

bool DwarfUnwindAdapter::FindProcInfo(unw_addr_space_t addr_space, uint64_t ip,
                                      unw_proc_info_t* proc_info, int need_unwind_info) {
  MapEntry* map = FindMapByAddr(thread_->maps, ip);
  if (map == nullptr) {
    return false;
  }
  std::string filename = map->dso->GetRedirectedPath();
  const DebugFrame* debug_frame = GetDebugFrameInFile(filename);
  if (debug_frame == nullptr) {
    return false;
  }

  bool result = false;
  if (debug_frame->is_eh_frame) {
    uint64_t ip_offset = ip - map->start_addr + map->pgoff;
    uint64_t ip_vaddr;  // vaddr in the elf file.
    result = FileOffsetToVaddr(debug_frame->eh_frame.program_headers, ip_offset, &ip_vaddr);
    CHECK(result);
    // Calculate the addresses where .eh_frame_hdr and .eh_frame stay when the process was running.
    eh_frame_hdr_space_.start = (ip - ip_vaddr) + debug_frame->eh_frame.eh_frame_hdr_vaddr;
    eh_frame_hdr_space_.end =
        eh_frame_hdr_space_.start + debug_frame->eh_frame.eh_frame_hdr_data.size();
    eh_frame_hdr_space_.data = debug_frame->eh_frame.eh_frame_hdr_data.data();

    eh_frame_space_.start = (ip - ip_vaddr) + debug_frame->eh_frame.eh_frame_vaddr;
    eh_frame_space_.end = eh_frame_space_.start + debug_frame->eh_frame.eh_frame_data.size();
    eh_frame_space_.data = debug_frame->eh_frame.eh_frame_data.data();

    unw_dyn_info_t di;
    memset(&di, '\0', sizeof(di));
    di.start_ip = map->start_addr;
    di.end_ip = map->start_addr + map->len;
    di.format = UNW_INFO_FORMAT_REMOTE_TABLE;
    di.u.rti.name_ptr = 0;
    // It is required by libunwind to be eh_frame_hdr's address in memory?
    di.u.rti.segbase = eh_frame_hdr_space_.start;
    di.u.rti.table_data =
        eh_frame_hdr_space_.start + debug_frame->eh_frame.fde_table_offset_in_eh_frame_hdr;
    di.u.rti.table_len = (eh_frame_hdr_space_.end - di.u.rti.table_data) / sizeof(unw_word_t);
    result = dwarf_search_unwind_table(addr_space, ip, &di, proc_info, need_unwind_info, this) == 0;
  } else {
    eh_frame_hdr_space_.Clear();
    eh_frame_space_.Clear();
    unw_dyn_info_t di;
    // TODO: Using dwarf_find_debug_frame() have address conflicts if more than one process
    // are involved. It doesn't matter because we usually only unwind for one process. But it
    // would be better if we can remove the limit.
    unw_word_t segbase = map->start_addr - map->pgoff;
    int found = dwarf_find_debug_frame(0, &di, ip, segbase, filename.c_str(), map->start_addr,
                                       map->start_addr + map->len);
    if (found == 1) {
      result =
          dwarf_search_unwind_table(addr_space, ip, &di, proc_info, need_unwind_info, this) == 0;
    }
  }
  return result;
}

bool DwarfUnwindAdapter::ReadReg(size_t perf_reg, uint64_t* value) {
  return GetRegValue(*regs_, perf_reg, value);
}

static uint64_t AlignedRead64(const void* p, const void* end) {
  // We already know all ReadMem called from libunwind is unw_word_t aligned.
  const unw_word_t* tp = reinterpret_cast<const unw_word_t*>(p);
  uint64_t value = *tp++;
  if (sizeof(unw_word_t) == sizeof(uint32_t) && tp < end) {
    value |= static_cast<uint64_t>(*tp) << 32;
  }
  return value;
}

bool DwarfUnwindAdapter::ReadMem(uint64_t addr, uint64_t* value) {
  if (addr >= eh_frame_hdr_space_.start && addr < eh_frame_hdr_space_.end) {
    *value = AlignedRead64(
        eh_frame_hdr_space_.data + addr - eh_frame_hdr_space_.start,
        eh_frame_hdr_space_.data + eh_frame_hdr_space_.end - eh_frame_hdr_space_.start);
    return true;
  }
  if (addr >= eh_frame_space_.start && addr < eh_frame_space_.end) {
    *value = AlignedRead64(eh_frame_space_.data + addr - eh_frame_space_.start,
                           eh_frame_space_.data + eh_frame_space_.end - eh_frame_space_.start);
    return true;
  }
  if (addr >= stack_.start && addr < stack_.end) {
    *value =
        AlignedRead64(stack_.data + addr - stack_.start, stack_.data + stack_.end - stack_.start);
    return true;
  }
  return false;
}

std::vector<uint64_t> DwarfUnwindAdapter::UnwindCallChain(const ThreadEntry& thread,
                                                          const RegSet& regs,
                                                          const std::vector<char>& stack) {
  uint64_t ip;
  if (!GetIpRegValue(regs, &ip)) {
    LOG(ERROR) << "can't read IP reg value";
    return std::vector<uint64_t>();
  }
  std::vector<uint64_t> result;
  result.push_back(ip);
  if (GetCurrentArch() != GetBuildArch()) {
    LOG(ERROR) << "can't unwind data recorded on a different architecture";
    return result;
  }

  thread_ = &thread;
  regs_ = &regs;
  uint64_t sp;
  if (!GetSpRegValue(regs, &sp)) {
    LOG(ERROR) << "can't read SP reg value";
    return result;
  }
  stack_.start = sp;
  stack_.end = sp + stack.size();
  stack_.data = reinterpret_cast<const uint8_t*>(stack.data());
  LOG(DEBUG) << "user stack [0x" << std::hex << stack_.start << " - 0x" << stack_.end << "]";

  unw_addr_space_t addr_space = unw_create_addr_space(&accessors, 0);
  unw_cursor_t cursor;
  int retval = unw_init_remote(&cursor, addr_space, this);
  if (retval != 0) {
    LOG(ERROR) << "unw_init_remote() failed: " << retval;
    goto ret;
  }
  while (true) {
    retval = unw_step(&cursor);
    if (retval <= 0) {
      if (retval < 0) {
        LOG(DEBUG) << "unw_step() failed: " << retval;
      }
      break;
    }
    unw_word_t value;
    retval = unw_get_reg(&cursor, UNW_REG_IP, &value);
    if (retval < 0) {
      LOG(DEBUG) << "unw_get_reg() failed: " << retval;
      break;
    }
    // DWARF spec says undefined return address means end of stack.
    if (value == 0) {
      break;
    }
    result.push_back(value);
  }

ret:
  unw_destroy_addr_space(addr_space);
  return result;
}
