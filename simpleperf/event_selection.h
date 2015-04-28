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

#ifndef SIMPLE_PERF_EVENT_SELECTION_H_
#define SIMPLE_PERF_EVENT_SELECTION_H_

#include <functional>
#include <map>
#include <vector>

#include <base/macros.h>

#include "event_fd.h"
#include "perf_event.h"

struct EventType;

// EventSelection helps to monitor one event type.
// Firstly, The user notifies the EventSelection about which event type to monitor, and how to
// monitor it (by setting enable_on_exec flag, sample frequency, etc).
// Then the user can start monitoring by ordering the EventSelection to open perf event files and
// enable events (if enable_on_exec flag isn't used).
// After that, the user can read counters or read mapped event records.
// At last, the EventSelection will clean up resources at destruction automatically.
class EventSelection {
 public:
  EventSelection() {
  }

  EventSelection(const EventType& event_type);

  EventSelection(EventSelection&&) = default;
  EventSelection& operator=(EventSelection&&) = default;

  const EventType& Type() const {
    return *event_type_;
  }
  const perf_event_attr& Attr() const {
    return event_attr_;
  }
  const std::vector<std::unique_ptr<EventFd>>& EventFds() const {
    return event_fds_;
  }

  void SampleIdAll();
  void EnableOnExec();
  void SetSampleFreq(uint64_t sample_freq);
  void SetSamplePeriod(uint64_t sample_period);

  bool OpenEventFilesForAllCpus();
  bool OpenEventFilesForProcess(pid_t pid);
  bool EnableEvents();
  bool ReadCounters(std::vector<PerfCounter>* counters);

  void PreparePollForEventFiles(std::vector<pollfd>* pollfds);
  bool MmapEventFiles(size_t mmap_pages);
  bool ReadMmapEventData(std::function<bool(const char*, size_t)> callback);

  std::string FindEventFileNameById(uint64_t id);

 private:
  const EventType* event_type_;
  perf_event_attr event_attr_;
  std::vector<std::unique_ptr<EventFd>> event_fds_;

  DISALLOW_COPY_AND_ASSIGN(EventSelection);
};

// EventSelectionList is a collection of EventSelection. Through it the user can monitor multiple
// event types at the same time.
class EventSelectionList {
 public:
  EventSelectionList() {
  }

  bool Empty() const {
    return event_selections_.empty();
  }

  void AddEventType(const EventType& event_type);
  void EnableOnExec();

  bool OpenEventFilesForAllCpus();
  bool OpenEventFilesForProcess(pid_t pid);
  bool EnableEvents();
  bool ReadCounters(std::map<const EventType*, std::vector<PerfCounter>>* counters_map);

  std::string FindEventFileNameById(uint64_t id);

 private:
  std::vector<EventSelection> event_selections_;

  DISALLOW_COPY_AND_ASSIGN(EventSelectionList);
};

#endif  // SIMPLE_PERF_EVENT_SELECTION_H_
