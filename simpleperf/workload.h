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

#ifndef SIMPLE_PERF_WORKLOAD_H_
#define SIMPLE_PERF_WORKLOAD_H_

#include <sys/types.h>
#include <chrono>
#include <string>
#include <vector>

#include <base/macros.h>

class Workload {
 public:
  static std::unique_ptr<Workload> CreateWorkloadInNewProcess(const std::vector<std::string>& args);
  static std::unique_ptr<Workload> CreateWorkloadOfSleep(std::chrono::seconds sleep_duration);

  virtual ~Workload() {
  }

  virtual bool Start() = 0;
  virtual bool IsFinished() = 0;
  virtual bool WaitFinish() = 0;
  virtual pid_t GetWorkPid() = 0;

 protected:
  Workload() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Workload);
};

#endif  // SIMPLE_PERF_WORKLOAD_H_
