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

#include "event_selection_set.h"

#include <base/logging.h>

#include "environment.h"
#include "event_attr.h"
#include "event_type.h"

// EventSelection helps to monitor one event type.
class EventSelection {
 public:
  EventSelection() {
  }

  EventSelection(const EventType& event_type)
      : event_type_(&event_type), event_attr_(CreateDefaultPerfEventAttr(event_type)) {
  }

  const EventType* Type() const {
    return event_type_;
  }

  const perf_event_attr& Attr() const {
    return event_attr_;
  }

  const std::vector<std::unique_ptr<EventFd>>& EventFds() const {
    return event_fds_;
  }

  void EnableOnExec();
  void SampleIdAll();
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

void EventSelection::EnableOnExec() {
  event_attr_.enable_on_exec = 1;
}

void EventSelection::SampleIdAll() {
  event_attr_.sample_id_all = 1;
}

void EventSelection::SetSampleFreq(uint64_t sample_freq) {
  event_attr_.freq = 1;
  event_attr_.sample_freq = sample_freq;
}

void EventSelection::SetSamplePeriod(uint64_t sample_period) {
  event_attr_.freq = 0;
  event_attr_.sample_period = sample_period;
}

bool EventSelection::OpenEventFilesForAllCpus() {
  std::vector<int> cpus = GetOnlineCpus();
  if (cpus.empty()) {
    return false;
  }
  event_fds_.clear();
  for (auto& cpu : cpus) {
    auto event_fd = EventFd::OpenEventFileForCpu(event_attr_, cpu);
    if (event_fd != nullptr) {
      event_fds_.push_back(std::move(event_fd));
    }
  }
  // As the online cpus can be enabled or disabled at runtime, we may not open event file for
  // all cpus successfully. But we should open at least one cpu successfully.
  if (event_fds_.empty()) {
    LOG(ERROR) << "failed to open perf event file for event_type " << event_type_->name
               << " on all cpus";
    return false;
  }
  return true;
}

bool EventSelection::OpenEventFilesForProcess(pid_t pid) {
  auto event_fd = EventFd::OpenEventFileForProcess(event_attr_, pid);
  if (event_fd == nullptr) {
    PLOG(ERROR) << "failed to open perf event file for event type " << event_type_->name
                << " on pid " << pid;
    return false;
  }
  event_fds_.clear();
  event_fds_.push_back(std::move(event_fd));
  return true;
}

bool EventSelection::EnableEvents() {
  for (auto& event_fd : event_fds_) {
    if (!event_fd->EnableEvent()) {
      return false;
    }
  }
  return true;
}

bool EventSelection::ReadCounters(std::vector<PerfCounter>* counters) {
  for (auto& event_fd : event_fds_) {
    PerfCounter counter;
    if (!event_fd->ReadCounter(&counter)) {
      return false;
    }
    counters->push_back(counter);
  }
  return true;
}

void EventSelection::PreparePollForEventFiles(std::vector<pollfd>* pollfds) {
  for (auto& event_fd : event_fds_) {
    pollfd poll_fd;
    event_fd->PreparePollForMmapData(&poll_fd);
    pollfds->push_back(poll_fd);
  }
}

bool EventSelection::MmapEventFiles(size_t mmap_pages) {
  for (auto& event_fd : event_fds_) {
    if (!event_fd->MmapContent(mmap_pages)) {
      return false;
    }
  }
  return true;
}

static bool ReadMmapEventDataForFd(std::unique_ptr<EventFd>& event_fd,
                                   std::function<bool(const char*, size_t)> callback,
                                   bool* have_data) {
  while (true) {
    char* data;
    size_t size = event_fd->GetAvailableMmapData(&data);
    if (size == 0) {
      break;
    }
    if (!callback(data, size)) {
      return false;
    }
    *have_data = true;
    event_fd->DiscardMmapData(size);
  }
  return true;
}

bool EventSelection::ReadMmapEventData(std::function<bool(const char*, size_t)> callback) {
  while (true) {
    bool have_data = false;
    for (auto& event_fd : event_fds_) {
      if (!ReadMmapEventDataForFd(event_fd, callback, &have_data)) {
        return false;
      }
    }
    if (!have_data) {
      return true;
    }
  }
}

std::string EventSelection::FindEventFileNameById(uint64_t id) {
  for (auto& event_fd : event_fds_) {
    if (event_fd->Id() == id) {
      return event_fd->Name();
    }
  }
  return "";
}

EventSelectionSet::~EventSelectionSet() {
  for (auto& pair : map_) {
    delete pair.second;
  }
}

void EventSelectionSet::AddEventType(const EventType& event_type) {
  map_.insert(std::make_pair(event_type.name, new EventSelection(event_type)));
}

void EventSelectionSet::EnableOnExec() {
  for (auto& pair : map_) {
    pair.second->EnableOnExec();
  }
}

bool EventSelectionSet::OpenEventFilesForAllCpus() {
  for (auto& pair : map_) {
    if (!pair.second->OpenEventFilesForAllCpus()) {
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::OpenEventFilesForProcess(pid_t pid) {
  for (auto& pair : map_) {
    if (!pair.second->OpenEventFilesForProcess(pid)) {
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::EnableEvents() {
  for (auto& pair : map_) {
    if (!pair.second->EnableEvents()) {
      return false;
    }
  }
  return true;
}

void EventSelectionSet::SampleIdAll() {
  for (auto& pair : map_) {
    pair.second->SampleIdAll();
  }
}

void EventSelectionSet::SetSampleFreq(uint64_t sample_freq) {
  for (auto& pair : map_) {
    pair.second->SetSampleFreq(sample_freq);
  }
}

void EventSelectionSet::SetSamplePeriod(uint64_t sample_period) {
  for (auto& pair : map_) {
    pair.second->SetSamplePeriod(sample_period);
  }
}

bool EventSelectionSet::ReadCounters(
    std::map<const EventType*, std::vector<PerfCounter>>* counters_map) {
  counters_map->clear();
  for (auto& pair : map_) {
    std::vector<PerfCounter> counters;
    if (!pair.second->ReadCounters(&counters)) {
      return false;
    }
    counters_map->insert(std::make_pair(pair.second->Type(), counters));
  }
  return true;
}

void EventSelectionSet::PreparePollForEventFiles(std::vector<pollfd>* pollfds) {
  for (auto& pair : map_) {
    pair.second->PreparePollForEventFiles(pollfds);
  }
}

bool EventSelectionSet::MmapEventFiles(size_t mmap_pages) {
  for (auto& pair : map_) {
    if (!pair.second->MmapEventFiles(mmap_pages)) {
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::ReadMmapEventData(std::function<bool(const char*, size_t)> callback) {
  for (auto& pair : map_) {
    if (!pair.second->ReadMmapEventData(callback)) {
      return false;
    }
  }
  return true;
}

std::string EventSelectionSet::FindEventFileNameById(uint64_t id) {
  for (auto& pair : map_) {
    std::string result = pair.second->FindEventFileNameById(id);
    if (!result.empty()) {
      return result;
    }
  }
  return "";
}

const perf_event_attr& EventSelectionSet::FindEventAttrByType(const EventType& event_type) {
  return map_[event_type.name]->Attr();
}

const std::vector<std::unique_ptr<EventFd>>& EventSelectionSet::FindEventFdsByType(
    const EventType& event_type) {
  return map_[event_type.name]->EventFds();
}
