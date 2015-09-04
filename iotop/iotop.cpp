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

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

#include "log.h"
#include "tasklist.h"
#include "taskstats.h"

constexpr uint64_t NSEC_PER_SEC = 1000000000;

static uint64_t BytesToKB(uint64_t bytes) {
  return (bytes + 1024-1) / 1024;
}

static float TimeToTgidPercent(uint64_t ns, int time, TaskStatistics& stats) {
  float percent = ns / stats.threads / (time * NSEC_PER_SEC / 100.0f);
  return std::min(percent, 99.99f);
}

static void usage(char* myname) {
  printf(
      "Usage: %s [-h] [-p] [-d <delay>] [-n <cycles>] [-s <column>]\n"
      "\n"
      "   -h  Display this help screen.\n"
      "   -d  Set the delay between refreshes in seconds.\n"
      "   -n  Set the number of refreshes before exiting.\n"
      "   -p  Show processes instead of the default threads.\n"
      "   -s  Set the column to sort by:\n"
      "       pid, read, write, total, io, swap, sched, mem or delay.\n",
      myname);
}

typedef std::function<void(std::vector<TaskStatistics>::iterator,
                           std::vector<TaskStatistics>::iterator)> sorter;
static sorter GetSorter(const std::string field) {
  auto comparator = [](const TaskStatistics& lhs, const TaskStatistics& rhs,
                       auto sort_field, bool sort_ascending) -> bool {
    auto a = (lhs.*sort_field)();
    auto b = (rhs.*sort_field)();
    if (a != b) {
      return sort_ascending ^ (a < b);
    } else {
      return lhs.Pid() < rhs.Pid();
    }
  };

  auto make_sorter = [comparator](auto field, bool ascending) {
    return [comparator, field, ascending](auto start, auto finish) {
      std::sort(start, finish,
                [comparator, field, ascending](auto lhs, auto rhs) {
                  return comparator(lhs, rhs, field, ascending);
                });
    };
  };

  const std::unordered_map<std::string, sorter> sorters{
      {"pid", make_sorter(&TaskStatistics::Pid, false)},
      {"read", make_sorter(&TaskStatistics::Read, true)},
      {"write", make_sorter(&TaskStatistics::Write, true)},
      {"total", make_sorter(&TaskStatistics::ReadWrite, true)},
      {"io", make_sorter(&TaskStatistics::DelayIO, true)},
      {"swap", make_sorter(&TaskStatistics::DelaySwap, true)},
      {"sched", make_sorter(&TaskStatistics::DelaySched, true)},
      {"mem", make_sorter(&TaskStatistics::DelayMem, true)},
      {"delay", make_sorter(&TaskStatistics::DelayTotal, true)},
  };

  auto it = sorters.find(field);
  if (it == sorters.end()) {
    return nullptr;
  }
  return it->second;
}

int main(int argc, char* argv[]) {
  bool processes = false;
  int delay = 1;
  int cycles = -1;
  int limit = -1;
  sorter sorter = GetSorter("total");

  while (1) {
    int c;
    const struct option longopts[] = {
        {"delay", required_argument, 0, 'd'},
        {"help", 0, 0, 'h'},
        {"limit", required_argument, 0, 'm'},
        {"cycles", required_argument, 0, 'n'},
        {"sort", required_argument, 0, 's'},
        {"processes", 0, 0, 'p'},
        {0, 0, 0, 0},
    };
    c = getopt_long(argc, argv, "d:hm:n:s:p", longopts, NULL);
    if (c < 0) {
      break;
    }
    switch (c) {
    case 'd':
      delay = atoi(optarg);
      break;
    case 'h':
      usage(argv[0]);
      return(EXIT_SUCCESS);
    case 'm':
      limit = atoi(optarg);
      break;
    case 'n':
      cycles = atoi(optarg);
      break;
    case 's': {
      sorter = GetSorter(optarg);
      if (sorter == nullptr) {
        ERROR("Invalid sort column \"%s\"", optarg);
        return(EXIT_FAILURE);
      }
      break;
    }
    case 'p':
      processes = true;
      break;
    case '?':
      ERROR("Invalid argument \"%s\"", argv[optind - 1]);
      usage(argv[0]);
      return(EXIT_FAILURE);
    default:
      abort();
    }
  }

  std::map<pid_t, std::vector<pid_t>> tgid_map;

  TaskstatsSocket taskstats_socket;
  taskstats_socket.Open();

  std::unordered_map<pid_t, TaskStatistics> old_stats;
  std::vector<TaskStatistics> diff_stats;

  bool first = true;
  bool second = true;

  while (true) {
    diff_stats.clear();
    if (!TaskList::Scan(tgid_map)) {
      ERROR("failed to scan tasks");
      return -1;
    }
    for (auto& tgid_it : tgid_map) {
      pid_t tgid = tgid_it.first;
      std::vector<pid_t>& pid_list = tgid_it.second;

      if (processes) {
        TaskStatistics tgid_statistics;
        if (!taskstats_socket.GetTgidStats(tgid, tgid_statistics)) {
          continue;
        }
        for (pid_t pid : pid_list) {
          TaskStatistics pid_statistics;
          if (!taskstats_socket.GetPidStats(pid, pid_statistics)) {
            continue;
          }
          tgid_statistics.AddPidToTgid(pid_statistics);
        }

        diff_stats.push_back(tgid_statistics.Delta(old_stats[tgid]));
        old_stats[tgid] = tgid_statistics;
      } else {
        for (pid_t pid : pid_list) {
          TaskStatistics pid_statistics;
          if (!taskstats_socket.GetPidStats(pid, pid_statistics)) {
            continue;
          }
          diff_stats.push_back(pid_statistics.Delta(old_stats[pid]));
          old_stats[pid] = pid_statistics;
        }
      }
    }

    if (!first) {
      sorter(diff_stats.begin(), diff_stats.end());
      if (!second) {
        printf("\n");
      }
      printf("%6s %-16s %20s %34s\n", "", "",
          "--- IO (KiB/s) ---", "----------- delayed on ----------");
      printf("%6s %-16s %6s %6s %6s  %-5s  %-5s  %-5s  %-5s  %-5s\n",
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
      int n = limit;
      for (TaskStatistics& statistics : diff_stats) {
        printf("%6d %-16s %6" PRIu64 " %6" PRIu64 " %6" PRIu64 " %5.2f%% %5.2f%% %5.2f%% %5.2f%% %5.2f%%\n",
            statistics.pid,
            statistics.comm.c_str(),
            BytesToKB(statistics.read_bytes),
            BytesToKB(statistics.write_bytes),
            BytesToKB(statistics.read_write_bytes),
            TimeToTgidPercent(statistics.block_io_delay_ns, delay, statistics),
            TimeToTgidPercent(statistics.swap_in_delay_ns, delay, statistics),
            TimeToTgidPercent(statistics.cpu_delay_ns, delay, statistics),
            TimeToTgidPercent(statistics.reclaim_delay_ns, delay, statistics),
            TimeToTgidPercent(statistics.total_delay_ns, delay, statistics));
        if (n > 0 && --n == 0) break;
      }
      second = false;

      if (cycles > 0 && --cycles == 0) break;
    }
    first = false;
    sleep(1);
  }

  return 0;
}
