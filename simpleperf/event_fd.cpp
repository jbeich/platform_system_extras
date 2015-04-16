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

#include "event_fd.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <memory>

#include <base/stringprintf.h>

#include "event.h"
#include "event_attr.h"
#include "perf_event.h"
#include "utils.h"

static int perf_event_open(perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForProcess(const EventAttr& attr, pid_t pid) {
  return OpenEventFile(attr, pid, -1);
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForCpu(const EventAttr& attr, int cpu) {
  return OpenEventFile(attr, -1, cpu);
}

std::unique_ptr<EventFd> EventFd::OpenEventFile(const EventAttr& attr, pid_t pid, int cpu) {
  perf_event_attr perf_attr = attr.Attr();
  std::string event_name = "unknown event";
  const Event* event = Event::FindEventByTypeAndConfig(perf_attr.type, perf_attr.config);
  if (event != nullptr) {
    event_name = event->Name();
  }
  int perf_event_fd = perf_event_open(&perf_attr, pid, cpu, -1, 0);
  if (perf_event_fd == -1) {
    SLOGW("open perf_event_file (event %s, pid %d, cpu %d) failed", event_name.c_str(), pid, cpu);
    return nullptr;
  }
  if (fcntl(perf_event_fd, F_SETFD, FD_CLOEXEC) == -1) {
    SLOGW("fcntl(FD_CLOEXEC) for perf_event_file (event %s, pid %d, cpu %d) failed", event_name.c_str(), pid,
          cpu);
    return nullptr;
  }
  return std::unique_ptr<EventFd>(new EventFd(perf_event_fd, event_name, pid, cpu));
}

EventFd::~EventFd() {
  close(perf_event_fd_);
}

std::string EventFd::Name() const {
  return android::base::StringPrintf("perf_event_file(event %s, pid %d, cpu %d)", event_name_.c_str(), pid_,
                                     cpu_);
}

bool EventFd::EnableEvent() {
  int result = ioctl(perf_event_fd_, PERF_EVENT_IOC_ENABLE, 0);
  if (result < 0) {
    SLOGE("ioctl(enable) %s failed", Name().c_str());
    return false;
  }
  return true;
}

bool EventFd::DisableEvent() {
  int result = ioctl(perf_event_fd_, PERF_EVENT_IOC_DISABLE, 0);
  if (result < 0) {
    SLOGE("ioctl(disable) %s failed", Name().c_str());
    return false;
  }
  return true;
}
