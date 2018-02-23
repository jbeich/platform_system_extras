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

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "environment.h"
#include "read_elf.h"
#include "utils.h"

namespace simpleperf {

namespace jit_debug_reader_impl {

size_t JITEntryHash(const JITEntry& entry) {
  return static_cast<size_t>(entry.jit_code_entry_addr);
}

size_t DexEntryHash(const DexEntry& entry) {
  return static_cast<size_t>(entry.dexfile_entry_addr);
}

}  // namespace jit_debug_reader_impl

using jit_debug_reader_impl::JITEntry;
using jit_debug_reader_impl::DexEntry;

// If the timestamps are changed, we need to read the linked lists. But the timestamps can be
// changed while we reading the linked lists. So we reread the timestamps after reading the
// linked lists, and reread the linked lists if need. To avoid an endless loop, using
// MAX_LINKED_LIST_READING_COUNT as the uplimit of reading linked lists in one ReadUpdate() call.
static constexpr size_t MAX_LINKED_LIST_READING_COUNT = 3u;

// To avoid too long time reading the jit linked list, set an uplimit of entries read from the
// linked list.
static constexpr size_t EXPECTED_MAX_JIT_LINKLED_LIST_LENGTH = 1024u;

// To avoid too long time reading the dex linked list, set an uplimit of entries read from the
// linked list.
static constexpr size_t EXPECTED_MAX_DEX_LINKED_LIST_LENGTH = 1024u;

// If the size of a symfile is larger than EXPECTED_MAX_SYMFILE_SIZE, we think of it as an invalid
// symfile.
static constexpr size_t EXPECTED_MAX_SYMFILE_SIZE = 1024 * 1024u;

template <typename ADDRT>
struct JITDescriptor {
  uint32_t version;
  uint32_t action_flag;
  ADDRT relevant_entry_addr;
  ADDRT first_entry_addr;

  bool Valid() const {
    return version == 1;
  }
};

template <typename ADDRT>
struct JITCodeEntry {
  ADDRT next_addr;
  ADDRT prev_addr;
  ADDRT symfile_addr;
  uint64_t symfile_size;

  bool Valid() const {
    return symfile_addr > 0u && symfile_size > 0u && symfile_size <= EXPECTED_MAX_SYMFILE_SIZE;
  }
};

template <typename ADDRT>
struct DexFileEntry {
  ADDRT next_addr;
  ADDRT prev_addr;
  uint64_t dexfile_addr;

  bool Valid() const {
    return dexfile_addr > 0u;
  }
};

using JITDescriptor32 = JITDescriptor<uint32_t>;
using JITCodeEntry32 = JITCodeEntry<uint32_t>;
using DexFileEntry32 = DexFileEntry<uint32_t>;

using JITDescriptor64 = JITDescriptor<uint64_t>;
using JITCodeEntry64 = JITCodeEntry<uint64_t>;
using DexFileEntry64 = DexFileEntry<uint64_t>;

JITDebugReader::JITDebugReader(pid_t pid)
    : pid_(pid),
      initialized_(false),
      jit_entry_stack_(jit_debug_reader_impl::JITEntryHash),
      dex_entry_stack_(jit_debug_reader_impl::DexEntryHash) {
  TryInit();
}

void JITDebugReader::ReadUpdate(std::vector<JITSymFile>* new_symfiles,
                                std::vector<DexFile>* new_dexfiles) {
  if (!TryInit()) {
    return;
  }
  std::vector<char> data(timestamps_size_);
  uint32_t jit_timestamp;
  uint32_t dex_timestamp;
  if (!ReadRemoteMem(timestamps_addr_, timestamps_size_, data.data())) {
    return;
  }
  GetTimestampsFromRemoteData(data.data(), jit_timestamp, dex_timestamp);
  if (jit_timestamp == jit_debug_descriptor_timestamp_ &&
      dex_timestamp == art_debug_dexfiles_timestamp_) {
    return;
  }

  // Need to read the linked lists.
  bool need_to_read_jit = (jit_timestamp > jit_debug_descriptor_timestamp_);
  bool need_to_read_dex = (dex_timestamp > art_debug_dexfiles_timestamp_);
  std::vector<JITEntry> new_jit_entries;
  std::vector<DexEntry> new_dex_entries;
  for (size_t repeat = 0u; repeat < MAX_LINKED_LIST_READING_COUNT &&
                           (need_to_read_jit || need_to_read_dex); ++repeat) {
    // 1. Read linked lists.
    if (need_to_read_jit) {
      std::vector<JITEntry> tmp_jit_entries;
      if (ReadNewJITEntries(&tmp_jit_entries)) {
        need_to_read_jit = false;
        jit_debug_descriptor_timestamp_ = jit_timestamp;
        new_jit_entries.insert(new_jit_entries.end(), tmp_jit_entries.begin(),
                               tmp_jit_entries.end());
      }
    }
    if (need_to_read_dex) {
      std::vector<DexEntry> tmp_dex_entries;
      if (ReadNewDexEntries(&tmp_dex_entries)) {
        need_to_read_dex = false;
        art_debug_dexfiles_timestamp_ = jit_timestamp;
        new_dex_entries.insert(new_dex_entries.end(), tmp_dex_entries.begin(),
                               tmp_dex_entries.end());
      }
    }
    // 2. Reread timestamps.
    if (!ReadRemoteMem(timestamps_addr_, timestamps_size_, data.data())) {
      break;
    }
    uint32_t tmp_jit_timestamp;
    uint32_t tmp_dex_timestamp;
    GetTimestampsFromRemoteData(data.data(), tmp_jit_timestamp, tmp_dex_timestamp);
    if (tmp_jit_timestamp != jit_timestamp) {
      jit_timestamp = tmp_jit_timestamp;
      need_to_read_jit = true;
    }
    if (tmp_dex_timestamp != dex_timestamp) {
      dex_timestamp = tmp_dex_timestamp;
      need_to_read_dex = true;
    }
  }

  // 3. Now the linked lists we read are valid, and we can return them to the caller.
  if (!new_jit_entries.empty()) {
    ReadNewSymFiles(new_jit_entries, new_symfiles);
  }
  if (!new_dex_entries.empty()) {
    BuildNewDexFiles(new_dex_entries, new_dexfiles);
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

  // 2. Read libart.so to find the addresses of __jit_debug_descriptor,
  // __jit_debug_descriptor_timestamp, __art_debug_dexfiles, __art_debug_dexfiles_timestamp.
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
  const char* jit_timestamp_str = "__jit_debug_descriptor_timestamp";
  const char* dex_str = "__art_debug_dexfiles";
  const char* dex_timestamp_str = "__art_debug_dexfiles_timestamp";
  std::unordered_map<std::string, uint64_t> name_to_addr_map;
  name_to_addr_map[jit_str] = 0u;
  name_to_addr_map[jit_timestamp_str] = 0u;
  name_to_addr_map[dex_str] = 0u;
  name_to_addr_map[dex_timestamp_str] = 0u;

  auto callback = [&](const ElfFileSymbol& symbol) {
    auto it = name_to_addr_map.find(symbol.name);
    if (it != name_to_addr_map.end()) {
      it->second = symbol.vaddr - min_vaddr_in_file + min_vaddr_in_memory;
    }
  };
  if (ParseDynamicSymbolsFromElfFile(art_lib_path, callback) != ElfStatus::NO_ERROR) {
    return false;
  }
  jit_debug_descriptor_addr_ = name_to_addr_map[jit_str];
  art_debug_dexfiles_addr_ = name_to_addr_map[dex_str];
  uint64_t jit_timestamp_addr = name_to_addr_map[jit_timestamp_str];
  uint64_t dex_timestamp_addr = name_to_addr_map[dex_timestamp_str];
  if (jit_debug_descriptor_addr_ == 0u || art_debug_dexfiles_addr_ == 0u ||
      jit_timestamp_addr == 0u || dex_timestamp_addr == 0u) {
    return false;
  }

  timestamps_addr_ = std::min(jit_timestamp_addr, dex_timestamp_addr);
  timestamps_size_ = std::max(jit_timestamp_addr, dex_timestamp_addr) + sizeof(uint32_t) -
                     timestamps_addr_;
  jit_debug_descriptor_timestamp_offset_ = jit_timestamp_addr - timestamps_addr_;
  art_debug_dexfiles_timestamp_offset_ = dex_timestamp_addr - timestamps_addr_;
  jit_debug_descriptor_timestamp_ = 0u;
  art_debug_dexfiles_timestamp_ = 0u;
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

void JITDebugReader::GetTimestampsFromRemoteData(const char* remote_data, uint32_t& jit_timestamp,
                                                 uint32_t& dex_timestamp) {
  const char* p = remote_data + jit_debug_descriptor_timestamp_offset_;
  MoveFromBinaryFormat(jit_timestamp, p);
  p = remote_data + art_debug_dexfiles_timestamp_offset_;
  MoveFromBinaryFormat(dex_timestamp, p);
}

// Since we don't stop the app process while reading jit entries, it is possible we are reading
// broken data. So return false once we detect that the data is broken.
bool JITDebugReader::ReadNewJITEntries(std::vector<JITEntry>* new_jit_entries) {
  if (is_64bit_) {
    return ReadNewJITEntriesImpl<JITDescriptor64, JITCodeEntry64>(new_jit_entries);
  }
  return ReadNewJITEntriesImpl<JITDescriptor32, JITCodeEntry32>(new_jit_entries);
}

template <typename DescriptorT, typename CodeEntryT>
bool JITDebugReader::ReadNewJITEntriesImpl(std::vector<JITEntry>* new_jit_entries) {
  DescriptorT descriptor;
  if (!ReadRemoteMem(jit_debug_descriptor_addr_, sizeof(descriptor), &descriptor)) {
    return false;
  }
  if (!descriptor.Valid()) {
    return false;
  }
  uint64_t current_entry_addr = descriptor.first_entry_addr;
  uint64_t prev_entry_addr = 0u;
  std::unordered_set<uint64_t> entry_addr_set;
  for (size_t i = 0u; i < EXPECTED_MAX_JIT_LINKLED_LIST_LENGTH && current_entry_addr != 0u; ++i) {
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
    JITEntry jit_entry;
    jit_entry.jit_code_entry_addr = current_entry_addr;
    jit_entry.symfile_addr = entry.symfile_addr;
    jit_entry.symfile_size = entry.symfile_size;
    if (jit_entry_stack_.Contains(jit_entry)) {
      // We hit an entry that we have seen before, which means old entries before it have been
      // removed and there is no new entry after it.
      while (*jit_entry_stack_.Head() != jit_entry) {
        jit_entry_stack_.Pop();
      }
      break;
    }
    new_jit_entries->push_back(jit_entry);
    prev_entry_addr = current_entry_addr;
    current_entry_addr = entry.next_addr;
  }
  for (auto it = new_jit_entries->rbegin(); it != new_jit_entries->rend(); ++it) {
    jit_entry_stack_.Push(*it);
  }
  return true;
}

bool JITDebugReader::ReadNewDexEntries(std::vector<DexEntry>* new_dex_entries) {
  if (is_64bit_) {
    return ReadNewDexEntriesImpl<uint64_t, DexFileEntry64>(new_dex_entries);
  }
  return ReadNewDexEntriesImpl<uint32_t, DexFileEntry32>(new_dex_entries);
}

template <typename ADDRT, typename DexFileEntryT>
bool JITDebugReader::ReadNewDexEntriesImpl(std::vector<DexEntry>* new_dex_entries) {
  ADDRT current_entry_addr;
  if (!ReadRemoteMem(art_debug_dexfiles_addr_, sizeof(ADDRT), &current_entry_addr)) {
    return false;
  }
  ADDRT prev_entry_addr = 0u;
  std::unordered_set<uint64_t> entry_addr_set;
  for (size_t i = 0u; i < EXPECTED_MAX_DEX_LINKED_LIST_LENGTH && current_entry_addr != 0u; ++i) {
    if (entry_addr_set.find(current_entry_addr) != entry_addr_set.end()) {
      // We enter a loop, which means a broken linked list.
      return false;
    }
    DexFileEntryT entry;
    if (!ReadRemoteMem(current_entry_addr, sizeof(DexFileEntryT), &entry)) {
      return false;
    }
    if (entry.prev_addr != prev_entry_addr || !entry.Valid()) {
      return false;
    }
    DexEntry dex_entry;
    dex_entry.dexfile_entry_addr = current_entry_addr;
    dex_entry.dexfile_addr = entry.dexfile_addr;
    if (dex_entry_stack_.Contains(dex_entry)) {
      // We hit an entry that we have seen before, which means old entries before it have been
      // removed and there is no new entry after it.
      while (*dex_entry_stack_.Head() != dex_entry) {
        dex_entry_stack_.Pop();
      }
      break;
    }
    new_dex_entries->push_back(dex_entry);
    prev_entry_addr = current_entry_addr;
    current_entry_addr = entry.next_addr;
  }
  for (auto it = new_dex_entries->rbegin(); it != new_dex_entries->rend(); ++it) {
    dex_entry_stack_.Push(*it);
  }
  return true;
}

void JITDebugReader::ReadNewSymFiles(const std::vector<JITEntry>& new_jit_entries,
                                     std::vector<JITSymFile>* new_symfiles) {
  std::vector<char> data;
  for (auto& jit_entry : new_jit_entries) {
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
      LOG(VERBOSE) << "JITSymbol " << symbol.name << "  at [" << std::hex << symbol.vaddr
                   << " - " << (symbol.vaddr + symbol.len);
    };
    if (ParseSymbolsFromElfFileInMemory(data.data(), jit_entry.symfile_size, callback) !=
        ElfStatus::NO_ERROR) {
      continue;
    }
    std::unique_ptr<TemporaryFile> tmp_file = CreateTempFileUsedInRecording();
    if (!android::base::WriteFully(tmp_file->fd, data.data(), jit_entry.symfile_size)) {
      continue;
    }
    JITSymFile symfile;
    symfile.addr = min_addr;
    symfile.len = max_addr - min_addr;
    symfile.file_path = tmp_file->path;
    new_symfiles->push_back(symfile);
    close(tmp_file->fd);
    tmpfiles_.push_back(std::move(tmp_file));
  }
}

void JITDebugReader::BuildNewDexFiles(const std::vector<DexEntry>& new_dex_entries,
                                      std::vector<DexFile>* new_dexfiles) {
  for (auto& dex_entry : new_dex_entries) {
    DexFile dexfile;
    dexfile.addr = dex_entry.dexfile_addr;
    new_dexfiles->push_back(dexfile);
  }
}

}  // namespace simpleperf