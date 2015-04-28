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

#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#include <base/logging.h>

#include "read_elf.h"
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
                          bool (*callback)(const KernelSymbol& symbol, void* arg),
                          void* callback_arg) {
  FILE* fp = fopen(symbol_file.c_str(), "re");
  if (fp == nullptr) {
    return false;
  }
  KernelSymbol symbol;
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    // Parse line like: ffffffffa005c4e4 d __warned.41698       [libsas]
    char type[reader.MaxLineSize()];
    char name[reader.MaxLineSize()];
    char module[reader.MaxLineSize()];

    strcpy(module, "");
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

    if (callback(symbol, callback_arg)) {
      return true;
    }
  }
  return false;
}

static bool FindStartOfKernelSymbolCallback(const KernelSymbol& symbol, uint64_t* arg) {
  if (symbol.module == nullptr) {
    *arg = symbol.addr;
    return true;
  }
  return false;
}

static bool FindStartOfKernelSymbol(const char* symbol_file, uint64_t* start_addr) {
  return ProcessKernelSymbols(symbol_file, reinterpret_cast<bool (*)(const KernelSymbol&, void*)>(
                                               FindStartOfKernelSymbolCallback),
                              start_addr);
}

struct FindKernelSymbolArg {
  const char* symbol_name;
  uint64_t symbol_addr;
};

static bool FindFunctionKernelSymbolAddrCallback(const KernelSymbol& symbol,
                                                 FindKernelSymbolArg* arg) {
  if ((symbol.type == 'T' || symbol.type == 'W' || symbol.type == 'A') &&
      strcmp(symbol.name, arg->symbol_name) == 0 && symbol.module == nullptr) {
    arg->symbol_addr = symbol.addr;
    return true;
  }
  return false;
}

static bool FindFunctionSymbolAddr(const char* symbol_file, const char* symbol,
                                   uint64_t* symbol_addr) {
  FindKernelSymbolArg arg;
  arg.symbol_name = symbol;
  if (ProcessKernelSymbols(symbol_file, reinterpret_cast<bool (*)(const KernelSymbol&, void*)>(
                                            FindFunctionKernelSymbolAddrCallback),
                           &arg)) {
    *symbol_addr = arg.symbol_addr;
    return true;
  }
  return false;
}

bool ProcessModules(const std::string& modules_file,
                    bool (*callback)(uint64_t module_addr, const char* module, void* arg),
                    void* callback_arg) {
  FILE* fp = fopen(modules_file.c_str(), "r");
  if (fp == nullptr) {
    return false;
  }
  bool result = false;
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    // Parse line like: nf_defrag_ipv6 34768 1 nf_conntrack_ipv6, Live 0xffffffffa0fe5000
    char* p = line;
    char* module = p;
    while (!isspace(*p)) {
      ++p;
    }
    if (*p == '\0') {
      continue;
    }
    *p++ = '\0';
    uint64_t module_addr = ULLONG_MAX;
    while (*p != '\0') {
      while (isspace(*p)) {
        ++p;
      }
      if (*p == '0' && *(p + 1) == 'x') {
        module_addr = strtoul(p + 2, nullptr, 16);
        break;
      }
      while (!isspace(*p) && *p != '\0') {
        ++p;
      }
    }
    if (module_addr != ULLONG_MAX) {
      if (callback(module_addr, module, callback_arg)) {
        result = true;
      }
    }
  }
  return result;
}

struct EnumModuleArg {
  std::vector<ModuleMmap>* modules;
};

static bool EnumModuleCallback(uint64_t module_addr, const char* module, EnumModuleArg* arg) {
  ModuleMmap module_mmap;
  module_mmap.name = module;
  module_mmap.start_addr = module_addr;
  arg->modules->push_back(module_mmap);
  return true;
}

static bool GetLinuxVersion(std::string& version);
static bool EnumModuleFiles(const char* module_dir,
                            std::unordered_map<std::string, std::string>& module_file_map);

static bool EnumAllModules(std::vector<ModuleMmap>& modules) {
  modules.clear();
  EnumModuleArg arg;
  arg.modules = &modules;
  if (!ProcessModules("/proc/modules",
                      reinterpret_cast<bool (*)(uint64_t, const char*, void*)>(EnumModuleCallback),
                      &arg)) {
    return false;
  }
  std::unordered_map<std::string, std::string> module_file_map;
  std::string linux_version;
  if (!GetLinuxVersion(linux_version)) {
    return false;
  }
  std::string module_dir = "/lib/modules/" + linux_version + "/kernel";
  if (!EnumModuleFiles(module_dir.c_str(), module_file_map)) {
    return false;
  }

  for (auto& module : modules) {
    auto it = module_file_map.find(module.name);
    if (it != module_file_map.end()) {
      module.filepath = it->second;
    }
  }
  return true;
}

static bool GetLinuxVersion(std::string& version) {
  FILE* fp = fopen("/proc/version", "r");
  if (fp == nullptr) {
    return false;
  }
  LineReader reader(fp);
  char* line;
  if ((line = reader.ReadLine()) != nullptr) {
    if (strncmp(line, "Linux version", strlen("Linux version")) == 0) {
      char s[reader.MaxLineSize()];
      if (sscanf(line, "Linux version %s", s) == 1) {
        version = s;
        return true;
      }
    }
  }
  return false;
}

static bool EnumModuleFiles(const char* module_dir,
                            std::unordered_map<std::string, std::string>& module_file_map) {
  DIR* dir = opendir(module_dir);
  if (dir != nullptr) {
    dirent entry, *entry_p;
    while (readdir_r(dir, &entry, &entry_p) == 0 && entry_p != nullptr) {
      if (strcmp(entry_p->d_name, ".") == 0 || strcmp(entry_p->d_name, "..") == 0) {
        continue;
      }
      std::string path = module_dir + std::string("/") + entry_p->d_name;
      struct stat st;
      if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
          EnumModuleFiles(path.c_str(), module_file_map);
        } else {
          size_t len = strlen(entry_p->d_name);
          if (len > 3 && strcmp(&entry_p->d_name[len - 3], ".ko") == 0) {
            std::string module_name(entry_p->d_name, len - 3);
            std::replace(module_name.begin(), module_name.end(), '-', '_');
            module_file_map[module_name] = path;
          }
        }
      }
    }
    closedir(dir);
  }
  return true;
}

static bool CompareModuleMmap(const ModuleMmap& module1, const ModuleMmap& module2) {
  return module1.start_addr < module2.start_addr;
}

bool GetKernelMmaps(KernelMmap& kernel_mmap, std::vector<ModuleMmap>& module_mmaps) {
  if (!FindStartOfKernelSymbol("/proc/kallsyms", &kernel_mmap.start_addr)) {
    LOG(ERROR) << "failed to call FindStartOfKernelSymbol()";
    return false;
  }
  if (!FindFunctionSymbolAddr("/proc/kallsyms", "_text", &kernel_mmap.pgoff)) {
    LOG(ERROR) << "failed to call FindFunctionSymbolAddr()";
    return false;
  }
  kernel_mmap.name = DEFAULT_KERNEL_MMAP_NAME;
  if (!EnumAllModules(module_mmaps)) {
    // There is no /proc/modules or /lib/modules on android devices, so it's acceptable to fail
    // here.
    module_mmaps.clear();
  }
  std::sort(module_mmaps.begin(), module_mmaps.end(), CompareModuleMmap);
  if (module_mmaps.size() == 0) {
    kernel_mmap.len = ULLONG_MAX - kernel_mmap.start_addr;
  } else {
    CHECK_LE(kernel_mmap.start_addr, module_mmaps[0].start_addr);

    // When not having enough privilege, all addresses are read as 0.
    if (kernel_mmap.start_addr == module_mmaps[0].start_addr) {
      kernel_mmap.len = 0;
    } else {
      kernel_mmap.len = module_mmaps[0].start_addr - kernel_mmap.start_addr - 1;
    }

    for (size_t i = 0; i + 1 < module_mmaps.size(); ++i) {
      if (module_mmaps[i].start_addr == module_mmaps[i + 1].start_addr) {
        module_mmaps[i].len = 0;
      } else {
        module_mmaps[i].len = module_mmaps[i + 1].start_addr - module_mmaps[i].start_addr - 1;
      }
    }
    module_mmaps.back().len = ULLONG_MAX - module_mmaps.back().start_addr;
  }
  return true;
}

static bool ReadThreadNameAndTgid(const char* status_file_path, std::string& comm, pid_t& tgid) {
  FILE* fp = fopen(status_file_path, "r");
  if (fp == nullptr) {
    return false;
  }
  bool read_comm = false;
  bool read_tgid = false;
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    if (!strncmp(line, "Name:", strlen("Name:"))) {
      char s[reader.MaxLineSize()];
      if (sscanf(line, "Name:%s", s) == 1) {
        comm = s;
        read_comm = true;
      }
    } else if (sscanf(line, "Tgid:%d", &tgid) == 1) {
      read_tgid = true;
    }
    if (read_comm && read_tgid) {
      break;
    }
  }
  return read_comm && read_tgid;
}

static bool GetThreadComm(pid_t pid, std::vector<ThreadComm>& thread_comms) {
  std::string pid_str = std::to_string(pid);
  std::string task_dirname = "/proc/" + pid_str + "/task";
  DIR* dir = opendir(task_dirname.c_str());
  if (dir == nullptr) {
    return false;
  }
  dirent entry, *entry_p;
  while (readdir_r(dir, &entry, &entry_p) == 0 && entry_p != nullptr) {
    char* endptr;
    pid_t tid = static_cast<pid_t>(strtol(entry_p->d_name, &endptr, 10));
    if (*endptr != '\0') {
      continue;
    }
    std::string tid_str = std::to_string(tid);
    std::string status_file = task_dirname + "/" + tid_str + "/status";
    std::string comm;
    pid_t tgid;
    if (!ReadThreadNameAndTgid(status_file.c_str(), comm, tgid)) {
      return false;
    }

    ThreadComm thread;
    thread.tid = tid;
    thread.tgid = tgid;
    thread.comm = comm;
    thread.is_process = (tid == pid);
    thread_comms.push_back(thread);
  }
  closedir(dir);
  return true;
}

bool GetThreadComms(std::vector<ThreadComm>& thread_comms) {
  thread_comms.clear();
  DIR* dir = opendir("/proc");
  if (dir == nullptr) {
    return false;
  }
  dirent entry, *entry_p;
  while (readdir_r(dir, &entry, &entry_p) == 0 && entry_p != nullptr) {
    char* endptr;
    pid_t pid = static_cast<pid_t>(strtol(entry_p->d_name, &endptr, 10));
    if (*endptr != '\0') {
      continue;
    }
    if (!GetThreadComm(pid, thread_comms)) {
      return false;
    }
  }
  closedir(dir);
  return true;
}

bool GetProcessMmaps(pid_t pid, std::vector<ThreadMmap>& thread_mmaps) {
  std::string mmap_file = "/proc/" + std::to_string(pid) + "/maps";
  FILE* fp = fopen(mmap_file.c_str(), "r");
  if (fp == nullptr) {
    return false;
  }
  thread_mmaps.clear();
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    // Parse line like: 00400000-00409000 r-xp 00000000 fc:00 426998  /usr/lib/gvfs/gvfsd-http
    uint64_t start_addr, end_addr, pgoff;
    char prot[5];
    char execname[reader.MaxLineSize()];

    strcpy(execname, "");

    if (sscanf(line, "%" PRIx64 "-%" PRIx64 " %s %" PRIx64 " %*x:%*x %*u %s\n", &start_addr,
               &end_addr, prot, &pgoff, execname) < 4) {
      continue;
    }
    if (strcmp(execname, "") == 0) {
      strcpy(execname, DEFAULT_EXEC_NAME_FOR_THREAD_MMAP);
    }

    ThreadMmap thread;
    thread.start_addr = start_addr;
    thread.len = end_addr - start_addr;
    thread.pgoff = pgoff;
    thread.name = execname;
    thread.readable = (prot[0] == 'r') ? 1 : 0;
    thread.writable = (prot[1] == 'w') ? 1 : 0;
    thread.executable = (prot[2] == 'x') ? 1 : 0;
    thread_mmaps.push_back(thread);
  }
  return true;
}

bool GetKernelBuildId(BuildId& build_id) {
  return GetBuildIdFromNoteFile("/sys/kernel/notes", build_id);
}

bool GetModuleBuildId(const char* module_name, BuildId& build_id) {
  std::string notefile = "/sys/module/" + std::string(module_name) + "/notes/.note.gnu.build-id";
  return GetBuildIdFromNoteFile(notefile.c_str(), build_id);
}
