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
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <memory>

#include "event_attr.h"
#include "perf_event.h"

int perf_event_open(perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForProcess(EventAttr& attr, pid_t pid) {
  int perf_event_fd = perf_event_open(attr.Attr(), pid, -1, -1, 0);
  if (perf_event_fd == -1) {
    return std::unique_ptr<EventFd>(nullptr);
  }
  if (fcntl(perf_event_fd, F_SETFD, FD_CLOEXEC) == -1) {
    return std::unique_ptr<EventFd>(nullptr);
  }
  return std::unique_ptr<EventFd>(new EventFd(perf_event_fd));
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForCpu(EventAttr& attr, int cpu) {
  int perf_event_fd = perf_event_open(attr.Attr(), -1, cpu, -1, 0);
  if (perf_event_fd == -1) {
    fprintf(stderr, "perf_event_open: %s\n", strerror(errno));
    return std::unique_ptr<EventFd>(nullptr);
  }
  return std::unique_ptr<EventFd>(new EventFd(perf_event_fd));
}

EventFd::~EventFd() {
  close(perf_event_fd);
}

static bool ReadFileBytes(int fd, void* buf, size_t bytes);

bool EventFd::ReadCounter(PerfCountStruct& counter) {
  uint64_t values[4];
  if (!ReadFileBytes(perf_event_fd, values, sizeof(values))) {
    return false;
  }
  counter.count = values[0];
  counter.time_enabled = values[1];
  counter.time_running = values[2];
  counter.id = values[3];
  return true;
}

static bool ReadFileBytes(int fd, void* buf, size_t bytes) {
  char* p = reinterpret_cast<char*>(buf);
  size_t bytes_read = 0;
  while (bytes_read < bytes) {
    ssize_t nread = TEMP_FAILURE_RETRY(read(fd, p + bytes_read, bytes - bytes_read));
    if (nread <= 0) {
      fprintf(stderr, "ReadFileBytes failed: %s\n", strerror(errno));
      return false;
    } else {
      bytes_read += static_cast<size_t>(nread);
    }
  }
  return true;
}

bool EventFd::EnableEvent() {
  return ioctl(perf_event_fd, PERF_EVENT_IOC_ENABLE, 0) == 0;
}

bool EventFd::DisableEvent() {
  return ioctl(perf_event_fd, PERF_EVENT_IOC_DISABLE, 0) == 0;
}
