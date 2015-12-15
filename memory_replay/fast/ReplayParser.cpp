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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <utility>

#include "MemoryReplay.h"
#include "ReplayParser.h"
#include "memreplay_fast.h"

AllocationId ReplayParser::NewAllocation(const char* address, ThreadId thread) {
  auto it = active_allocations.find(address);
  if (it != active_allocations.end()) {
    errx(1, "preexisting address returned by allocation");
  }

  AllocationId id = allocation_count++;
  active_allocations[address] = { id, thread };
  return id;
}

Allocation ReplayParser::ReplayParser::GetAllocation(const char* address) {
  auto it = active_allocations.find(address);
  if (it == active_allocations.end()) {
    errx(1, "failed to find allocation");
  }
  return it->second;
}

void ReplayParser::EmitLocalSync(ThreadId thread_a, ThreadId thread_b) {
  if (thread_a != thread_b) {
    auto key = std::make_pair(std::min(thread_a, thread_b), std::max(thread_a, thread_b));
    auto it = local_sync_points.find(key);
    if (it == local_sync_points.end()) {
      auto value = std::make_pair(key, static_cast<LocalSyncPoint>(local_sync_points.size()));
      it = local_sync_points.insert(local_sync_points.begin(), value);
    }

    command cmd;
    cmd.type = command_type::local_sync;
    cmd.arg1 = it->second;

    commands[thread_a].push_back(cmd);
    commands[thread_b].push_back(cmd);
  }
}

GlobalSyncPoint ReplayParser::CreateGlobalSync() {
  GlobalSyncPoint result = global_sync_points.size();
  global_sync_points.push_back(active_threads.size());
  return result;
}

ThreadId ReplayParser::GetThread(const std::string& thread_name) {
  auto it = thread_map.find(thread_name);
  if (it != thread_map.end()) {
    return it->second;
  }

  // If the thread doesn't exist, create a global sync point and tell the active threads.
  ThreadId result = thread_index++;

  if (result != 0) {
    // Only do this for threads that spawn after the first thread;
    GlobalSyncPoint sync_point = CreateGlobalSync();
    for (const auto& thread : active_threads) {
      command cmd;
      cmd.type = command_type::thread_start;
      cmd.arg1 = sync_point;
      cmd.arg2 = result;
      cmd.arg3 = 0;
      commands[thread].push_back(cmd);
    }
  }

  thread_map[thread_name] = result;
  active_threads.insert(result);
  commands[result];

  return result;
}

void ReplayParser::EmitDump(uint32_t line_number) {
  GlobalSyncPoint sync_point = CreateGlobalSync();
  for (const auto& thread : active_threads) {
    command cmd;
    cmd.type = command_type::dump;
    cmd.arg1 = sync_point;
    cmd.arg2 = line_number;
    cmd.arg3 = 0;
    commands[thread].push_back(cmd);
  }
}

void ReplayParser::HandleLine(char* line, uint32_t line_number) {
  char* saveptr;
  auto read_arg = [=, &saveptr](void) {
    char* result = strtok_r(nullptr, " ", &saveptr);
    if (!result) {
      errx(1, "line %" PRIu32 " malformed", line_number);
    }
    return result;
  };

  char* thread_id = strtok_r(line, ":", &saveptr);
  char* type = read_arg();

  ThreadId thread = GetThread(thread_id);
  command cmd;
  if (strcmp(type, "malloc") == 0) {
    cmd.type = command_type::malloc;
    cmd.arg1 = NewAllocation(read_arg(), thread);
    cmd.arg2 = atoi(read_arg());
  } else if (strcmp(type, "calloc") == 0) {
    cmd.type = command_type::calloc;
    cmd.arg1 = NewAllocation(read_arg(), thread);
    cmd.arg2 = atoi(read_arg());
  } else if (strcmp(type, "memalign") == 0) {
    cmd.type = command_type::memalign;
    cmd.arg1 = NewAllocation(read_arg(), thread);
    cmd.arg2 = atoi(read_arg());
  } else if (strcmp(type, "realloc") == 0) {
    const char* dst = read_arg();
    const char* src = read_arg();
    uint32_t size = atoi(read_arg());

    if (strcmp(src, "0") == 0 || strcmp(src, "0x0") == 0) {
      cmd.type = command_type::malloc;
      cmd.arg1 = NewAllocation(dst, thread);
      cmd.arg2 = size;
    } else {
      Allocation allocation = GetAllocation(src);
      EmitLocalSync(thread, allocation.owning_thread);
      allocation.owning_thread = thread;
      active_allocations.erase(src);
      active_allocations[dst] = allocation;

      cmd.type = command_type::realloc;
      cmd.arg1 = allocation.id;
      cmd.arg2 = size;
    }
  } else if (strcmp(type, "free") == 0) {
    const char* pointer = read_arg();
    if (strcmp(pointer, "0") == 0 || strcmp(pointer, "0x0") == 0) {
      return;
    }
    Allocation allocation = GetAllocation(pointer);
    if (active_threads.find(allocation.owning_thread) != active_threads.end()) {
      EmitLocalSync(thread, allocation.owning_thread);
    }
    active_allocations.erase(pointer);

    cmd.type = command_type::free;
    cmd.arg1 = allocation.id;
  } else if (strcmp(type, "thread_done") == 0) {
    cmd.type = command_type::thread_exit;
    cmd.arg1 = CreateGlobalSync();
    cmd.arg2 = thread;
    for (ThreadId thread : active_threads) {
      commands[thread].push_back(cmd);
    }
    active_threads.erase(thread);
    return;
  } else {
    errx(1, "line %" PRIu32 ": unhandled command '%s'", line_number, type);
  }

  commands[thread].push_back(cmd);
}

MemoryReplay ReplayParser::ParseReplay(FILE* input, uint32_t dump_interval) {
  // Create a dummy thread so that there is always at least one thread alive.
  commands[GetThread("")];

  char buf[4096];
  char* lineptr = buf;
  size_t size = sizeof(buf);
  uint32_t line_number = 0;

  while (true) {
    // Set errno to 0 so we can distinguish between failure and EOF.
    errno = 0;
    ssize_t result = getline(&lineptr, &size, input);
    if (result < 0) {
      if (errno == 0) {
        break;
      }
      err(1, "failed to read input file");
    }

    // Delete the trailing newline.
    buf[result - 1] = '\0';

    HandleLine(buf, line_number++);

    if (dump_interval != 0 && line_number % dump_interval == 0) {
      EmitDump(line_number);
    }
  }

  MemoryReplay replay;
  replay.allocation_count = allocation_count;
  replay.final_thread_count = active_threads.size();
  replay.commands = std::move(commands);
  replay.local_sync_points = std::move(local_sync_points);
  replay.global_sync_points = std::move(global_sync_points);
  return replay;
}

MemoryReplay ReplayParser::Parse(int fd, uint32_t dump_interval) {
  FILE* file = fdopen(fd, "r");
  ReplayParser parser;
  MemoryReplay replay = parser.ParseReplay(file, dump_interval);
  fclose(file);
  return replay;
}
