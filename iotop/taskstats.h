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

#include <memory>
#include <string>

#include <stdint.h>

#ifndef _IOTOP_TASKSTATS_H
#define _IOTOP_TASKSTATS_H

struct nl_sock;

class TaskStatistics {
public:
  TaskStatistics(struct taskstats& taskstats);
  TaskStatistics() = default;
  TaskStatistics(const TaskStatistics&) = default;
  void AddPidToTgid(const TaskStatistics&);
  TaskStatistics Delta(const TaskStatistics&) const;

  pid_t Pid() const { return pid; }
  uint64_t Read() const { return read_bytes; }
  uint64_t Write() const { return write_bytes; }
  uint64_t ReadWrite() const { return read_write_bytes; }
  uint64_t DelayIO() const { return block_io_delay_ns; }
  uint64_t DelaySwap() const { return swap_in_delay_ns; }
  uint64_t DelaySched() const { return cpu_delay_ns; }
  uint64_t DelayMem() const { return reclaim_delay_ns; }
  uint64_t DelayTotal() const { return total_delay_ns; }

  std::string comm;
  uid_t uid;
  gid_t gid;
  pid_t pid;
  pid_t ppid;

  uint64_t cpu_delay_count;
  uint64_t cpu_delay_ns;

  uint64_t block_io_delay_count;
  uint64_t block_io_delay_ns;

  uint64_t swap_in_delay_count;
  uint64_t swap_in_delay_ns;

  uint64_t reclaim_delay_count;
  uint64_t reclaim_delay_ns;

  uint64_t total_delay_ns;

  uint64_t cpu_time_real;
  uint64_t cpu_time_virtual;

  uint64_t read_bytes;
  uint64_t write_bytes;
  uint64_t read_write_bytes;
  uint64_t cancelled_write_bytes;

  int threads;
};

class TaskstatsSocket {
public:
  TaskstatsSocket();
  bool Open();
  void Close();

  bool GetPidStats(int, TaskStatistics&);
  bool GetTgidStats(int, TaskStatistics&);
private:
  bool GetStats(int, int, TaskStatistics& stats);
  std::unique_ptr<struct nl_sock, void(*)(struct nl_sock*)> nl_;
  int family_id_;
};

#endif // _IOTOP_TASKSTATS_H
