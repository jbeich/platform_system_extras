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

#ifndef MEMORYREPLAY_H
#define MEMORYREPLAY_H

#include <stdint.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "memreplay_fast.h"

class MemoryReplay {
  friend class ReplayParser;

  AllocationId allocation_count;
  ThreadId final_thread_count;
  std::unordered_map<ThreadId, std::vector<command>> commands;
  std::unordered_map<std::pair<ThreadId, ThreadId>, LocalSyncPoint> local_sync_points;
  std::vector<uint32_t> global_sync_points;

  MemoryReplay();

 public:
  MemoryReplay(MemoryReplay&& move);
  void WriteDump(int fd);
};

#endif
