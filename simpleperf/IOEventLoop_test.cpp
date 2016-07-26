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

#include "IOEventLoop.h"

#include <gtest/gtest.h>

#include <thread>

TEST(IOEventLoop, signal) {
  IOEventLoop loop;
  static int count;
  count = 0;
  ASSERT_TRUE(loop.AddSignalEvent(SIGINT, [&]() {
    if (++count == 100) {
      loop.ExitLoop();
    }
    return true;
  }));
  std::thread thread([]() {
    for (int i = 0; i < 100; ++i) {
      usleep(1000);
      kill(getpid(), SIGINT);
    }
  });
  ASSERT_TRUE(loop.RunLoop());
  thread.join();
  ASSERT_EQ(100, count);
}

TEST(IOEventLoop, time) {
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  static int count;
  count = 0;
  IOEventLoop loop;
  ASSERT_TRUE(loop.AddTimeEvent(tv, [&]() {
    if (++count == 100) {
      loop.ExitLoop();
    }
    return true;
  }));
  ASSERT_TRUE(loop.RunLoop());
  ASSERT_EQ(100, count);
}
