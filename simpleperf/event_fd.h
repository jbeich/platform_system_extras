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

#include <sys/types.h>

#include <memory>
#include <string>

class EventAttr;

// EventFd represents an open perf_event_file.
class EventFd {
 public:
  ~EventFd();

  std::string Name() const;

  bool EnableEvent();
  bool DisableEvent();

  static std::unique_ptr<EventFd> OpenEventFileForProcess(EventAttr& attr, pid_t pid);
  static std::unique_ptr<EventFd> OpenEventFileForCpu(EventAttr& attr, int cpu);

 private:
  EventFd(int perf_event_fd, const std::string& event_name, pid_t pid, int cpu)
      : perf_event_fd(perf_event_fd), event_name(event_name), pid(pid), cpu(cpu) {
  }

  int perf_event_fd;
  const std::string event_name;
  pid_t pid;
  int cpu;

  EventFd(const EventFd&) = delete;
  EventFd& operator=(const EventFd&) = delete;
};

#endif  // SIMPLE_PERF_EVENT_FD_H_
