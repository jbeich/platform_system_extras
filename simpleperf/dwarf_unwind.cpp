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
#include "sample_tree.h"

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

static uint64_t ReadSULEB128(const uint8_t** pp, const uint8_t* end, bool is_signed) {
  uint64_t result = 0;
  const uint8_t* p = *pp;
  size_t shift = 0;
  while (p < end) {
    uint8_t c = *p++;
    result |= static_cast<uint64_t>(c & 0x7f) << shift;
    shift += 7;
    if ((c & 0x80) == 0) {
      if (is_signed) {
        result |= ULLONG_MAX << shift;
      }
      break;
    }
  }
  *pp = p;
  return result;
}

static uint64_t ReadSUData(const uint8_t** pp, const uint8_t* end, size_t size, bool is_signed) {
  CHECK_LE(*pp + size, end);
  uint64_t result = 0;
  if (!is_signed) {
    if (size == 2) {
      result = *reinterpret_cast<const uint16_t*>(*pp);
    } else if (size == 4) {
      result = *reinterpret_cast<const uint32_t*>(*pp);
    } else if (size == 8) {
      result = *reinterpret_cast<const uint64_t*>(*pp);
    } else {
      LOG(ERROR) << "unexpected size " << size;
    }
  } else {
    int64_t value = 0;
    if (size == 2) {
      value = *reinterpret_cast<const int16_t*>(*pp);
    } else if (size == 4) {
      value = *reinterpret_cast<const int32_t*>(*pp);
    } else if (size == 8) {
      value = *reinterpret_cast<const int64_t*>(*pp);
    } else {
      LOG(ERROR) << "unexpected size " << size;
    }
    result = value;
  }
  *pp += size;
  return result;
}

static uint64_t ReadEncodedValue(uint8_t encode, const uint8_t** pp, const uint8_t* end,
                                 uint64_t file_offset, uint64_t frame_hdr_file_offset) {
  if (encode == DW_EH_PE_omit) {
    return 0;
  }
  uint8_t format = encode & 0x0f;
  uint8_t application = encode & 0xf0;
  uint64_t value = 0;
  switch (format) {
    case DW_EH_PE_ptr:
      value = ReadSUData(pp, end, sizeof(unw_word_t), false);
      break;
    case DW_EH_PE_uleb128:
      value = ReadSULEB128(pp, end, false);
      break;
    case DW_EH_PE_udata2:
      value = ReadSUData(pp, end, 2, false);
      break;
    case DW_EH_PE_udata4:
      value = ReadSUData(pp, end, 4, false);
      break;
    case DW_EH_PE_udata8:
      value = ReadSUData(pp, end, 8, false);
      break;
    case DW_EH_PE_sleb128:
      value = ReadSULEB128(pp, end, true);
      break;
    case DW_EH_PE_sdata2:
      value = ReadSUData(pp, end, 2, true);
      break;
    case DW_EH_PE_sdata4:
      value = ReadSUData(pp, end, 4, true);
      break;
    case DW_EH_PE_sdata8:
      value = ReadSUData(pp, end, 8, true);
      break;
    default:
      LOG(ERROR) << "unhandled encode: " << std::hex << static_cast<uint32_t>(encode);
      return 0;
  }
  switch (application) {
    case DW_EH_PE_absptr:
      break;
    case DW_EH_PE_pcrel:
      value += file_offset;
      break;
    case DW_EH_PE_datarel:
      value += frame_hdr_file_offset;
      break;
    default:
      LOG(ERROR) << "unhandled encode: " << std::hex << static_cast<uint32_t>(encode);
      return 0;
  }
  return value;
}

bool DwarfUnwindAdapter::ReadFdeTableFromEhFrameHdrSection(uint64_t file_offset,
                                                    const std::vector<uint8_t>& data,
                                                    std::vector<FdeIndex>* fde_table) {
  const uint8_t* start = data.data();
  const uint8_t* end = start + data.size();
  const uint8_t* p = start;
  CHECK_LE(p + 4, end);
  uint8_t version = *p++;
  CHECK_EQ(1, version);
  uint8_t eh_frame_ptr_encode = *p++;
  uint8_t fde_count_encode = *p++;
  uint8_t table_encode = *p++;

  uint64_t eh_frame_ptr =
      ReadEncodedValue(eh_frame_ptr_encode, &p, end, file_offset + p - start, file_offset);
  CHECK_NE(0u, eh_frame_ptr);
  uint64_t fde_count =
      ReadEncodedValue(fde_count_encode, &p, end, file_offset + p - start, file_offset);
  fde_table->resize(fde_count);
  for (auto& fde : *fde_table) {
    fde.start_ip_offset =
        ReadEncodedValue(table_encode, &p, end, file_offset + p - start, file_offset);
    fde.fde_offset = ReadEncodedValue(table_encode, &p, end, file_offset + p - start, file_offset);
  }
  return (fde_count == 0) ? false : true;
}

bool DwarfUnwindAdapter::ReadFdeTableFromDebugFrameSection(
    const std::vector<uint8_t>& data, std::vector<FdeIndex>* fde_table) {
  unw_debug_frame_list fdesc;
  fdesc.start = 0;
  fdesc.end = 0;
  fdesc.debug_frame = reinterpret_cast<char*>(const_cast<uint8_t*>(data.data()));
  fdesc.debug_frame_size = data.size();
  fdesc.index = NULL;
  fdesc.index_size = 0;
  fdesc.next = NULL;
  int retval = dwarf_read_fde_table_from_debug_frame(&fdesc);
  if (retval == 0) {
    return false;
  }
  fde_table->resize(fdesc.index_size);
  FdeIndex* table = reinterpret_cast<FdeIndex*>(fdesc.index);
  for (size_t i = 0; i < fdesc.index_size; ++i) {
    (*fde_table)[i] = table[i];
  }
  free(fdesc.index);
  return true;
}

const DwarfUnwindAdapter::DebugFrame* DwarfUnwindAdapter::GetDebugFrameInFile(
    const std::string& filename) {
  auto it = debug_frames_.find(filename);
  if (it != debug_frames_.end()) {
    return it->second.get();
  }
  std::vector<ElfFileSection> sections(3);
  sections[0].name = ".eh_frame_hdr";
  sections[1].name = ".eh_frame";
  sections[2].name = ".debug_frame";
  if (!ReadSectionsFromElfFileByName(filename, &sections)) {
    return nullptr;
  }
  DebugFrame* debug_frame = nullptr;
  if (!sections[0].data.empty() && !sections[1].data.empty()) {
    std::vector<FdeIndex> fde_table;
    if (!ReadFdeTableFromEhFrameHdrSection(sections[0].offset, sections[0].data, &fde_table)) {
      LOG(ERROR) << "failed to read fde table from .eh_frame_hdr in " << filename;
      return nullptr;
    }
    debug_frame = new DebugFrame;
    debug_frame->is_eh_frame = true;
    debug_frame->file_offset = sections[1].offset;
    debug_frame->data = std::move(sections[1].data);
    debug_frame->fde_table = std::move(fde_table);
  } else if (!sections[2].data.empty()) {
    std::vector<FdeIndex> fde_table;
    if (!ReadFdeTableFromDebugFrameSection(sections[2].data, &fde_table)) {
      LOG(ERROR) << "failed to read fde_table from .debug_frame in " << filename;
      return nullptr;
    }
    debug_frame = new DebugFrame;
    debug_frame->is_eh_frame = false;
    debug_frame->file_offset = sections[2].offset;
    debug_frame->data = std::move(sections[2].data);
    debug_frame->fde_table = std::move(fde_table);
  } else {
    LOG(ERROR) << "no debug_frame/eh_frame in " << filename;
    return nullptr;
  }
  debug_frames_.insert(std::make_pair(filename, std::unique_ptr<DebugFrame>(debug_frame)));
  return debug_frame;
}

bool DwarfUnwindAdapter::GetFdeOffsetByIp(const std::vector<FdeIndex>& fde_table, uint32_t ip_offset,
                                          uint32_t* fde_offset) {
  size_t low, high;
  for (low = 0, high = fde_table.size(); low < high;) {
    size_t mid = (low + high) / 2;
    if (fde_table[mid].start_ip_offset > ip_offset) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }
  if (high == 0) {
    return false;
  }
  *fde_offset = fde_table[high - 1].fde_offset;
  return true;
}

bool DwarfUnwindAdapter::IsIntersected(uint64_t start, uint64_t end, const Space& space) {
  return (start >= space.start && start < space.end) || (end >= space.start && end < space.end);
}

bool DwarfUnwindAdapter::FindProcInfo(unw_addr_space_t addr_space, uint64_t ip,
                                      unw_proc_info_t* proc_info, int need_unwind_info) {
  MapEntry* map = FindMapByAddr(thread_->maps, ip);
  if (map == nullptr) {
    return false;
  }
  if (IsIntersected(map->start_addr, map->start_addr + map->len, stack_)) {
    LOG(ERROR) << "map intersects with stack";
    return false;
  }
  std::string filename = map->dso->GetFullPath();
  const DebugFrame* debug_frame = GetDebugFrameInFile(filename);
  if (debug_frame == nullptr) {
    return false;
  }
  // TODO: load_bias.
  uint32_t ip_offset = ip - map->start_addr + map->pgoff;
  uint32_t fde_offset;
  if (!GetFdeOffsetByIp(debug_frame->fde_table, ip_offset, &fde_offset)) {
    LOG(DEBUG) << "GetFdeOffsetByIp() failed for file " << filename << ", ip_offset "
        << std::hex << ip_offset;
    return false;
  }
  uint64_t segbase = map->start_addr - map->pgoff;
  int result;
  if (debug_frame->is_eh_frame) {
    unw_word_t fde_addr = segbase + fde_offset;
    // .eh_frame is loaded into memory at runtime, so it can be accessed through ReadMem().
    result = dwarf_extract_proc_info_from_fde(addr_space, unw_get_accessors(addr_space),
                                              &fde_addr, proc_info, need_unwind_info,
                                              0, this);
  } else {
    unw_word_t debug_frame_base = reinterpret_cast<unw_word_t>(debug_frame->data.data());
    unw_word_t fde_addr = fde_offset + debug_frame_base;
    // unw_local_addr_space is used for .debug_frame. Because .debug_frame is not loaded into
    // memory at runtime. It can't be accessed through ReadMem().
    result = dwarf_extract_proc_info_from_fde(unw_local_addr_space,
                                              unw_get_accessors(unw_local_addr_space),
                                              &fde_addr, proc_info, need_unwind_info,
                                              debug_frame_base, this);
    if (result == 0) {
      // .debug_frame uses an absolute encoding that does not know about any shared library relocation.
      proc_info->start_ip += segbase;
      proc_info->end_ip += segbase;
      proc_info->flags = UNW_PI_FLAG_DEBUG_FRAME;
    }
  }
  if (result == 0) {
    LOG(DEBUG) << "FindProcInfo for file " << filename << ", ip " << std::hex << ip << ", ["
        << proc_info->start_ip << " - " << proc_info->end_ip << "]";
  } else {
    LOG(DEBUG) << "FindProcInfo for file " << filename << ", ip " << std::hex << ip << " failed"
        << ", fde_offset = " << fde_offset;
  }
  return result == 0;
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
  if (addr >= stack_.start && addr < stack_.end) {
    *value =
        AlignedRead64(stack_.data + addr - stack_.start, stack_.data + stack_.end - stack_.start);
    return true;
  }
  MapEntry* map = FindMapByAddr(thread_->maps, addr);
  if (map != nullptr) {
    std::string filename = map->dso->GetFullPath();
    auto it = debug_frames_.find(filename);
    if (it != debug_frames_.end()) {
      DebugFrame* debug_frame = it->second.get();
      // TODO: consider load_bias.
      uint64_t file_offset = addr - map->start_addr + map->pgoff;
      if (file_offset >= debug_frame->file_offset &&
          file_offset < debug_frame->file_offset + debug_frame->data.size()) {
        *value = AlignedRead64(debug_frame->data.data() + file_offset - debug_frame->file_offset,
                               debug_frame->data.data() + debug_frame->data.size());
        return true;
      }
    }
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
    result.push_back(value);
  }

ret:
  unw_destroy_addr_space(addr_space);
  return result;
}
