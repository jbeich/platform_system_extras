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
