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
  Event(const char* name, uint32_t type, uint64_t config)
  		: name(name), type(type), config(config), support_state(SupportState::NotYetChecked) { }

  virtual ~Event() {}

  const char* Name() const {
    return name.c_str();
  };

  uint32_t Type() const {
    return type;
  }

  uint64_t Config() const {
    return config;
  }

  bool Supported() const;

  static const Event* FindEventByName(const std::string& name) {
    return FindEventByName(name.c_str());
  }

  static const Event* FindEventByName(const char* name);
  static const Event* FindEventByConfig(uint32_t type, uint64_t config);

  static const std::vector<const Event*>& HardwareEvents();
  static const std::vector<const Event*>& SoftwareEvents();
  static const std::vector<const Event*>& HwcacheEvents();

 private:
  bool CheckSupport() const;

  const std::string name;
  const uint32_t type;
  const uint64_t config;
  mutable SupportState support_state;
};

#endif  // SIMPLE_PERF_EVENT_H_
