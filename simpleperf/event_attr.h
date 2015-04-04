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

#ifndef SIMPLE_PERF_EVENT_ATTR_H_
#define SIMPLE_PERF_EVENT_ATTR_H_

#include <stddef.h>
#include <stdint.h>

#include "perf_event.h"

class Event;

class EventAttr {
 public:
  EventAttr(const Event* event, bool enabled = true);
  EventAttr(const perf_event_attr* attr);

  perf_event_attr* Attr() {
    return &attr;
  }

  const perf_event_attr* Attr() const {
    return &attr;
  }

  uint64_t SampleType() const {
    return attr.sample_type;
  }

  void EnableOnExec() {
    attr.enable_on_exec = 1;
  }

  void SetSampleFreq(uint64_t freq) {
    attr.freq = 1;
    attr.sample_freq = freq;
  }

  void SetSamplePeriod(uint64_t period) {
    attr.freq = 0;
    attr.sample_period = period;
  }

  void SetSampleAll() {
    attr.sample_id_all = 1;
  }

  bool GetSampleAll() const {
    return attr.sample_id_all == 1;
  }

  const Event& GetEvent() const {
    return *event;
  }

  void Print(int space = 0) const;

 private:
  void Init(uint32_t type, uint64_t config, bool enabled);

  perf_event_attr attr;
  const Event* event;
};

#endif  // SIMPLE_PERF_EVENT_ATTR_H_
