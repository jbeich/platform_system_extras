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

#include "event.h"

#include <string.h>
#include <unistd.h>
#include <vector>
#include "event_attr.h"
#include "event_fd.h"
#include "event_table.h"

extern std::vector<const Event*> hardware_events;
extern std::vector<const Event*> software_events;
extern std::vector<const Event*> hwcache_events;

const std::vector<const Event*>& Event::HardwareEvents() {
  return hardware_events;
}

const std::vector<const Event*>& Event::SoftwareEvents() {
  return software_events;
}

const std::vector<const Event*>& Event::HwcacheEvents() {
  return hwcache_events;
}

const Event* Event::FindEventByName(const char* name) {
  std::vector<std::vector<const Event*>*> events_list {&hardware_events,
                                                       &software_events,
                                                       &hwcache_events,
                                                      };

  for (auto events : events_list) {
    for (auto event : *events) {
      if (!strcmp(event->Name(), name)) {
        return event;
      }
    }
  }
  return nullptr;
}

const Event* Event::FindEventByConfig(uint32_t type, uint64_t config) {
  std::vector<std::vector<const Event*>*> events_list {&hardware_events,
                                                       &software_events,
                                                       &hwcache_events,
                                                      };

  for (auto events : events_list) {
    for (auto event : *events) {
      if (event->Type() == type && event->Config() == config) {
        return event;
      }
    }
  }
  return nullptr;
}

bool Event::Supported() const {
  if (support_state == SupportState::NotYetChecked) {
    support_state = (CheckSupport() == true) ? SupportState::Supported : SupportState::Unsupported;
  }
  return support_state == SupportState::Supported;
}

bool Event::CheckSupport() const {
  EventAttr attr(this, false);
  auto event_fd = EventFd::OpenEventFileForProcess(attr, getpid());
  return event_fd != nullptr;
}
