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
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "memreplay_fast.hpp"

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

static void xwrite(int fd, const void* buf, size_t len) {
  ssize_t result = write(fd, buf, len);
  if (result < 0 || static_cast<size_t>(result) != len) {
    err(1, "failed to write to output file");
  }
}

typedef uint32_t LocalSyncPoint;
typedef uint32_t GlobalSyncPoint;

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
  AllocationId allocation_index = 0;

  std::unordered_map<std::pair<ThreadId, ThreadId>, LocalSyncPoint> local_sync_points;
  std::vector<uint32_t> global_sync_points;

  AllocationId NewAllocation(const char* address, ThreadId thread) {
    auto it = active_allocations.find(address);
    if (it != active_allocations.end()) {
      errx(1, "preexisting address returned by allocation");
    }

    AllocationId id = allocation_index++;
    active_allocations[address] = { id, thread };
    return id;
  }

  Allocation GetAllocation(const char* address) {
    auto it = active_allocations.find(address);
    if (it == active_allocations.end()) {
      errx(1, "failed to find allocation");
    }
    return it->second;
  }

  void EmitLocalSync(ThreadId thread_a, ThreadId thread_b) {
    if (thread_a != thread_b) {
      auto key = std::make_pair(std::min(thread_a, thread_b), std::max(thread_a, thread_b));
      auto it = local_sync_points.find(key);
      if (it == local_sync_points.end()) {
        auto value = std::make_pair(key, static_cast<LocalSyncPoint>(local_sync_points.size()));
        it = local_sync_points.insert(local_sync_points.begin(), value);
      }

      command cmd = {.type = command_type::local_sync, .arg1 = it->second, .arg2 = 0, .arg3 = 0 };

      commands[thread_a].push_back(cmd);
      commands[thread_b].push_back(cmd);
    }
  }

  GlobalSyncPoint CreateGlobalSync() {
    GlobalSyncPoint result = global_sync_points.size();
    global_sync_points.push_back(active_threads.size());
    return result;
  }

  ThreadId GetThread(const std::string& thread_name) {
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

    return result;
  }

  void EmitDump(uint32_t line_number) {
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

  void HandleLine(char* line, uint32_t line_number) {
    char* saveptr;
    auto read_arg = [=, &saveptr](void) {
      char* result = strtok_r(nullptr, " ", &saveptr);
      if (!result) {
        errx(1, "line %" PRIu32 " malformed", line_number);
      }
      return result;
    };

    char* thread_id = strtok_r(line, ":", &saveptr);
    char* command_type = read_arg();

    ThreadId thread = GetThread(thread_id);
    command cmd = {};
    if (strcmp(command_type, "malloc") == 0) {
      AllocationId allocation = NewAllocation(read_arg(), thread);

      cmd.type = command_type::malloc;
      cmd.arg1 = allocation;
      cmd.arg2 = atoi(read_arg());
    } else if (strcmp(command_type, "calloc") == 0) {
      AllocationId allocation = NewAllocation(read_arg(), thread);
      cmd.type = command_type::calloc;
      cmd.arg1 = allocation;
      cmd.arg2 = atoi(read_arg());
    } else if (strcmp(command_type, "memalign") == 0) {
      AllocationId allocation = NewAllocation(read_arg(), thread);
      cmd.type = command_type::memalign;
      cmd.arg1 = allocation;
      cmd.arg2 = atoi(read_arg());
    } else if (strcmp(command_type, "realloc") == 0) {
      const char* dst = read_arg();
      const char* src = read_arg();
      uint32_t size = atoi(read_arg());

      if (strcmp(src, "0") == 0 || strcmp(src, "0x0") == 0) {
        AllocationId allocation = NewAllocation(dst, thread);
        cmd.type = command_type::malloc;
        cmd.arg1 = allocation;
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
    } else if (strcmp(command_type, "free") == 0) {
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
    } else if (strcmp(command_type, "thread_done") == 0) {
      GlobalSyncPoint sync_point = CreateGlobalSync();
      cmd.type = command_type::thread_exit;
      cmd.arg1 = sync_point;
      cmd.arg2 = thread;
      for (ThreadId thread : active_threads) {
        commands[thread].push_back(cmd);
      }
      active_threads.erase(thread);
      return;
    } else {
      errx(1, "line %" PRIu32 ": unhandled command '%s'", line_number, command_type);
    }

    commands[thread].push_back(cmd);
  }

 public:
  void ParseReplay(std::istream& input, uint32_t dump_interval = 100000) {
    // Create a dummy thread so that there is always at least one thread alive.
    GetThread("");

    char buf[4096];
    uint32_t line_number = 0;
    std::string last_line;
    while (true) {
      input.getline(buf, sizeof(buf));
      if (input.eof()) {
        return;
      } else if (input.fail()) {
        errx(1, "failed to read input file");
      }
      last_line = buf;
      HandleLine(buf, line_number++);

      if (dump_interval != 0 && line_number % dump_interval == 0) {
        EmitDump(line_number);
      }
    }
  }

  void WriteDump(int fd) {
    file_header header = {.magic = { 'M', 'E', 'M', '_', 'R', 'P', 'L', 'Y' },
                          .allocation_count = allocation_index,
                          .thread_count = thread_index,
                          .final_thread_count = ThreadId(active_threads.size()),
                          .local_sync_count = LocalSyncPoint(local_sync_points.size()),
                          .global_sync_count = GlobalSyncPoint(global_sync_points.size()) };
    xwrite(fd, &header, sizeof(header));
    xwrite(fd, global_sync_points.data(), 4 * global_sync_points.size());

    for (size_t i = 0; i < commands.size(); ++i) {
      thread_header th;
      th.thread_id = i;
      th.command_count = commands[i].size();
      xwrite(fd, &th, sizeof(th));
      xwrite(fd, commands[i].data(), sizeof(command) * th.command_count);
    }
  }
};

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("usage: %s <input file> <output file>\n", argv[0]);
    return 1;
  }

  ReplayParser parser;
  std::ifstream input_stream(argv[1]);
  parser.ParseReplay(input_stream);

  int fd = open(argv[2], O_RDWR | O_TRUNC | O_CREAT, 0755);
  if (fd < 0) {
    err(1, "failed to open output file '%s'", argv[2]);
  }
  parser.WriteDump(fd);
  close(fd);

  printf("Successfully preprocessed %s\n", argv[1]);
}
