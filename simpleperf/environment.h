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

#ifndef SIMPLE_PERF_ENVIRONMENT_H_
#define SIMPLE_PERF_ENVIRONMENT_H_

#include <stdint.h>
#include <sys/types.h>
#include <string>
#include <utility>
#include <vector>

#include "build_id.h"

std::vector<int> GetOnlineCpus();

int64_t NanoTime();

#define DEFAULT_KERNEL_MMAP_NAME "[kernel.kallsyms]_text"

struct KernelMmap {
  std::string name;
  uint64_t start_addr;
  uint64_t len;
  uint64_t pgoff;
};

struct ModuleMmap {
  std::string name;
  uint64_t start_addr;
  uint64_t len;
  std::string filepath;
};

bool GetMmapInfo(KernelMmap& kernel_mmap, std::vector<ModuleMmap>& module_mmaps);

struct ThreadComm {
  pid_t tid, tgid;
  std::string comm;
  bool is_process;
};

bool GetThreadComms(std::vector<ThreadComm>& thread_comms);

#define DEFAULT_EXEC_NAME_FOR_THREAD_MMAP "//anon"

struct ThreadMmap {
  uint64_t start_addr;
  uint64_t len;
  uint64_t pgoff;
  std::string name;
  unsigned char readable : 1;
  unsigned char writable : 1;
  unsigned char executable : 1;
};

bool GetProcessMmaps(pid_t pid, std::vector<ThreadMmap>& thread_mmaps);

#define DEFAULT_KERNEL_FILENAME_FOR_BUILD_ID "[kernel.kallsyms]"

bool GetKernelBuildId(BuildId& build_id);

bool GetModuleBuildId(const char* module_name, BuildId& build_id);

#endif  // SIMPLE_PERF_ENVIRONMENT_H_
