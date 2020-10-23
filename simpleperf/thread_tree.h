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

#ifndef SIMPLE_PERF_THREAD_TREE_H_
#define SIMPLE_PERF_THREAD_TREE_H_

#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <unordered_map>

#include "dso.h"

struct Record;

constexpr char DEFAULT_KERNEL_MMAP_NAME[] = "[kernel.kallsyms]";
// Seen in perf.data file generated by perf.
constexpr char DEFAULT_KERNEL_MMAP_NAME_PERF[] = "[kernel.kallsyms]_text";
constexpr char DEFAULT_EXECNAME_FOR_THREAD_MMAP[] = "//anon";

namespace simpleperf {

namespace map_flags {
constexpr uint32_t PROT_JIT_SYMFILE_MAP = 0x4000;
}  // namespace map_flags

struct MapEntry {
  uint64_t start_addr;
  uint64_t len;
  uint64_t pgoff;
  Dso* dso;
  bool in_kernel;
  uint32_t flags;

  MapEntry(uint64_t start_addr, uint64_t len, uint64_t pgoff, Dso* dso, bool in_kernel,
           uint32_t flags = 0)
      : start_addr(start_addr),
        len(len),
        pgoff(pgoff),
        dso(dso),
        in_kernel(in_kernel),
        flags(flags) {}
  MapEntry() {}

  uint64_t get_end_addr() const { return start_addr + len; }

  uint64_t Contains(uint64_t addr) const { return addr >= start_addr && addr < get_end_addr(); }

  uint64_t GetVaddrInFile(uint64_t addr) const {
    if (Contains(addr)) {
      return dso->IpToVaddrInFile(addr, start_addr, pgoff);
    }
    return 0;
  }
};

struct MapSet {
  std::map<uint64_t, const MapEntry*> maps;  // Map from start_addr to a MapEntry.
  uint64_t version = 0u;                     // incremented each time changing maps

  const MapEntry* FindMapByAddr(uint64_t addr) const;
};

struct ThreadEntry {
  int pid;
  int tid;
  const char* comm;              // It always refers to the latest comm.
  std::shared_ptr<MapSet> maps;  // maps is shared by threads in the same process.
};

// ThreadTree contains thread information (in ThreadEntry) and mmap information
// (in MapEntry) of the monitored threads. It also has interface to access
// symbols in executable binaries mapped in the monitored threads.
class ThreadTree {
 public:
  ThreadTree()
      : show_ip_for_unknown_symbol_(false),
        show_mark_for_unknown_symbol_(false),
        unknown_symbol_("unknown", 0, std::numeric_limits<unsigned long long>::max()) {
    unknown_dso_ = Dso::CreateDso(DSO_UNKNOWN_FILE, "unknown");
    unknown_map_ =
        MapEntry(0, std::numeric_limits<unsigned long long>::max(), 0, unknown_dso_.get(), false);
    kernel_dso_ = Dso::CreateDso(DSO_KERNEL, DEFAULT_KERNEL_MMAP_NAME);
    // We can't dump comm for pid 0 from /proc, so add it's name here.
    SetThreadName(0, 0, "swapper");
  }

  void SetThreadName(int pid, int tid, const std::string& comm);
  void ForkThread(int pid, int tid, int ppid, int ptid);
  ThreadEntry* FindThread(int tid);
  ThreadEntry* FindThreadOrNew(int pid, int tid);
  void ExitThread(int pid, int tid);
  void AddKernelMap(uint64_t start_addr, uint64_t len, uint64_t pgoff, const std::string& filename);
  const MapSet& GetKernelMaps() { return kernel_maps_; }
  void AddThreadMap(int pid, int tid, uint64_t start_addr, uint64_t len, uint64_t pgoff,
                    const std::string& filename, uint32_t flags = 0);

  // Add process symbols that do not correspond to any real dso.
  // For example, these might be symbols generated by a JIT.
  void AddSymbolsForProcess(int pid, std::vector<Symbol>* symbols);

  const MapEntry* FindMap(const ThreadEntry* thread, uint64_t ip, bool in_kernel);
  // Find map for an ip address when we don't know whether it is in kernel.
  const MapEntry* FindMap(const ThreadEntry* thread, uint64_t ip);
  const Symbol* FindSymbol(const MapEntry* map, uint64_t ip, uint64_t* pvaddr_in_file,
                           Dso** pdso = nullptr);
  const Symbol* FindKernelSymbol(uint64_t ip);
  bool IsUnknownDso(const Dso* dso) const { return dso == unknown_dso_.get(); }
  const Symbol* UnknownSymbol() const { return &unknown_symbol_; }

  void ShowIpForUnknownSymbol() { show_ip_for_unknown_symbol_ = true; }
  void ShowMarkForUnknownSymbol() {
    show_mark_for_unknown_symbol_ = true;
    unknown_symbol_ = Symbol("*unknown", 0, ULLONG_MAX);
  }
  // Clear thread and map information, but keep loaded dso information. It saves
  // the time to reload dso information.
  void ClearThreadAndMap();

  void AddDsoInfo(const std::string& file_path, uint32_t file_type, uint64_t min_vaddr,
                  uint64_t file_offset_of_min_vaddr, std::vector<Symbol>* symbols,
                  const std::vector<uint64_t>& dex_file_offsets);
  void AddDexFileOffset(const std::string& file_path, uint64_t dex_file_offset);

  // Update thread tree with information provided by record.
  void Update(const Record& record);

  std::vector<Dso*> GetAllDsos() const;

 private:
  ThreadEntry* CreateThread(int pid, int tid);
  Dso* FindKernelDsoOrNew(const std::string& filename);
  Dso* FindUserDsoOrNew(const std::string& filename, uint64_t start_addr = 0,
                        DsoType dso_type = DSO_ELF_FILE);
  const MapEntry* AllocateMap(const MapEntry& entry);
  void InsertMap(MapSet& maps, const MapEntry& entry);

  // Add thread maps to cover symbols in dso.
  void AddThreadMapsForDsoSymbols(ThreadEntry* thread, Dso* dso);

  std::unordered_map<int, std::unique_ptr<ThreadEntry>> thread_tree_;
  std::vector<std::unique_ptr<std::string>> thread_comm_storage_;

  MapSet kernel_maps_;
  std::vector<std::unique_ptr<MapEntry>> map_storage_;
  MapEntry unknown_map_;

  std::unique_ptr<Dso> kernel_dso_;
  std::unordered_map<std::string, std::unique_ptr<Dso>> module_dso_tree_;
  std::unordered_map<std::string, std::unique_ptr<Dso>> user_dso_tree_;
  std::unique_ptr<Dso> unknown_dso_;
  bool show_ip_for_unknown_symbol_;
  bool show_mark_for_unknown_symbol_;
  Symbol unknown_symbol_;
};

}  // namespace simpleperf

using MapEntry = simpleperf::MapEntry;
using ThreadEntry = simpleperf::ThreadEntry;
using ThreadTree = simpleperf::ThreadTree;

#endif  // SIMPLE_PERF_THREAD_TREE_H_
