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

#ifndef SIMPLE_PERF_EVENT_H_
#define SIMPLE_PERF_EVENT_H_

#include <stdint.h>
#include <string>
#include <vector>

// Event represents one event, like cpu_cycle_event, which is actually one type of concrete events.
// As far as we don't represent one concrete event outside the kernel, we don't need to rename
// Event to EventType.
// The user knows one event by its name, and the kernel knows one event by its event_type and
// type_specific_config. Event connects the two representations, and tells if the event
// is supported by the kernel.
class Event {
 private:
  enum class SupportState {
    NotYetChecked,
    Supported,
    Unsupported,
  };

 public:
  Event(const std::string& name, uint32_t type, uint64_t type_specific_config)
      : name_(name),
        type_(type),
        type_specific_config_(type_specific_config),
        support_state_(SupportState::NotYetChecked) {
  }

  virtual ~Event() {
  }

  const std::string& Name() const {
    return name_;
  }

  uint32_t Type() const {
    return type_;
  }

  uint64_t TypeSpecificConfig() const {
    return type_specific_config_;
  }

  bool IsSupported() const;

  static const std::vector<const Event*>& HardwareEvents();
  static const std::vector<const Event*>& SoftwareEvents();
  static const std::vector<const Event*>& HwcacheEvents();

  static const Event* FindEventByName(const std::string& name);
  static const Event* FindEventByTypeAndConfig(uint32_t type, uint64_t type_specific_config);

 private:
  bool CheckSupport() const;

  static std::vector<const Event*> GetAllEvents();

  const std::string name_;
  const uint32_t type_;
  const uint64_t type_specific_config_;
  mutable SupportState support_state_;
};

#endif  // SIMPLE_PERF_EVENT_H_
