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

#include "event.h"

#include <unistd.h>
#include <string>
#include <vector>
#include "event_attr.h"
#include "event_fd.h"
#include "event_table.h"

const std::vector<const Event*>& Event::HardwareEvents() {
  return hardware_events;
}

const std::vector<const Event*>& Event::SoftwareEvents() {
  return software_events;
}

const std::vector<const Event*>& Event::HwcacheEvents() {
  return hwcache_events;
}

bool Event::Supported() const {
  if (support_state == SupportState::NotYetChecked) {
    support_state = (CheckSupport() == true) ? SupportState::Supported : SupportState::Unsupported;
  }
  return support_state == SupportState::Supported;
}

bool Event::CheckSupport() const {
  EventAttr attr(this);
  auto event_fd = EventFd::OpenEventFileForProcess(attr, getpid());
  return event_fd != nullptr;
}

const Event* Event::FindEventByName(const std::string& name) {
  for (auto event : GetAllEvents()) {
    if (event->Name() == name) {
      return event;
    }
  }
  return nullptr;
}

const Event* Event::FindEventByConfig(uint32_t type, uint64_t config) {
  for (auto event : GetAllEvents()) {
    if (event->Type() == type && event->Config() == config) {
      return event;
    }
  }
  return nullptr;
}

std::vector<const Event*> Event::GetAllEvents() {
  std::vector<const Event*> result;
  for (auto event : HardwareEvents()) {
    result.push_back(event);
  }
  for (auto event : SoftwareEvents()) {
    result.push_back(event);
  }
  for (auto event : HwcacheEvents()) {
    result.push_back(event);
  }
  return result;
}
