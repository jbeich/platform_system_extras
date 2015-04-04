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

  bool GetEventId(uint64_t& id);

  bool EnableEvent();
  bool DisableEvent();

  const Event& GetEvent() const {
    return attr.GetEvent();
  }

  const EventAttr& GetAttr() const {
    return attr;
  }

  int Fd() const {
    return perf_event_fd;
  }

 private:
  EventFd(const EventAttr& attr, int perf_event_fd)
    : attr(attr), perf_event_fd(perf_event_fd), event_id(-1) {
  }

 private:
  const EventAttr attr;
  int perf_event_fd;
  uint64_t event_id;
};

#endif  // SIMPLE_PERF_EVENT_FD_H_
