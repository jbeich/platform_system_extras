/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
