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

#ifndef MEMREPLAY_FAST_H
#define MEMREPLAY_FAST_H

#include <stdint.h>

#include <unordered_map>
#include <utility>

typedef uint32_t AllocationId;
typedef uint16_t ThreadId;

typedef uint32_t LocalSyncPoint;
typedef uint32_t GlobalSyncPoint;

namespace std {
template <>
struct hash<std::pair<ThreadId, ThreadId>> {
  typedef std::pair<ThreadId, ThreadId> argument_type;
  typedef std::size_t result_type;
  result_type operator()(const argument_type& x) const {
    // The two thread IDs are guaranteed to be different.
    std::hash<ThreadId> h;
    return h(x.first) ^ h(x.second);
  }
};
}

enum class command_type : uint8_t {
  invalid = 0,

  // arg1 = malloc(arg2)
  malloc,

  // arg1 = calloc(arg2, arg3)
  calloc,

  // arg1 = memalign(arg2, arg3)
  memalign,

  // NOTE: The index for realloc does not change.
  // arg1 = realloc(arg1, arg2)
  realloc,

  // free(arg1)
  free,

  // arg1 = local sync point
  local_sync,

  // arg1 = global sync point, arg2 = line number
  dump,

  // arg1 = global sync point, arg2 = thread id
  thread_start,

  // arg1 = global sync point, arg2 = thread id
  thread_exit,
};

#pragma pack(push, 1)
struct command {
  command_type type = command_type::invalid;
  uint32_t arg1 = 0;
  uint32_t arg2 = 0;
  uint32_t arg3 = 0;
};

struct file_header {
  char magic[8];
  AllocationId allocation_count;
  ThreadId thread_count;
  ThreadId final_thread_count;
  uint32_t local_sync_count;
  uint32_t global_sync_count;
};

struct thread_header {
  ThreadId thread_id;
  uint32_t command_count;
};
#pragma pack(pop)

#endif
