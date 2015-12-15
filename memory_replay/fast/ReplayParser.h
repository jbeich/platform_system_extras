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

#ifndef REPLAY_PARSER_H
#define REPLAY_PARSER_H

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "MemoryReplay.h"
#include "memreplay_fast.h"

struct Allocation {
  AllocationId id;
  ThreadId owning_thread;
};

class ReplayParser {
  std::unordered_map<std::string, ThreadId> thread_map;
  std::unordered_set<ThreadId> active_threads;
  ThreadId thread_index = 0;

  std::unordered_map<ThreadId, std::vector<command>> commands;

  std::unordered_map<std::string, Allocation> active_allocations;
  AllocationId allocation_count = 0;

  std::unordered_map<std::pair<ThreadId, ThreadId>, LocalSyncPoint> local_sync_points;
  std::vector<uint32_t> global_sync_points;

  AllocationId NewAllocation(const char* address, ThreadId thread);
  Allocation GetAllocation(const char* address);
  void EmitLocalSync(ThreadId thread_a, ThreadId thread_b);
  GlobalSyncPoint CreateGlobalSync();
  ThreadId GetThread(const std::string& thread_name);
  void EmitDump(uint32_t line_number);
  void HandleLine(char* line, uint32_t line_number);
  MemoryReplay ParseReplay(FILE* input, uint32_t dump_interval);

 public:
  static MemoryReplay Parse(int fd, uint32_t dump_interval = 100000);
};

#endif
