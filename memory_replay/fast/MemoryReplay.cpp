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
#include <unistd.h>

#include "MemoryReplay.h"
#include "memreplay_fast.h"

static void xwrite(int fd, const void* buf, size_t len) {
  ssize_t result = write(fd, buf, len);
  if (result < 0 || static_cast<size_t>(result) != len) {
    err(1, "failed to write to output file");
  }
}

MemoryReplay::MemoryReplay() {
}

MemoryReplay::MemoryReplay(MemoryReplay&& move) {
  allocation_count = move.allocation_count;
  final_thread_count = move.final_thread_count;
  commands = std::move(move.commands);
  local_sync_points = std::move(move.local_sync_points);
  global_sync_points = std::move(move.global_sync_points);
}

void MemoryReplay::WriteDump(int fd) {
  file_header header = {.magic = { 'M', 'E', 'M', '_', 'R', 'P', 'L', 'Y' },
                        .allocation_count = allocation_count,
                        .thread_count = ThreadId(commands.size()),
                        .final_thread_count = final_thread_count,
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
