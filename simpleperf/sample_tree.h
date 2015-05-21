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

#ifndef SIMPLE_PERF_SAMPLE_TREE_H_
#define SIMPLE_PERF_SAMPLE_TREE_H_

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "build_id.h"

struct ProcessMap {
  int pid;
  uint64_t start_addr;
  uint64_t len;
  uint64_t pgoff;
  uint64_t time;  // Map creation time.
  std::string filename;
};

struct Sample {
  uint64_t ip;
  uint64_t time;
  uint64_t cpu;
  uint64_t period;
};

struct SampleInMap {
  int pid;
  int tid;
  const ProcessMap* map;
  std::vector<Sample> samples;
};

class SampleTree {
 public:
  SampleTree() {
    pids_.insert(0);  // Add pid 0, which is swapper/idle process.
  }
  void AddMap(int pid, uint64_t start_addr, uint64_t len, uint64_t pgoff,
              const std::string& filename, uint64_t time);
  void AddSample(int pid, int tid, uint64_t ip, uint64_t time, uint32_t cpu, uint64_t period);
  void VisitAllSamples(std::function<void(const SampleInMap&)> callback) const;

 private:
  const ProcessMap* FindMap(int pid, uint64_t ip);

  struct ProcessMapComparator {
    bool operator()(const ProcessMap&, const ProcessMap&);
  };
  struct SampleInMapComparator {
    bool operator()(const SampleInMap&, const SampleInMap&);
  };

  std::set<ProcessMap, ProcessMapComparator> map_tree_;
  std::set<SampleInMap, SampleInMapComparator> sample_tree_;
  std::set<int> pids_;
};

#endif  // SIMPLE_PERF_SAMPLE_TREE_H_
