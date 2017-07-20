/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SIMPLE_PERF_CMD_RECORD_H_
#define SIMPLE_PERF_CMD_RECORD_H_

#include <sys/types.h>

#include <memory>
#include <set>
#include <string>

#include "command.h"

static constexpr char RECORDING_CONTROL_START_BYTE = 'S';
static constexpr char RECORDING_CONTROL_STOP_BYTE = 'E';
static constexpr char RECORDING_CONTROL_FINISH_BYTE = 'F';

struct CmdRecordBuilder {
  std::set<std::string> events;
  std::set<pid_t> processes;
  std::set<pid_t> threads;
  std::set<pid_t> exclude_threads;
  bool inherit;
  uint64_t sample_freq;
  uint64_t sample_period;
  bool fp_callgraph_sampling;
  bool dwarf_callgraph_sampling;
  uint32_t dump_stack_size_in_dwarf_sampling;
  std::string record_filepath;
  int recording_control_fd;

  CmdRecordBuilder() : sample_freq(0), sample_period(0), fp_callgraph_sampling(false),
      dwarf_callgraph_sampling(false), dump_stack_size_in_dwarf_sampling(0),
      recording_control_fd(-1) {
  }

  std::unique_ptr<Command> Build();
};

#endif  // SIMPLE_PERF_CMD_RECORD_H_
