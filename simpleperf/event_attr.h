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
