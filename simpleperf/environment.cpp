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

#include "environment.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>

#include "utils.h"

std::vector<int> GetOnlineCpus() {
  std::vector<int> result;
  FILE* fp = fopen("/sys/devices/system/cpu/online", "re");
  if (fp == nullptr) {
    PLOG(ERROR) << "can't open online cpu information";
    return result;
  }

  LineReader reader(fp);
  char* line;
  if ((line = reader.ReadLine()) != nullptr) {
    result = GetOnlineCpusFromString(line);
  }
  CHECK(!result.empty()) << "can't get online cpu information";
  return result;
}

std::vector<int> GetOnlineCpusFromString(const std::string& s) {
  std::vector<int> result;
  bool have_dash = false;
  const char* p = s.c_str();
  char* endp;
  long cpu;
  // Parse line like: 0,1-3, 5, 7-8
  while ((cpu = strtol(p, &endp, 10)) != 0 || endp != p) {
    if (have_dash && result.size() > 0) {
      for (int t = result.back() + 1; t < cpu; ++t) {
        result.push_back(t);
      }
    }
    have_dash = false;
    result.push_back(cpu);
    p = endp;
    while (!isdigit(*p) && *p != '\0') {
      if (*p == '-') {
        have_dash = true;
      }
      ++p;
    }
  }
  return result;
}

bool ProcessKernelSymbols(const std::string& symbol_file,
                          std::function<bool(const KernelSymbol&)> callback) {
  FILE* fp = fopen(symbol_file.c_str(), "re");
  if (fp == nullptr) {
    PLOG(DEBUG) << "failed to open file " << symbol_file;
    return false;
  }
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    // Parse line like: ffffffffa005c4e4 d __warned.41698       [libsas]
    char type[reader.MaxLineSize()];
    char name[reader.MaxLineSize()];
    char module[reader.MaxLineSize()];
    strcpy(module, "");

    KernelSymbol symbol;
    if (sscanf(line, "%" PRIx64 "%s%s%s", &symbol.addr, type, name, module) < 3) {
      continue;
    }
    symbol.type = type[0];
    symbol.name = name;
    size_t module_len = strlen(module);
    if (module_len > 2 && module[0] == '[' && module[module_len - 1] == ']') {
      module[module_len - 1] = '\0';
      symbol.module = &module[1];
    } else {
      symbol.module = nullptr;
    }

    if (callback(symbol)) {
      return true;
    }
  }
  return false;
}

static bool FindStartOfKernelSymbolCallback(const KernelSymbol& symbol, uint64_t* start_addr) {
  if (symbol.module == nullptr) {
    *start_addr = symbol.addr;
    return true;
  }
  return false;
}

static bool FindStartOfKernelSymbol(const std::string& symbol_file, uint64_t* start_addr) {
  return ProcessKernelSymbols(
      symbol_file, std::bind(&FindStartOfKernelSymbolCallback, std::placeholders::_1, start_addr));
}

static bool FindKernelFunctionSymbolCallback(const KernelSymbol& symbol, const std::string& name,
                                             uint64_t* addr) {
  if ((symbol.type == 'T' || symbol.type == 'W' || symbol.type == 'A') &&
      symbol.module == nullptr && name == symbol.name) {
    *addr = symbol.addr;
    return true;
  }
  return false;
}

static bool FindKernelFunctionSymbol(const std::string& symbol_file, const std::string& name,
                                     uint64_t* addr) {
  return ProcessKernelSymbols(
      symbol_file, std::bind(&FindKernelFunctionSymbolCallback, std::placeholders::_1, name, addr));
}

bool ProcessModules(const std::string& modules_file,
                    std::function<void(uint64_t, const char*)> callback) {
  FILE* fp = fopen(modules_file.c_str(), "re");
  if (fp == nullptr) {
    PLOG(DEBUG) << "failed to open file " << modules_file;
    return false;
  }
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    // Parse line like: nf_defrag_ipv6 34768 1 nf_conntrack_ipv6, Live 0xffffffffa0fe5000
    char name[reader.MaxLineSize()];
    if (sscanf(line, "%s", name) != 1) {
      continue;
    }
    uint64_t module_addr = 0;
    for (char* p = line + strlen(name); *p != '\0'; ++p) {
      if (*p == '0' && *(p + 1) == 'x') {
        module_addr = strtoull(p + 2, nullptr, 16);
        break;
      }
    }
    if (module_addr != 0) {
      callback(module_addr, name);
    }
  }
  return true;
}

static void GetModulesInUseCallback(uint64_t module_addr, const char* module_name,
                                    std::vector<ModuleMmap>* module_mmaps) {
  ModuleMmap map;
  map.name = module_name;
  map.start_addr = module_addr;
  module_mmaps->push_back(map);
}

static bool GetLinuxVersion(std::string* version) {
  FILE* fp = fopen("/proc/version", "re");
  if (fp == nullptr) {
    PLOG(DEBUG) << "failed to open file /proc/version";
    return false;
  }
  LineReader reader(fp);
  char* line;
  if ((line = reader.ReadLine()) != nullptr) {
    char s[reader.MaxLineSize()];
    if (sscanf(line, "Linux version %s", s) == 1) {
      *version = s;
      return true;
    }
  }
  return false;
}

static void GetAllModuleFiles(std::string path,
                              std::unordered_map<std::string, std::string>* module_file_map) {
  if (path.back() != '/') {
    path += "/";
  }
  for (auto& name : GetEntriesInDir(path)) {
    if (name.back() == '/') {  // Is a directory.
      GetAllModuleFiles(path + name, module_file_map);
    } else {
      if (name.size() > 3 && name.rfind(".ko") == name.size() - 3) {
        std::string module_name = name.substr(0, name.size() - 3);
        std::replace(module_name.begin(), module_name.end(), '-', '_');
        module_file_map->insert(std::make_pair(module_name, path + name));
      }
    }
  }
}

static bool GetModulesInUse(std::vector<ModuleMmap>* module_mmaps) {
  module_mmaps->clear();
  if (!ProcessModules("/proc/modules", std::bind(&GetModulesInUseCallback, std::placeholders::_1,
                                                 std::placeholders::_2, module_mmaps))) {
    return false;
  }
  std::string linux_version;
  if (!GetLinuxVersion(&linux_version)) {
    LOG(DEBUG) << "GetLinuxVersion failed";
    return false;
  }
  std::string module_dirpath = "/lib/modules/" + linux_version + "/kernel";
  std::unordered_map<std::string, std::string> module_file_map;
  GetAllModuleFiles(module_dirpath, &module_file_map);
  for (auto& module : *module_mmaps) {
    auto it = module_file_map.find(module.name);
    if (it != module_file_map.end()) {
      module.filepath = it->second;
    }
  }
  return true;
}

bool GetKernelAndModuleMmaps(KernelMmap* kernel_mmap, std::vector<ModuleMmap>* module_mmaps) {
  if (!FindStartOfKernelSymbol("/proc/kallsyms", &kernel_mmap->start_addr)) {
    LOG(DEBUG) << "call FindStartOfKernelSymbol() failed";
    return false;
  }
  if (!FindKernelFunctionSymbol("/proc/kallsyms", "_text", &kernel_mmap->pgoff)) {
    LOG(DEBUG) << "call FindKernelFunctionSymbol() failed";
    return false;
  }
  kernel_mmap->name = DEFAULT_KERNEL_MMAP_NAME;
  if (!GetModulesInUse(module_mmaps)) {
    // There is no /proc/modules or /lib/modules on android devices, so it's acceptable to fail
    // here.
    module_mmaps->clear();
  }
  if (module_mmaps->size() == 0) {
    kernel_mmap->len = ULLONG_MAX - kernel_mmap->start_addr;
  } else {
    std::sort(
        module_mmaps->begin(), module_mmaps->end(),
        [](const ModuleMmap& m1, const ModuleMmap& m2) { return m1.start_addr < m2.start_addr; });
    CHECK_LE(kernel_mmap->start_addr, (*module_mmaps)[0].start_addr);
    // When not having enough privilege, all addresses are read as 0.
    if (kernel_mmap->start_addr == (*module_mmaps)[0].start_addr) {
      kernel_mmap->len = 0;
    } else {
      kernel_mmap->len = (*module_mmaps)[0].start_addr - kernel_mmap->start_addr - 1;
    }
    for (size_t i = 0; i + 1 < module_mmaps->size(); ++i) {
      if ((*module_mmaps)[i].start_addr == (*module_mmaps)[i + 1].start_addr) {
        (*module_mmaps)[i].len = 0;
      } else {
        (*module_mmaps)[i].len =
            (*module_mmaps)[i + 1].start_addr - (*module_mmaps)[i].start_addr - 1;
      }
    }
    module_mmaps->back().len = ULLONG_MAX - module_mmaps->back().start_addr;
  }
  return true;
}

static bool StringToPid(const std::string& s, pid_t* pid) {
  char* endptr;
  *pid = static_cast<pid_t>(strtol(s.c_str(), &endptr, 10));
  return *endptr == '\0';
}

static bool ReadThreadNameAndTgid(const std::string& status_file, std::string* comm, pid_t* tgid) {
  FILE* fp = fopen(status_file.c_str(), "re");
  if (fp == nullptr) {
    return false;
  }
  bool read_comm = false;
  bool read_tgid = false;
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    char s[reader.MaxLineSize()];
    if (sscanf(line, "Name:%s", s) == 1) {
      *comm = s;
      read_comm = true;
    } else if (sscanf(line, "Tgid:%d", tgid) == 1) {
      read_tgid = true;
    }
    if (read_comm && read_tgid) {
      return true;
    }
  }
  return false;
}

static bool GetThreadComm(pid_t pid, std::vector<ThreadComm>* thread_comms) {
  std::string task_dirname = android::base::StringPrintf("/proc/%d/task", pid);
  for (auto& name : GetEntriesInDir(task_dirname)) {
    if (name.back() != '/') {
      continue;
    }
    pid_t tid;
    if (!StringToPid(name.substr(0, name.size() - 1), &tid)) {
      continue;
    }
    std::string status_file = android::base::StringPrintf("%s/%d/status", task_dirname.c_str(), tid);
    std::string comm;
    pid_t tgid;
    if (!ReadThreadNameAndTgid(status_file, &comm, &tgid)) {
      return false;
    }
    ThreadComm thread;
    thread.tid = tid;
    thread.tgid = tgid;
    thread.comm = comm;
    thread.is_process = (tid == tgid);
    thread_comms->push_back(thread);
  }
  return true;
}

bool GetThreadComms(std::vector<ThreadComm>* thread_comms) {
  thread_comms->clear();
  for (auto& name : GetEntriesInDir("/proc")) {
    if (name.back() != '/') {
      continue;
    }
    pid_t pid;
    if (!StringToPid(name.substr(0, name.size() - 1), &pid)) {
      continue;
    }
    if (!GetThreadComm(pid, thread_comms)) {
      return false;
    }
  }
  return true;
}

bool GetThreadMmapsInProcess(pid_t pid, std::vector<ThreadMmap>* thread_mmaps) {
  std::string map_file = android::base::StringPrintf("/proc/%d/maps", pid);
  FILE* fp = fopen(map_file.c_str(), "re");
  if (fp == nullptr) {
    PLOG(DEBUG) << "can't open file " << map_file;
    return false;
  }
  thread_mmaps->clear();
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    // Parse line like: 00400000-00409000 r-xp 00000000 fc:00 426998  /usr/lib/gvfs/gvfsd-http
    uint64_t start_addr, end_addr, pgoff;
    char type[reader.MaxLineSize()];
    char execname[reader.MaxLineSize()];
    strcpy(execname, "");
    if (sscanf(line, "%" PRIx64 "-%" PRIx64 " %s %" PRIx64 " %*x:%*x %" PRIu64 " %s\n", &start_addr,
               &end_addr, type, &pgoff, &pgoff, execname) < 4) {
      continue;
    }
    if (strcmp(execname, "") == 0) {
      strcpy(execname, DEFAULT_EXECNAME_FOR_THREAD_MMAP);
    }
    ThreadMmap thread;
    thread.start_addr = start_addr;
    thread.len = end_addr - start_addr;
    thread.pgoff = pgoff;
    thread.name = execname;
    thread.readable = (type[0] == 'r');
    thread.writable = (type[1] == 'w');
    thread.executable = (type[2] == 'x');
    thread_mmaps->push_back(thread);
  }
  return true;
}
