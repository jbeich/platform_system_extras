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

#ifndef SIMPLE_PERF_JIT_DEBUG_READER_H_
#define SIMPLE_PERF_JIT_DEBUG_READER_H_

#include <unistd.h>

#include <functional>
#include <memory>
#include <stack>
#include <unordered_set>
#include <vector>

#include <android-base/logging.h>
#include <android-base/test_utils.h>

namespace simpleperf {

namespace jit_debug_reader_impl {

// FastCheckingStack allows O(1) checking whether an entry exists in the stack.
// It is used to tracking the change of jit_code_entry linked list and art_debug_dexfiles.
template <typename T, typename HashT>
class FastCheckingStack {
 public:
  typedef std::unordered_set<T, HashT> set_type;

  FastCheckingStack(const HashT& hash_func)
      : set_(100, hash_func) {
  }

  const T* Head() const {
    return stack_.empty() ? nullptr : &stack_.top();
  }

  void Pop() {
    CHECK(!stack_.empty());
    set_.erase(stack_.top());
    stack_.pop();
  }

  void Push(const T& data) {
    stack_.push(data);
    set_.insert(data);
  }

  bool Contains(const T& data) const {
    return set_.find(data) != set_.end();
  }

 private:
  std::stack<T> stack_;
  set_type set_;
};

struct JITEntry {
  uint64_t jit_code_entry_addr;
  uint64_t symfile_addr;
  uint64_t symfile_size;

  bool operator==(const JITEntry& o) const {
    return jit_code_entry_addr == o.jit_code_entry_addr &&
           symfile_addr == o.symfile_addr && symfile_size == o.symfile_size;
  }
  bool operator!=(const JITEntry& o) const {
    return !(*this == o);
  }
};

size_t JITEntryHash(const JITEntry& entry);

using JITEntryStack = FastCheckingStack<JITEntry, decltype(&JITEntryHash)>;

struct DexEntry {
  uint64_t dexfile_entry_addr;
  uint64_t dexfile_addr;

  bool operator==(const DexEntry& o) const {
    return dexfile_entry_addr == o.dexfile_entry_addr && dexfile_addr == o.dexfile_addr;
  }
  bool operator!=(const DexEntry& o) const {
    return !(*this == o);
  }
};

size_t DexEntryHash(const DexEntry& entry);

using DexEntryStack = FastCheckingStack<DexEntry, decltype(&DexEntryHash)>;

}  // jit_debug_reader_impl

struct JITSymFile {
  uint64_t addr;
  uint64_t len;
  std::string file_path;
};

struct DexFile {
  uint64_t addr;
};

class JITDebugReader {
 public:
  JITDebugReader(pid_t pid);

  pid_t Pid() const {
    return pid_;
  }

  void ReadUpdate(std::vector<JITSymFile>* new_symfiles, std::vector<DexFile>* new_dexfiles);

 private:
  bool TryInit();
  bool ReadRemoteMem(uint64_t remote_addr, uint64_t size, void* data);
  void GetTimestampsFromRemoteData(const char* remote_data, uint32_t& jit_timestamp,
                                   uint32_t& dex_timestamp);
  bool ReadNewJITEntries(std::vector<jit_debug_reader_impl::JITEntry>* new_jit_entries);
  template <typename DescriptorT, typename CodeEntryT>
  bool ReadNewJITEntriesImpl(std::vector<jit_debug_reader_impl::JITEntry>* new_jit_entries);

  bool ReadNewDexEntries(std::vector<jit_debug_reader_impl::DexEntry>* new_dex_entries);
  template <typename ADDRT, typename DexFileEntryT>
  bool ReadNewDexEntriesImpl(std::vector<jit_debug_reader_impl::DexEntry>* new_dex_entries);

  void ReadNewSymFiles(const std::vector<jit_debug_reader_impl::JITEntry>& new_jit_entries,
                       std::vector<JITSymFile>* new_symfiles);
  void BuildNewDexFiles(const std::vector<jit_debug_reader_impl::DexEntry>& new_dex_entries,
                        std::vector<DexFile>* new_dexfiles);

  pid_t pid_;
  bool initialized_;
  bool is_64bit_;

  // Timestamps can be read in range [timestamps_addr, timestamps_addr + timestamps_size] in
  // a single process_vm_readv() call, 
  uint64_t timestamps_addr_;
  uint64_t timestamps_size_;
  // offset relative to timestamps_addr
  uint64_t jit_debug_descriptor_timestamp_offset_;
  // offset relative to timestamps_addr
  uint64_t art_debug_dexfiles_timestamp_offset_;

  uint64_t jit_debug_descriptor_addr_;
  uint64_t art_debug_dexfiles_addr_;

  // Below variables keep the state we know about the remote process:
  uint32_t jit_debug_descriptor_timestamp_;
  uint32_t art_debug_dexfiles_timestamp_;
  jit_debug_reader_impl::JITEntryStack jit_entry_stack_;
  jit_debug_reader_impl::DexEntryStack dex_entry_stack_;

  // tmpfiles used to store jit sym files on disk
  std::vector<std::unique_ptr<TemporaryFile>> tmpfiles_;
};

}  //namespace simpleperf

#endif   // SIMPLE_PERF_JIT_DEBUG_READER_H_