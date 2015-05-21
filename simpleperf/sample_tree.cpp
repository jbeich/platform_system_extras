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

#include "sample_tree.h"

#include <base/logging.h>

static bool CompareProcessMap(const ProcessMap& map1, const ProcessMap& map2) {
  if (map1.pid != map2.pid) {
    return map1.pid < map2.pid;
  }
  if (map1.start_addr != map2.start_addr) {
    return map1.start_addr < map2.start_addr;
  }
  if (map1.len != map2.len) {
    return map1.len < map2.len;
  }
  if (map1.pgoff != map2.pgoff) {
    return map1.pgoff < map2.pgoff;
  }
  if (map1.time != map2.time) {
    return map1.time < map2.time;
  }
  if (map1.filename != map2.filename) {
    return map1.filename < map2.filename;
  }
  return false;
}

bool SampleTree::ProcessMapComparator::operator()(const ProcessMap& map1, const ProcessMap& map2) {
  return CompareProcessMap(map1, map2);
}

bool SampleTree::SampleInMapComparator::operator()(const SampleInMap& sample1,
                                                   const SampleInMap& sample2) {
  if (sample1.pid != sample2.pid) {
    return sample1.pid < sample2.pid;
  }
  if (sample1.tid != sample2.tid) {
    return sample1.tid < sample2.tid;
  }
  return CompareProcessMap(*sample1.map, *sample2.map);
}

void SampleTree::AddMap(int pid, uint64_t start_addr, uint64_t len, uint64_t pgoff,
                        const std::string& filename, uint64_t time) {
  ProcessMap map;
  map.pid = pid;
  map.start_addr = start_addr;
  map.len = len;
  map.pgoff = pgoff;
  map.time = time;
  map.filename = filename;
  map_tree_.insert(map);
}

static bool IsIpInMap(int pid, uint64_t ip, const ProcessMap& map) {
  return (pid == map.pid && map.start_addr <= ip && map.start_addr + map.len > ip);
}

const ProcessMap* SampleTree::FindMap(int pid, uint64_t ip) {
  ProcessMap map;
  map.pid = pid;
  map.start_addr = ip;
  map.len = ULLONG_MAX;
  map.pgoff = 0;
  auto it = map_tree_.upper_bound(map);
  if (it != map_tree_.begin() && IsIpInMap(map.pid, ip, *--it)) {
    return &*it;
  }
  // Check if the ip is in the kernel. Maps of the kernel use -1 as pid.
  map.pid = -1;
  it = map_tree_.upper_bound(map);
  if (it != map_tree_.begin() && IsIpInMap(map.pid, ip, *--it)) {
    return &*it;
  }
  return nullptr;
}

void SampleTree::AddSample(int pid, int tid, uint64_t ip, uint64_t time, uint32_t cpu,
                           uint64_t period) {
  const ProcessMap* map = FindMap(pid, ip);
  if (map == nullptr) {
    LOG(WARNING) << "Can't find map for sample (pid " << pid << ", tid " << tid << ", ip "
                 << std::hex << ip << std::dec << ", time " << time << ")";
    return;
  }
  Sample s;
  s.ip = ip;
  s.time = time;
  s.cpu = cpu;
  s.period = period;

  SampleInMap sample;
  sample.pid = pid;
  sample.tid = tid;
  sample.map = map;
  auto it = sample_tree_.find(sample);
  if (it != sample_tree_.end()) {
    auto& samples = *const_cast<std::vector<Sample>*>(&it->samples);
    samples.push_back(s);
  } else {
    sample.samples.push_back(s);
    sample_tree_.insert(sample);
  }
}

void SampleTree::VisitAllSamples(std::function<void(const SampleInMap&)> callback) const {
  for (auto& sample : sample_tree_) {
    callback(sample);
  }
}
