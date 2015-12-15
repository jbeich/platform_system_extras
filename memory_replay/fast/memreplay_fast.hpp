/*
 * Copyright (C) 2014 The Android Open Source Project
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

enum class command_type : uint8_t {
  // arg1 = malloc(arg2)
  malloc = 0,

  // arg1 = calloc(arg2, arg3)
  calloc = 1,

  // arg1 = memalign(arg2, arg3)
  memalign = 2,

  // NOTE: The index for realloc does not change.
  // arg1 = realloc(arg1, arg2)
  realloc = 3,

  // free(arg1)
  free = 4,

  // arg1 = local sync point
  local_sync = 5,

  // arg1 = global sync point, arg2 = line number
  dump = 6,

  // arg1 = global sync point, arg2 = thread id
  thread_start = 7,

  // arg1 = global sync point, arg2 = thread id
  thread_exit = 8,
};

typedef uint32_t AllocationId;
typedef uint16_t ThreadId;

#pragma pack(push, 1)
struct command {
  command_type type;
  uint32_t arg1;
  uint32_t arg2;
  uint32_t arg3;
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
