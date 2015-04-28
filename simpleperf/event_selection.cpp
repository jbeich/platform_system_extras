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

#include "event_selection.h"

#include <base/logging.h>

#include "environment.h"
#include "event_attr.h"
#include "event_type.h"

EventSelection::EventSelection(const EventType& event_type)
    : event_type_(&event_type), event_attr_(CreateDefaultPerfEventAttr(event_type)) {
}

void EventSelection::SampleIdAll() {
  event_attr_.sample_id_all = 1;
}

void EventSelection::EnableOnExec() {
  event_attr_.enable_on_exec = 1;
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
  counters->clear();
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
  pollfds->clear();
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

void EventSelectionList::AddEventType(const EventType& event_type) {
  event_selections_.push_back(EventSelection(event_type));
}

void EventSelectionList::EnableOnExec() {
  for (auto& selection : event_selections_) {
    selection.EnableOnExec();
  }
}

bool EventSelectionList::OpenEventFilesForAllCpus() {
  for (auto& selection : event_selections_) {
    if (!selection.OpenEventFilesForAllCpus()) {
      return false;
    }
  }
  return true;
}

bool EventSelectionList::OpenEventFilesForProcess(pid_t pid) {
  for (auto& selection : event_selections_) {
    if (!selection.OpenEventFilesForProcess(pid)) {
      return false;
    }
  }
  return true;
}

bool EventSelectionList::EnableEvents() {
  for (auto& selection : event_selections_) {
    if (!selection.EnableEvents()) {
      return false;
    }
  }
  return true;
}

bool EventSelectionList::ReadCounters(
    std::map<const EventType*, std::vector<PerfCounter>>* counters_map) {
  counters_map->clear();
  for (auto& selection : event_selections_) {
    std::vector<PerfCounter> counters;
    if (!selection.ReadCounters(&counters)) {
      return false;
    }
    counters_map->insert(
        std::pair<const EventType*, std::vector<PerfCounter>>(&selection.Type(), counters));
  }
  return true;
}

std::string EventSelectionList::FindEventFileNameById(uint64_t id) {
  for (auto& selection : event_selections_) {
    std::string result = selection.FindEventFileNameById(id);
    if (!result.empty()) {
      return result;
    }
  }
  return "";
}
