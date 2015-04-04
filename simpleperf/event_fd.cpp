#include "event_fd.h"

#include <errno.h>
#include <linux/perf_event.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <memory>

#include "event_attr.h"

int perf_event_open(perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForProcess(EventAttr& attr, pid_t pid) {
  int perf_event_fd = perf_event_open(attr.Attr(), pid, -1, -1, 0);
  if (perf_event_fd == -1) {
    return std::unique_ptr<EventFd>(nullptr);
  }
  return std::unique_ptr<EventFd>(new EventFd(attr, perf_event_fd));
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForCpu(EventAttr& attr, int cpu) {
  int perf_event_fd = perf_event_open(attr.Attr(), -1, cpu, -1, 0);
  if (perf_event_fd == -1) {
    return std::unique_ptr<EventFd>(nullptr);
  }
  return std::unique_ptr<EventFd>(new EventFd(attr, perf_event_fd));
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
      fprintf(stderr, "ReadFileBytes fails: %s\n", strerror(errno));
      return false;
    } else {
      bytes_read += static_cast<size_t>(nread);
    }
  }
  return true;
}

bool EventFd::GetEventId(uint64_t& id) {
/*
  if (event_id == static_cast<uint64_t>(-1)) {
    PerfCountStruct counter;
    if (!ReadCounter(counter)) {
      return false;
    }
    event_id = counter.id;
  }
*/
  id = event_id;
  return true;
}

bool EventFd::EnableEvent() {
  return ioctl(perf_event_fd, PERF_EVENT_IOC_ENABLE, 0) == 0;
}

bool EventFd::DisableEvent() {
  return ioctl(perf_event_fd, PERF_EVENT_IOC_DISABLE, 0) == 0;
}
