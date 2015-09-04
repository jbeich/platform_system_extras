// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sys/cdefs.h>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

#include "log.h"
#include "tasklist.h"
#include "taskstats.h"

#ifndef __unused
#define __unused __attribute__((unused))
#endif

const uint64_t NSEC_PER_SEC = 1000000000;

static uint64_t BytesToKB(uint64_t bytes) {
  return (bytes + 1024-1) / 1024;
}

float TimeToTgidPercent(uint64_t ns, int time, TaskStatistics& stats) {
  float percent = ns / stats.threads / (time * NSEC_PER_SEC / 100.0f);
  return std::min(percent, 99.99f);
}

int main(int argc __unused, char *argv[] __unused) {
  std::map<pid_t, std::vector<pid_t>> tgid_map;

  TaskstatsSocket taskstats_socket;
  taskstats_socket.Open();

  std::unordered_map<pid_t, TaskStatistics> old_stats;
  std::vector<TaskStatistics> diff_stats;

  bool first = true;
  bool second = true;

  const int loop_duration = 1;

  while (true) {
    diff_stats.clear();
    if (!TaskList::Scan(tgid_map)) {
      ERROR("failed to scan tasks");
      return -1;
    }
    for (auto& tgid_it : tgid_map) {
      pid_t tgid = tgid_it.first;
      std::vector<pid_t>& pid_list = tgid_it.second;

      TaskStatistics tgid_statistics;
      if (!taskstats_socket.GetTgidStats(tgid, tgid_statistics)) {
        continue;
      }
      for (auto &pid : pid_list) {
          TaskStatistics pid_statistics;
          if (!taskstats_socket.GetPidStats(pid, pid_statistics)) {
            continue;
          }
          tgid_statistics.AddPidToTgid(pid_statistics);
      }

      diff_stats.push_back(tgid_statistics.Delta(old_stats[tgid]));
      old_stats[tgid] = tgid_statistics;
    }

    if (!first) {
        std::stable_sort(diff_stats.begin(), diff_stats.end(),
          TaskStatistics::Compare(TaskStatistics::DELAYTOTAL));
      int count = 10;
      if (!second) {
        printf("\n");
      }
      printf("%6s %-16s %20s %34s\n", "", "",
          "---- IO (KB/s) ----", "--------- delayed on ---------");
      printf("%6s %-16s %6s %6s %6s %6s %6s %6s %6s %6s\n",
          "PID",
          "Command",
          "read",
          "write",
          "total",
          "IO",
          "swap",
          "sched",
          "mem",
          "total");
      for (auto tgid_statistics : diff_stats) {
        if (count-- == 0) break;
        printf("%6d %-16s %6lu %6lu %6lu %5.2f%% %5.2f%% %5.2f%% %5.2f%% %5.2f%%\n",
            tgid_statistics.pid,
            tgid_statistics.comm.c_str(),
            BytesToKB(tgid_statistics.read_bytes),
            BytesToKB(tgid_statistics.write_bytes),
            BytesToKB(tgid_statistics.read_bytes + tgid_statistics.write_bytes),
            TimeToTgidPercent(tgid_statistics.block_io_delay_ns, loop_duration, tgid_statistics),
            TimeToTgidPercent(tgid_statistics.swap_in_delay_ns, loop_duration, tgid_statistics),
            TimeToTgidPercent(tgid_statistics.cpu_delay_ns, loop_duration, tgid_statistics),
            TimeToTgidPercent(tgid_statistics.reclaim_delay_ns, loop_duration, tgid_statistics),
            TimeToTgidPercent(tgid_statistics.block_io_delay_ns +
                tgid_statistics.swap_in_delay_ns +
                tgid_statistics.cpu_delay_ns +
                tgid_statistics.reclaim_delay_ns,
                loop_duration, tgid_statistics));
      }
      second = false;
    }
    first = false;
    sleep(1);
  }

	return 0;
}
