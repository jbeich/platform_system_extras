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

#ifndef SIMPLE_PERF_INPLACE_SAMPLER_H_
#define SIMPLE_PERF_INPLACE_SAMPLER_H_

#include <set>
#include <vector>

#include "event_attr.h"
#include "record.h"
#include "UnixSocket.h"

class InplaceSampler {
 public:
  static std::unique_ptr<InplaceSampler> Create(const perf_event_attr& attr,
                                                const std::set<pid_t>& processes,
                                                const std::set<pid_t>& threads);
  uint64_t Id() const;
  bool StartPolling(IOEventLoop& loop, const std::function<bool(Record*)>& callback);

 private:
  InplaceSampler(const perf_event_attr& attr, pid_t pid,
                 const std::vector<pid_t>& tids);
  bool ConnectServer();
  bool StartProfiling();
  bool ProcessMessage(const UnixSocketMessage& msg);
  bool SendStartProfilingMessage();

  const perf_event_attr attr_;
  const pid_t pid_;
  uint32_t freq_;
  const std::vector<pid_t> tids_;
  std::unique_ptr<UnixSocketConnection> conn_;
  std::function<bool(Record*)> record_callback_;
};

#endif  // SIMPLE_PERF_INPLACE_SAMPLER_H_
