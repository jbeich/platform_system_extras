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

#ifndef SIMPLE_PERF_EVENT_FD_H_
#define SIMPLE_PERF_EVENT_FD_H_

#include <stdint.h>
#include <sys/types.h>
#include <memory>

#include "event_attr.h"

struct PerfCountStruct {
  uint64_t count;
  uint64_t time_enabled;
  uint64_t time_running;
  uint64_t id;

  PerfCountStruct() : count(0), time_enabled(0), time_running(0) { }
};

class Event;
class EventAttr;

class EventFd {
 public:
  static std::unique_ptr<EventFd> OpenEventFileForProcess(EventAttr& attr, pid_t pid);

  static std::unique_ptr<EventFd> OpenEventFileForCpu(EventAttr& attr, int cpu);

  ~EventFd();

  EventFd(const EventFd&) = delete;
  EventFd& operator=(const EventFd&) = delete;

  bool ReadCounter(PerfCountStruct& counter);

  bool EnableEvent();
  bool DisableEvent();

  int Fd() const {
    return perf_event_fd;
  }

 private:
  EventFd(int perf_event_fd) : perf_event_fd(perf_event_fd) { }

 private:
  int perf_event_fd;
};

#endif  // SIMPLE_PERF_EVENT_FD_H_
