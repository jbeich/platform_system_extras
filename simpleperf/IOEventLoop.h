/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef SIMPLE_PERF_IOEVENT_LOOP_H_
#define SIMPLE_PERF_IOEVENT_LOOP_H_

#include <time.h>

#include <functional>
#include <memory>
#include <vector>

struct IOEvent;
struct event_base;

// IOEventLoop is a class wrapper of libevent, it monitors events happened,
// and calls the corresponding callbacks. Possible events are: file ready to
// read, file ready to write, signal happens, periodic timer timeout.
class IOEventLoop {
 public:
  IOEventLoop();
  ~IOEventLoop();

  // Register a read Event, so [callback] is called when [fd] can be read
  // without blocking.
  bool AddReadEvent(int fd, const std::function<bool()>& callback);

  // Register a signal Event, so [callback] is called each time signal [sig]
  // happens.
  bool AddSignalEvent(int sig, const std::function<bool()>& callback);

  // Register a vector of signal Events.
  bool AddSignalEvents(std::vector<int> sigs,
                       const std::function<bool()>& callback);

  // Register a periodic Event, so [callback] is called periodically every
  // [duration].
  bool AddPeriodicEvent(timeval duration,
                        const std::function<bool()>& callback);

  // Run a loop polling for Events. It only exits when ExitLoop() is called
  // in a callback function of registered Events.
  bool RunLoop();

  // Exit the loop started by RunLoop().
  bool ExitLoop();

 private:
  bool EnsureInit();
  bool AddEvent(int fd_or_sig, short events, timeval* timeout,
                const std::function<bool()>& callback);
  static void EventCallbackFn(int, short, void*);

  event_base* ebase_;
  std::vector<std::unique_ptr<IOEvent>> events_;
  bool has_error_;
};

#endif  // SIMPLE_PERF_IOEVENT_LOOP_H_
