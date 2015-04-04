/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "environment.h"

#include <ctype.h>
#include <dirent.h>
#include <elf.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <unordered_map>

#include "read_elf.h"

std::vector<int> GetOnlineCpus() {
  std::vector<int> result;
  FILE* fp = fopen("/sys/devices/system/cpu/online", "r");
  if (fp == nullptr) {
    return result;
  }

  char* buf = nullptr;
  size_t n;
  if (getline(&buf, &n, fp) != -1) {
    bool have_dash = false;
    bool have_cpu = false;
    int cpu = 0;
    for (char* p = buf; *p != '\0'; ++p) {
      if (isdigit(*p)) {
        if (!have_cpu) {
          cpu = *p - '0';
          have_cpu = true;
        } else {
          cpu = cpu * 10 + *p - '0';
        }
      } else {
        if (have_cpu) {
          if (have_dash) {
            for (int t = result.back() + 1; t < cpu; ++t) {
              result.push_back(t);
            }
            have_dash = false;
          }
          result.push_back(cpu);
          have_cpu = false;
        }
        if (*p == '-') {
          have_dash = true;
        } else {
          have_dash = false;
        }
      }
    }
    if (have_cpu) {
      if (have_dash) {
        for (int t = result.back() + 1; t < cpu; ++t) {
          result.push_back(t);
        }
      }
      result.push_back(cpu);
    }
  }
  free(buf);
  fclose(fp);
  return result;
}

struct Symbol {
  uint64_t addr;
  char type;
  const char* name;
  const char* module;  // If nullptr, the symbol is not in a module.
};

static bool ProcessKernelSymbols(bool (*callback)(const Symbol& symbol, void* arg), void* callback_arg) {
  FILE* fp = fopen("/proc/kallsyms", "r");
  if (fp == nullptr) {
    return false;
  }
  Symbol symbol;
  char buf[1024];
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    if (!isxdigit(toupper(buf[0]))) {
      continue;
    }
    char* p;
    symbol.addr = strtoul(buf, &p, 16);
    while (isspace(*p)) {
      ++p;
    }
    if (*p == '\0' || *(p + 1) == '\0') {
      continue;
    }

    symbol.type = *p++;
    while (isspace(*p)) {
      ++p;
    }
    if (*p == '\0') {
      continue;
    }

    symbol.name = p;
    while (!isspace(*p) && *p != '\0') {
      ++p;
    }
    symbol.module = nullptr;
    if (*p != '\0') {
      *p++ = '\0';
      while (*p != '[' && *p != '\0') {
        ++p;
      }
      if (*p == '[' && *(p + 1) != '\0') {
        symbol.module = p + 1;
        p += 2;
        while (*p != ']' && *p != '\0') {
          ++p;
        }
        if (*p == ']') {
          *p = '\0';
        } else {
          fprintf(stderr, "/proc/kallsyms line too long in addr 0x%" PRIx64 "\n", symbol.addr);
          continue;
        }
      }
    }

    if (callback(symbol, callback_arg)) {
      fclose(fp);
      return true;
    }
  }
  fclose(fp);
  return false;
}

struct FindSymbolArg {
  const char* symbol_name;
  uint64_t symbol_addr;
};

static bool FindFunctionSymbolCallback(const Symbol& symbol, FindSymbolArg* arg) {
  if ((symbol.type == 'T' || symbol.type  == 'W' || symbol.type == 'A') &&
      strcmp(symbol.name, arg->symbol_name) == 0 && symbol.module == nullptr) {
    arg->symbol_addr = symbol.addr;
    return true;
  }
  return false;
}

static bool FindFunctionSymbolAddr(const char* symbol, uint64_t& symbol_addr) {
  FindSymbolArg arg;
  arg.symbol_name = symbol;
  if (ProcessKernelSymbols(reinterpret_cast<bool (*)(const Symbol&, void*)>(
                             FindFunctionSymbolCallback), &arg)) {
    symbol_addr = arg.symbol_addr;
    return true;
  }
  return false;
}

static bool FindStartOfKernelSymbolCallback(const Symbol& symbol, uint64_t* arg) {
  if (symbol.module == nullptr) {
    *arg = symbol.addr;
    return true;
  }
  return false;
}

static bool FindStartOfKernelSymbol(uint64_t& symbol_addr) {
  if (ProcessKernelSymbols(reinterpret_cast<bool (*)(const Symbol&, void*)>(
                             FindStartOfKernelSymbolCallback), &symbol_addr)) {
    return true;
  }
  return false;
}

static bool ProcessModules(bool (*callback)(uint64_t module_addr, const char* module, void*arg),
                           void* callback_arg) {
  FILE* fp = fopen("/proc/modules", "r");
  if (fp == nullptr) {
    return false;
  }
  bool result = true;
  char buf[1024];
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    char* p = buf;
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
  fclose(fp);
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
  if (!ProcessModules(reinterpret_cast<bool (*)(uint64_t, const char*, void*)>(EnumModuleCallback),
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
  bool result = false;
  char buf[1024];
  if (fgets(buf, sizeof(buf), fp) != nullptr) {
    if (strncmp(buf, "Linux version ", strlen("Linux version ")) == 0) {
      char* str = buf + strlen("Linux version ");
      char* p = str;
      while (!isspace(*p) && *p != '\0') {
        ++p;
      }
      *p = '\0';
      version = str;
      result = true;
    }
  }
  fclose(fp);
  return result;
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

bool GetMmapInfo(KernelMmap& kernel_mmap,
                              std::vector<ModuleMmap>& module_mmaps) {
  if (!FindStartOfKernelSymbol(kernel_mmap.start_addr)) {
    return false;
  }
  if (!FindFunctionSymbolAddr("_text", kernel_mmap.pgoff)) {
    return false;
  }
  kernel_mmap.name = "[kernel.kallsyms]_text";
  if (!EnumAllModules(module_mmaps)) {
    // There is no /proc/modules or /lib/modules on android devices, so it's normal to fail.
    module_mmaps.clear();
  }
  std::sort(module_mmaps.begin(), module_mmaps.end(), CompareModuleMmap);

  if (module_mmaps.size() == 0) {
    kernel_mmap.len = ULLONG_MAX - kernel_mmap.start_addr;
  } else {
    if (kernel_mmap.start_addr > module_mmaps[0].start_addr) {
      fprintf(stderr, "/proc/kallsyms out of expectation.\n");
      fprintf(stderr, "kernel_mmap.start_addr = 0x%" PRIx64 "\n", kernel_mmap.start_addr);
      for (auto& module_mmap : module_mmaps) {
        fprintf(stderr, "module_mmap %s 0x%" PRIx64 "\n", module_mmap.name.c_str(), module_mmap.start_addr);
      }
      return false;
    }

    // When not having enough privilege, all addrs are read as 0.
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
  char buf[1024];
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    if (!strncmp(buf, "Name:", 5)) {
      char* start = buf + 5;
      while (isspace(*start)) {
        ++start;
      }
      char* end = start;
      while (!isspace(*end) && *end != '\0') {
        ++end;
      }
      *end = '\0';
      comm = std::string(start);
      read_comm = true;
    } else if (!strncmp(buf, "Tgid:", 5)) {
      char* start = buf + 5;
      while (isspace(*start)) {
        ++start;
      }
      char* end = start;
      while (!isspace(*end) && *end != '\0') {
        ++end;
      }
      *end = '\0';
      tgid = static_cast<pid_t>(strtol(start, &end, 10));
      if (*end == '\0') {
        read_tgid = true;
      }
    }
    if (read_comm && read_tgid) {
      break;
    }
  }
  fclose(fp);
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

static bool CommitAddressInBuffer(char* buf, char** new_buf, uint64_t& addr) {
  addr = strtoul(buf, new_buf, 16);
  if (*new_buf != buf) {
    return true;
  }
  return false;
}

static bool CommitCharacterInBuffer(char* buf, char** new_buf, char c) {
  if (*buf == c) {
    *new_buf = buf + 1;
    return true;
  }
  return false;
}

static bool CommitStringInBuffer(char* buf, char** new_buf, const char** pstr) {
  while (isspace(*buf)) {
    ++buf;
  }
  if (*buf == '\0') {
    return false;
  }
  if (pstr != nullptr) {
    *pstr = buf;
  }
  while (!isspace(*buf) && *buf != '\0') {
    ++buf;
  }
  if (*buf == '\0') {
    *new_buf = buf;
  } else {
    *buf = '\0';
    *new_buf = buf + 1;
  }
  return true;
}

bool GetProcessMmaps(pid_t pid, std::vector<ThreadMmap>& thread_mmaps) {
  std::string mmap_file = "/proc/" + std::to_string(pid) + "/maps";
  FILE* fp = fopen(mmap_file.c_str(), "r");
  if (fp == nullptr) {
    return false;
  }
  thread_mmaps.clear();
  char buf[8192];
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    // Parse line like this: 00400000-00409000 r-xp 00000000 fc:00 426998  /usr/lib/gvfs/gvfsd-http
    char* p = buf;
    uint64_t start_addr;
    if (!CommitAddressInBuffer(p, &p, start_addr)) {
      continue;
    }
    if (!CommitCharacterInBuffer(p, &p, '-')) {
      continue;
    }
    uint64_t end_addr;
    if (!CommitAddressInBuffer(p, &p, end_addr)) {
      continue;
    }

    const char* prot;
    if (!CommitStringInBuffer(p, &p, &prot)) {
      continue;
    }

    uint64_t pgoff;
    if (!CommitAddressInBuffer(p, &p, pgoff)) {
      continue;
    }

    if (!CommitStringInBuffer(p, &p, nullptr) || !CommitStringInBuffer(p, &p, nullptr)) {
      continue;
    }

    const char* execname = DEFAULT_EXEC_NAME_FOR_THREAD_MMAP;
    CommitStringInBuffer(p, &p, &execname);

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
  fclose(fp);
  return true;
}

bool GetKernelBuildId(BuildId& build_id) {
  return GetBuildIdFromNoteFile("/sys/kernel/notes", build_id);
}

bool GetModuleBuildId(const char* module_name, BuildId& build_id) {
  std::string notefile = "/sys/module/" + std::string(module_name) + "/notes/.note.gnu.build-id";
  return GetBuildIdFromNoteFile(notefile.c_str(), build_id);
}
