/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef SIMPLE_PERF_INPLACE_SAMPLER_PROTOCOL_H_
#define SIMPLE_PERF_INPLACE_SAMPLER_PROTOCOL_H_

#include <sys/types.h>

#include <string>
#include <vector>

static const std::string INPLACE_SERVER_NAME = "inplace_sampler_";

// Inplace Sampler Messages:
enum InplaceSamplerMessageType {
  START_PROFILING,
  START_PROFILING_REPLY,
  SAMPLE_DATA,
  MAP_DATA,
};

// When constructing a message, make sure the data alignment
// is fine when accessing each member.

// Type: START_PROFILING
// Direction: client to server
// Binary Data:
//   int32_t signo
//   uint32_t freq
//   uint32_t tid_nr
//   uint32_t  tid[tid_nr]
struct MessageStartProfiling {
  uint32_t signo;
  uint32_t freq;
  std::vector<uint32_t> tid;
};

// Type: START_PROFILING_REPLY
// Direction: server to client
// Binary Data:
//   nothing

// Type: SAMPLE_DATA
// Direction: server to client
// Binary Data:
//   uint64_t  tid
//   uint64_t time
//   uint64_t period
//   uint64_t ip_nr
//   uint64_t ip[ip_nr]
struct MessageSampleData {
  uint64_t tid;
  uint64_t time;
  uint64_t period;
  std::vector<uint64_t> ip;
};

// Type: MAP_DATA
// Direction: server to client
// Binary Data:
//   uint64_t time
//   uint64_t tid_nr
//   uint64_t  tid1
//   char     comm1[?] // '\0' terminated string, aligned for 64-bit
//   ....
//   uint64_t map_nr
//   uint64_t map1_start
//   uint64_t map1_len
//   uint64_t map1_offset
//   char     map1_dso[?] // '\0' terminated string, aligned for 64-bit
//   ...
struct MessageMapData {
  uint64_t time;
  struct TidComm {
    uint64_t tid;
    std::string comm;
  };
  std::vector<TidComm> tids;
  struct Map {
    uint64_t start;
    uint64_t len;
    uint64_t offset;
    std::string dso;
  };
  std::vector<Map> maps;
};

#endif  // SIMPLE_PERF_INPLACE_SAMPLER_PROTOCOL_H_
