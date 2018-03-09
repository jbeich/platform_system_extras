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

#include "JITDebugReader.h"

#include <inttypes.h>
#include <sys/uio.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "dso.h"
#include "environment.h"
#include "read_elf.h"
#include "utils.h"

namespace simpleperf {

// If the timestamps are changed, we need to read the linked lists. But the timestamps can be
// changed while we reading the linked lists. So we reread the timestamps after reading the
// linked lists, and reread the linked lists if need. To avoid an endless loop, using
// MAX_LINKED_LIST_READING_COUNT as the uplimit of reading linked lists in one ReadUpdate() call.
static constexpr size_t MAX_LINKED_LIST_READING_COUNT = 3u;

// To avoid too long time reading the jit/dex linked list, set an uplimit of entries read from the
// linked list.
static constexpr size_t MAX_LINKLED_LIST_LENGTH = 1024u;

// If the size of a symfile is larger than EXPECTED_MAX_SYMFILE_SIZE, we don't want to read it
// remotely.
static constexpr size_t MAX_JIT_SYMFILE_SIZE = 1024 * 1024u;

// Match the format of JITDescriptor in art/runtime/jit/debugger_itnerface.cc.
template <typename ADDRT>
struct JITDescriptor {
  uint32_t version;
  uint32_t action_flag;
  ADDRT relevant_entry_addr;
  ADDRT first_entry_addr;
  uint8_t magic[8];
  uint32_t flags;
  uint32_t sizeof_descriptor;
  uint32_t sizeof_entry;
  uint32_t action_counter;
  uint64_t action_timestamp;  // CLOCK_MONOTONIC time of last action

  bool Valid() const {
    return version == 1 && strncmp(reinterpret_cast<const char*>(magic), "Android1", 8) == 0;
  }
};

// Match the format of JITCodeEntry in art/runtime/jit/debugger_itnerface.cc.
template <typename ADDRT>
struct JITCodeEntry {
  ADDRT next_addr;
  ADDRT prev_addr;
  ADDRT symfile_addr;
  uint64_t symfile_size;
  uint64_t register_timestamp;  // CLOCK_MONOTONIC time of entry registration

  bool Valid() const {
    return symfile_addr > 0u && symfile_size > 0u;
  }
};

// Match the format of JITCodeEntry in art/runtime/jit/debugger_itnerface.cc.
template <typename ADDRT>
struct __attribute__((packed)) PackedJITCodeEntry {
  ADDRT next_addr;
  ADDRT prev_addr;
  ADDRT symfile_addr;
  uint64_t symfile_size;
  uint64_t register_timestamp;

  bool Valid() const {
    return symfile_addr > 0u && symfile_size > 0u;
  }
};

using JITDescriptor32 = JITDescriptor<uint32_t>;
using JITDescriptor64 = JITDescriptor<uint64_t>;

#if defined(__x86_64__)
using JITCodeEntry32 = PackedJITCodeEntry<uint32_t>;
#else
using JITCodeEntry32 = JITCodeEntry<uint32_t>;
#endif
using JITCodeEntry64 = JITCodeEntry<uint64_t>;

// We want to support both 64-bit and 32-bit simpleperf when profiling either 64-bit or 32-bit
// apps. So using static_asserts to make sure that simpleperf on arm and aarch64 having the same
// view of structures, and simpleperf on i386 and x86_64 having the same view of structures.
static_assert(sizeof(JITDescriptor32) == 48, "");
static_assert(sizeof(JITDescriptor64) == 56, "");
#if defined(__i386__) or defined(__x86_64__)
static_assert(sizeof(JITCodeEntry32) == 28, "");
#else
static_assert(sizeof(JITCodeEntry32) == 32, "");
#endif
static_assert(sizeof(JITCodeEntry64) == 40, "");

JITDebugReader::JITDebugReader(pid_t pid, bool keep_symfiles)
    : pid_(pid),
      keep_symfiles_(keep_symfiles),
      initialized_(false) {
  TryInit();
}

void JITDebugReader::ReadUpdate(std::vector<JITSymFile>* new_jit_symfiles,
                                std::vector<DexSymFile>* new_dex_symfiles) {
  if (!TryInit()) {
    return;
  }
  Descriptor jit_descriptor;
  Descriptor dex_descriptor;
  if (!ReadDescriptors(&jit_descriptor, &dex_descriptor)) {
    return;
  }
  // No change if the timestamps stay the same.
  if (jit_descriptor.action_timestamp == last_jit_descriptor_.action_timestamp &&
      dex_descriptor.action_timestamp == last_dex_descriptor_.action_timestamp) {
    return;
  }

  // Need to read the linked lists.
  bool need_to_read_jit = false;
  bool need_to_read_dex = false;
  if (jit_descriptor.action_timestamp > last_jit_descriptor_.action_timestamp &&
      jit_descriptor.action_counter >= 0) {
    need_to_read_jit = true;
  }
  if (dex_descriptor.action_timestamp > last_dex_descriptor_.action_timestamp &&
      dex_descriptor.action_counter >= 0) {
    need_to_read_dex = true;
  }
  std::vector<CodeEntry> new_jit_entries;
  std::vector<CodeEntry> new_dex_entries;
  for (size_t repeat = 0u; repeat < MAX_LINKED_LIST_READING_COUNT &&
                           (need_to_read_jit || need_to_read_dex); ++repeat) {
    // 1. Read JIT code entries.
    if (need_to_read_jit) {
      std::vector<CodeEntry> tmp_jit_entries;
      if (ReadNewCodeEntries(jit_descriptor, last_jit_descriptor_.action_timestamp,
                             &tmp_jit_entries)) {
        need_to_read_jit = false;
        last_jit_descriptor_ = jit_descriptor;
        new_jit_entries.insert(new_jit_entries.end(), tmp_jit_entries.begin(),
                               tmp_jit_entries.end());
      }
    }
    // 2. Read dex code entries.
    if (need_to_read_dex) {
      std::vector<CodeEntry> tmp_dex_entries;
      if (ReadNewCodeEntries(dex_descriptor, last_dex_descriptor_.action_timestamp,
                             &tmp_dex_entries)) {
        need_to_read_dex = false;
        last_dex_descriptor_ = dex_descriptor;
        new_dex_entries.insert(new_dex_entries.end(), tmp_dex_entries.begin(),
                               tmp_dex_entries.end());
      }
    }
    // 3. Reread descriptors to detect any more updates.
    Descriptor tmp_jit_descriptor;
    Descriptor tmp_dex_descriptor;
    if (!ReadDescriptors(&tmp_jit_descriptor, &tmp_dex_descriptor)) {
      break;
    }
    if (jit_descriptor.action_timestamp > last_jit_descriptor_.action_timestamp &&
        jit_descriptor.action_counter >= 0) {
      need_to_read_jit = true;
    }
    if (dex_descriptor.action_timestamp > last_dex_descriptor_.action_timestamp &&
        dex_descriptor.action_counter >= 0) {
      need_to_read_dex = true;
    }
  }

  // 4. Now the code entries we read are valid, and we can return them to the caller.
  if (!new_jit_entries.empty()) {
    ReadJITSymFiles(new_jit_entries, new_jit_symfiles);
  }
  if (!new_dex_entries.empty()) {
    ReadDexSymFiles(new_dex_entries, new_dex_symfiles);
  }
}

bool JITDebugReader::TryInit() {
  if (initialized_) {
    return true;
  }
  // 1. Read map file to find the location of libart.so.
  std::vector<ThreadMmap> thread_mmaps;
  if (!GetThreadMmapsInProcess(pid_, &thread_mmaps)) {
    return false;
  }
  std::string art_lib_path;
  for (auto& map : thread_mmaps) {
    if (android::base::EndsWith(map.name, "libart.so")) {
      art_lib_path = map.name;
      break;
    }
  }
  if (art_lib_path.empty()) {
    return false;
  }
  is_64bit_ = art_lib_path.find("lib64") != std::string::npos;

  // 2. Read libart.so to find the addresses of __jit_debug_descriptor and __dex_debug_descriptor.
  uint64_t min_vaddr_in_file;
  ElfStatus status = ReadMinExecutableVirtualAddressFromElfFile(art_lib_path, BuildId(),
                     &min_vaddr_in_file);
  if (status != ElfStatus::NO_ERROR) {
    LOG(ERROR) << "ReadMinExecutableVirtualAddress failed, status = " << status;
    return false;
  }
  uint64_t min_vaddr_in_memory = 0u;
  for (auto& map : thread_mmaps) {
    if (map.executable && map.name == art_lib_path) {
      min_vaddr_in_memory = map.start_addr;
      break;
    }
  }
  if (min_vaddr_in_memory == 0u) {
    return false;
  }
  const char* jit_str = "__jit_debug_descriptor";
  const char* dex_str = "__dex_debug_descriptor";
  uint64_t jit_addr = 0u;
  uint64_t dex_addr = 0u;

  auto callback = [&](const ElfFileSymbol& symbol) {
    if (symbol.name == jit_str) {
      jit_addr = symbol.vaddr - min_vaddr_in_file + min_vaddr_in_memory;
    } else if (symbol.name == dex_str) {
      dex_addr = symbol.vaddr - min_vaddr_in_file + min_vaddr_in_memory;
    }
  };
  if (ParseDynamicSymbolsFromElfFile(art_lib_path, callback) != ElfStatus::NO_ERROR) {
    return false;
  }
  if (jit_addr == 0u || dex_addr == 0u) {
    return false;
  }
  descriptors_addr_ = std::min(jit_addr, dex_addr);
  descriptors_size_ = std::max(jit_addr, dex_addr) +
      (is_64bit_ ? sizeof(JITDescriptor64) : sizeof(JITDescriptor32)) - descriptors_addr_;
  if (descriptors_size_ >= 4096u) {
    PLOG(WARNING) << "The descriptors_size is unexpected large: " << descriptors_size_;
  }
  descriptors_buf_.resize(descriptors_size_);
  jit_descriptor_offset_ = jit_addr - descriptors_addr_;
  dex_descriptor_offset_ = dex_addr - descriptors_addr_;
  last_jit_descriptor_.action_timestamp = 0u;
  last_dex_descriptor_.action_timestamp = 0u;
  initialized_ = true;
  return true;
}

bool JITDebugReader::ReadRemoteMem(uint64_t remote_addr, uint64_t size, void* data) {
  iovec local_iov;
  local_iov.iov_base = data;
  local_iov.iov_len = size;
  iovec remote_iov;
  remote_iov.iov_base = reinterpret_cast<void*>(static_cast<uintptr_t>(remote_addr));
  remote_iov.iov_len = size;
  ssize_t result = process_vm_readv(pid_, &local_iov, 1, &remote_iov, 1, 0);
  if (static_cast<size_t>(result) != size) {
    PLOG(DEBUG) << "ReadRemoteMem(" << " pid " << pid_ << ", addr " << std::hex
                << remote_addr << ", size " << size << ") failed";
    return false;
  }
  return true;
}

bool JITDebugReader::ReadDescriptors(Descriptor* jit_descriptor, Descriptor* dex_descriptor) {
  if (!ReadRemoteMem(descriptors_addr_, descriptors_size_, descriptors_buf_.data())) {
    return false;
  }
  return LoadDescriptor(descriptors_buf_.data() + jit_descriptor_offset_, jit_descriptor) &&
      LoadDescriptor(descriptors_buf_.data() + dex_descriptor_offset_, dex_descriptor);
}

bool JITDebugReader::LoadDescriptor(const char* data, Descriptor* descriptor) {
  if (is_64bit_) {
    return LoadDescriptorImpl<JITDescriptor64>(data, descriptor);
  }
  return LoadDescriptorImpl<JITDescriptor32>(data, descriptor);
}

template <typename DescriptorT>
bool JITDebugReader::LoadDescriptorImpl(const char* data, Descriptor* descriptor) {
  DescriptorT raw_descriptor;
  MoveFromBinaryFormat(raw_descriptor, data);
  if (!raw_descriptor.Valid() || sizeof(raw_descriptor) != raw_descriptor.sizeof_descriptor ||
      (is_64bit_ ? sizeof(JITCodeEntry64) : sizeof(JITCodeEntry32)) != raw_descriptor.sizeof_entry
      ) {
    return false;
  }
  descriptor->action_counter = raw_descriptor.action_counter;
  descriptor->action_timestamp = raw_descriptor.action_timestamp;
  descriptor->first_entry_addr = raw_descriptor.first_entry_addr;
  return true;
}

// Read new code entries with timestamp > last_action_timestamp.
// Since we don't stop the app process while reading code entries, it is possible we are reading
// broken data. So return false once we detect that the data is broken.
bool JITDebugReader::ReadNewCodeEntries(const Descriptor& descriptor,
                                        uint64_t last_action_timestamp,
                                        std::vector<CodeEntry>* new_code_entries) {
  if (is_64bit_) {
    return ReadNewCodeEntriesImpl<JITDescriptor64, JITCodeEntry64>(descriptor,
                                                                   last_action_timestamp,
                                                                   new_code_entries);
  }
  return ReadNewCodeEntriesImpl<JITDescriptor32, JITCodeEntry32>(descriptor,
                                                                 last_action_timestamp,
                                                                 new_code_entries);
}

template <typename DescriptorT, typename CodeEntryT>
bool JITDebugReader::ReadNewCodeEntriesImpl(const Descriptor& descriptor,
                                            uint64_t last_action_timestamp,
                                            std::vector<CodeEntry>* new_code_entries) {
  uint64_t current_entry_addr = descriptor.first_entry_addr;
  uint64_t prev_entry_addr = 0u;
  std::unordered_set<uint64_t> entry_addr_set;
  for (size_t i = 0u; i < MAX_LINKLED_LIST_LENGTH && current_entry_addr != 0u; ++i) {
    if (entry_addr_set.find(current_entry_addr) != entry_addr_set.end()) {
      // We enter a loop, which means a broken linked list.
      return false;
    }
    CodeEntryT entry;
    if (!ReadRemoteMem(current_entry_addr, sizeof(entry), &entry)) {
      return false;
    }
    if (entry.prev_addr != prev_entry_addr || !entry.Valid()) {
      // A broken linked list
      return false;
    }
    if (entry.register_timestamp <= last_action_timestamp) {
      // The linked list has entries with timestamp in decreasing order. So stop searching
      // once we hit an entry with timestamp <= last_action_timestmap.
      break;
    }
    CodeEntry code_entry;
    code_entry.addr = current_entry_addr;
    code_entry.symfile_addr = entry.symfile_addr;
    code_entry.symfile_size = entry.symfile_size;
    new_code_entries->push_back(code_entry);
    entry_addr_set.insert(current_entry_addr);
    prev_entry_addr = current_entry_addr;
    current_entry_addr = entry.next_addr;
  }
  return true;
}

void JITDebugReader::ReadJITSymFiles(const std::vector<CodeEntry>& jit_entries,
                                     std::vector<JITSymFile>* jit_symfiles) {
  std::vector<char> data;
  for (auto& jit_entry : jit_entries) {
    if (jit_entry.symfile_size > MAX_JIT_SYMFILE_SIZE) {
      continue;
    }
    if (data.size() < jit_entry.symfile_size) {
      data.resize(jit_entry.symfile_size);
    }
    if (!ReadRemoteMem(jit_entry.symfile_addr, jit_entry.symfile_size, data.data())) {
      continue;
    }
    if (!IsValidElfFileMagic(data.data(), jit_entry.symfile_size)) {
      continue;
    }
    uint64_t min_addr = UINT64_MAX;
    uint64_t max_addr = 0;
    auto callback = [&](const ElfFileSymbol& symbol) {
      min_addr = std::min(min_addr, symbol.vaddr);
      max_addr = std::max(max_addr, symbol.vaddr + symbol.len);
      LOG(VERBOSE) << "JITSymbol " << symbol.name << " at [" << std::hex << symbol.vaddr
                   << " - " << (symbol.vaddr + symbol.len) << " with size " << symbol.len;
    };
    if (ParseSymbolsFromElfFileInMemory(data.data(), jit_entry.symfile_size, callback) !=
        ElfStatus::NO_ERROR || min_addr >= max_addr) {
      continue;
    }
    std::unique_ptr<TemporaryFile> tmp_file = ScopedTempFiles::CreateTempFile(!keep_symfiles_);
    if (tmp_file == nullptr || !android::base::WriteFully(tmp_file->fd, data.data(),
                                                          jit_entry.symfile_size)) {
      continue;
    }
    if (keep_symfiles_) {
      tmp_file->DoNotRemove();
    }
    JITSymFile symfile;
    symfile.addr = min_addr;
    symfile.len = max_addr - min_addr;
    symfile.file_path = tmp_file->path;
    jit_symfiles->push_back(symfile);
  }
}

void JITDebugReader::ReadDexSymFiles(const std::vector<CodeEntry>& dex_entries,
                                     std::vector<DexSymFile>* dex_symfiles) {
  std::vector<ThreadMmap> thread_mmaps;
  if (!GetThreadMmapsInProcess(pid_, &thread_mmaps)) {
    return;
  }
  auto comp = [](const ThreadMmap& map, uint64_t addr) {
    return map.start_addr <= addr;
  };
  for (auto& dex_entry : dex_entries) {
    auto it = std::lower_bound(thread_mmaps.begin(), thread_mmaps.end(),
                               dex_entry.symfile_addr, comp);
    if (it == thread_mmaps.begin()) {
      continue;
    }
    --it;
    if (it->start_addr + it->len < dex_entry.symfile_addr + dex_entry.symfile_size) {
      continue;
    }
    if (!IsRegularFile(it->name)) {
      // TODO: read dex file only exist in memory?
      continue;
    }
    // Offset of dex file in .vdex file or .apk file.
    uint64_t dex_file_offset = dex_entry.symfile_addr - it->start_addr + it->pgoff;
    DexSymFile symfile;
    symfile.addr = dex_entry.symfile_addr;
    symfile.len = dex_entry.symfile_size;
    symfile.pgoff = dex_file_offset;
    symfile.file_path = it->name;
    dex_symfiles->push_back(symfile);
    LOG(VERBOSE) << "DexFile " << symfile.file_path << "+" << std::hex << dex_file_offset
                 << " at [" << std::hex << symfile.addr << " - " << (symfile.addr + symfile.len)
                 << "] with size " << symfile.len;
  }
}

}  // namespace simpleperf
