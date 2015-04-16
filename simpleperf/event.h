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

class Event {
 private:
  enum class SupportState {
    NotYetChecked,
    Supported,
    Unsupported,
  };

 public:
  Event(const std::string& name, uint32_t type, uint64_t config)
      : name(name), type(type), config(config), support_state(SupportState::NotYetChecked) {
  }

  virtual ~Event() {
  }

  const std::string& Name() const {
    return name;
  };

  uint32_t Type() const {
    return type;
  }

  uint64_t Config() const {
    return config;
  }

  bool Supported() const;

  static const std::vector<const Event*>& HardwareEvents();
  static const std::vector<const Event*>& SoftwareEvents();
  static const std::vector<const Event*>& HwcacheEvents();

  static const Event* FindEventByName(const std::string& name);
  static const Event* FindEventByConfig(uint32_t type, uint64_t config);

 private:
  bool CheckSupport() const;

  static std::vector<const Event*> GetAllEvents();

  const std::string name;
  const uint32_t type;
  const uint64_t config;
  mutable SupportState support_state;
};

#endif  // SIMPLE_PERF_EVENT_H_
