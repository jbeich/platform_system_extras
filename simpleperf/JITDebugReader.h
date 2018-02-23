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

struct Descriptor {
  int32_t action_counter;  // number of actions, or -1 if locked.
  uint64_t action_timestamp;  // CLOCK_MONOTONIC time of last action
  uint64_t first_entry_addr;
};

struct CodeEntry {
  uint64_t addr;
  uint64_t symfile_addr;
  uint64_t symfile_size;
  uint64_t timestamp;  // CLOCK_MONOTONIC time of last action

  bool operator==(const CodeEntry& o) const {
    return addr == o.addr && symfile_addr == o.symfile_addr && symfile_size == o.symfile_size;
  }
  bool operator!=(const CodeEntry& o) const {
    return !(*this == o);
  }
  static size_t Hash(const CodeEntry& entry) {
    return static_cast<size_t>(entry.addr);
  }
};

}  // jit_debug_reader_impl

struct JITSymFile {
  uint64_t addr;
  uint64_t len;
  std::string file_path;
};

struct DexSymFile {
  uint64_t addr;
  uint64_t len;
  uint64_t pgoff;
  std::string file_path;
};

class JITDebugReader {
 public:
  JITDebugReader(pid_t pid, bool keep_symfiles);

  pid_t Pid() const {
    return pid_;
  }

  void ReadUpdate(std::vector<JITSymFile>* new_jit_symfiles,
                  std::vector<DexSymFile>* new_dex_symfiles);

 private:
  bool TryInit();
  bool ReadRemoteMem(uint64_t remote_addr, uint64_t size, void* data);
  bool ReadDescriptors(jit_debug_reader_impl::Descriptor* jit_descriptor,
                       jit_debug_reader_impl::Descriptor* dex_descriptor);
  bool LoadDescriptor(const char* data, jit_debug_reader_impl::Descriptor* descriptor);
  template <typename DescriptorT>
  bool LoadDescriptorImpl(const char* data, jit_debug_reader_impl::Descriptor* descriptor);

  bool ReadNewCodeEntries(const jit_debug_reader_impl::Descriptor& descriptor,
                          uint64_t last_action_timestamp,
                          std::vector<jit_debug_reader_impl::CodeEntry>* new_code_entries);
  template <typename DescriptorT, typename CodeEntryT>
  bool ReadNewCodeEntriesImpl(const jit_debug_reader_impl::Descriptor& descriptor,
                              uint64_t last_action_timestamp,
                              std::vector<jit_debug_reader_impl::CodeEntry>* new_code_entries);

  void ReadJITSymFiles(const std::vector<jit_debug_reader_impl::CodeEntry>& jit_entries,
                       std::vector<JITSymFile>* jit_symfiles);
  void ReadDexSymFiles(const std::vector<jit_debug_reader_impl::CodeEntry>& dex_entries,
                        std::vector<DexSymFile>* dex_symfiles);

  pid_t pid_;
  bool keep_symfiles_;
  bool initialized_;
  bool is_64bit_;

  // The jit descriptor and dex descriptor can be read in one process_vm_readv() call.
  uint64_t descriptors_addr_;
  uint64_t descriptors_size_;
  std::vector<char> descriptors_buf_;
  // offset relative to descriptors_addr
  uint64_t jit_descriptor_offset_;
  // offset relative to descriptors_addr
  uint64_t dex_descriptor_offset_;

  // Below variables keep the state we know about the remote process:
  jit_debug_reader_impl::Descriptor last_jit_descriptor_;
  jit_debug_reader_impl::Descriptor last_dex_descriptor_;
};

}  //namespace simpleperf

#endif   // SIMPLE_PERF_JIT_DEBUG_READER_H_
