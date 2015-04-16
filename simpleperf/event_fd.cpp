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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <memory>

#include <base/stringprintf.h>

#include "event_attr.h"
#include "perf_event.h"
#include "utils.h"

static int perf_event_open(perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForProcess(EventAttr& attr, pid_t pid) {
  int perf_event_fd = perf_event_open(&attr.Attr(), pid, -1, -1, 0);
  if (perf_event_fd == -1) {
    LOGW("open perf_event_file (event %s, pid %d) failed: %s", attr.Name().c_str(), pid, strerror(errno));
    return nullptr;
  }
  if (fcntl(perf_event_fd, F_SETFD, FD_CLOEXEC) == -1) {
    LOGW("fcntl(FD_CLOEXEC) for perf_event_file (event %s, pid %d) failed: %s", attr.Name().c_str(), pid,
         strerror(errno));
    return nullptr;
  }
  return std::unique_ptr<EventFd>(new EventFd(perf_event_fd, attr.Name(), pid, -1));
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForCpu(EventAttr& attr, int cpu) {
  int perf_event_fd = perf_event_open(&attr.Attr(), -1, cpu, -1, 0);
  if (perf_event_fd == -1) {
    LOGW("open perf_event_file (event %s, cpu %d) failed: %s", attr.Name().c_str(), cpu, strerror(errno));
    return nullptr;
  }
  if (fcntl(perf_event_fd, F_SETFD, FD_CLOEXEC) == -1) {
    LOGW("fcntl(FD_CLOEXEC) for perf_event_file (event %s, cpu %d) failed: %s", attr.Name().c_str(), cpu,
         strerror(errno));
    return nullptr;
  }
  return std::unique_ptr<EventFd>(new EventFd(perf_event_fd, attr.Name(), -1, cpu));
}

EventFd::~EventFd() {
  close(perf_event_fd);
}

std::string EventFd::Name() const {
  return android::base::StringPrintf("perf_event_file(event %s, pid %d, cpu %d)", event_name.c_str(), pid,
                                     cpu);
}

bool EventFd::EnableEvent() {
  int result = ioctl(perf_event_fd, PERF_EVENT_IOC_ENABLE, 0);
  if (result < 0) {
    LOGE("ioctl(enable) %s failed: %s", Name().c_str(), strerror(errno));
    return false;
  }
  return true;
}

bool EventFd::DisableEvent() {
  int result = ioctl(perf_event_fd, PERF_EVENT_IOC_DISABLE, 0);
  if (result < 0) {
    LOGE("ioctl(disable) %s failed: %s", Name().c_str(), strerror(errno));
    return false;
  }
  return true;
}
