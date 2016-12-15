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

#include <gtest/gtest.h>

#include <signal.h>

#include <android-base/test_utils.h>

#include "IOEventLoop.h"
#include "utils.h"
#include "workload.h"

TEST(workload, success) {
  IOEventLoop loop;
  ASSERT_TRUE(loop.AddSignalEvent(SIGCHLD, [&]() {
    return loop.ExitLoop();
  }));
  auto workload = Workload::CreateWorkload({"sleep", "1"});
  ASSERT_TRUE(workload != nullptr);
  ASSERT_TRUE(workload->GetPid() != 0);
  ASSERT_TRUE(workload->Start());
  ASSERT_TRUE(loop.RunLoop());
}

TEST(workload, execvp_failure) {
  auto workload = Workload::CreateWorkload({"/dev/null"});
  ASSERT_TRUE(workload != nullptr);
  ASSERT_FALSE(workload->Start());
}

TEST(workload, signaled_warning) {
  CapturedStderr cap;
  {
    IOEventLoop loop;
    ASSERT_TRUE(loop.AddSignalEvent(SIGCHLD, [&]() {
      return loop.ExitLoop();
    }));
    auto workload = Workload::CreateWorkload({"sleep", "10"});
    ASSERT_TRUE(workload != nullptr);
    ASSERT_TRUE(workload->Start());
    ASSERT_EQ(0, kill(workload->GetPid(), SIGKILL));
    ASSERT_TRUE(loop.RunLoop());
    // Make sure the destructor of workload is called.
  }
  cap.reset();
  ASSERT_NE(cap.output().find("child process was terminated by signal"), std::string::npos);
}

TEST(workload, exit_nonzero_warning) {
  CapturedStderr cap;
  {
    IOEventLoop loop;
    ASSERT_TRUE(loop.AddSignalEvent(SIGCHLD, [&]() {
      return loop.ExitLoop();
    }));
    auto workload = Workload::CreateWorkload({"ls", "nonexistdir"});
    ASSERT_TRUE(workload != nullptr);
    ASSERT_TRUE(workload->Start());
    ASSERT_TRUE(loop.RunLoop());
    // Make sure the destructor of workload is called.
  }
  cap.reset();
  ASSERT_NE(cap.output().find("child process exited with exit code"), std::string::npos);
}
