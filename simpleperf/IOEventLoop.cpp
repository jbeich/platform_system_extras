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

#include "IOEventLoop.h"

#include <event2/event.h>
#include <fcntl.h>

#include <android-base/logging.h>

struct IOEvent {
  IOEventLoop* loop;
  event* e;
  std::function<bool()> callback;

  IOEvent(IOEventLoop* loop, std::function<bool()> callback)
      : loop(loop), e(nullptr), callback(callback) {}

  ~IOEvent() {
    if (e != nullptr) {
      event_free(e);
    }
  }
};

IOEventLoop::IOEventLoop() : ebase_(nullptr), has_error_(false) {}

IOEventLoop::~IOEventLoop() {
  if (ebase_ != nullptr) {
    event_base_free(ebase_);
  }
}

bool IOEventLoop::EnsureInit() {
  if (ebase_ == nullptr) {
    ebase_ = event_base_new();
    if (ebase_ == nullptr) {
      LOG(ERROR) << "failed to call event_base_new()";
      return false;
    }
  }
  return true;
}

void IOEventLoop::EventCallbackFn(int, short, void* arg) {
  IOEvent* e = static_cast<IOEvent*>(arg);
  if (!e->callback()) {
    e->loop->has_error_ = true;
    e->loop->ExitLoop();
  }
}

bool IOEventLoop::AddSignalEvent(int sig, std::function<bool()> callback) {
  return AddEvent(sig, EV_SIGNAL | EV_PERSIST, nullptr, callback);
}

bool IOEventLoop::AddSignalEvents(std::vector<int> sigs,
                                  std::function<bool()> callback) {
  for (auto sig : sigs) {
    if (!AddSignalEvent(sig, callback)) {
      return false;
    }
  }
  return true;
}

bool IOEventLoop::AddTimeEvent(timeval duration,
                               std::function<bool()> callback) {
  return AddEvent(-1, EV_PERSIST, &duration, callback);
}

bool IOEventLoop::AddEvent(int fd_or_sig, short events, timeval* timeout,
                           std::function<bool()> callback) {
  if (!EnsureInit()) {
    return false;
  }
  events_.push_back(std::unique_ptr<IOEvent>(new IOEvent(this, callback)));
  IOEvent* e = events_.back().get();
  e->e = event_new(ebase_, fd_or_sig, events, EventCallbackFn, e);
  if (e->e == nullptr) {
    events_.pop_back();
    LOG(ERROR) << "failed to call event_new()";
    return false;
  }
  if (event_add(e->e, timeout) != 0) {
    LOG(ERROR) << "failed to call event_add()";
    events_.pop_back();
    return false;
  }
  return true;
}

bool IOEventLoop::RunLoop() {
  if (event_base_dispatch(ebase_) == -1) {
    LOG(ERROR) << "fails to call event_base_dispatch()";
    return false;
  }
  if (has_error_) {
    return false;
  }
  return true;
}

bool IOEventLoop::ExitLoop() {
  if (event_base_loopbreak(ebase_) == -1) {
    LOG(ERROR) << "fails to call event_base_loopbreak()";
    return false;
  }
  return true;
}
