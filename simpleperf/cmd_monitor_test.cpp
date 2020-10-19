/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#if defined(__ANDROID__)
#include <android-base/properties.h>
#endif

#include <map>
#include <memory>
#include <thread>

#include "command.h"
#include "environment.h"
#include "event_selection_set.h"
#include "get_test_data.h"
#include "record.h"
#include "record_file.h"
#include "test_util.h"
#include "thread_tree.h"

using android::base::Realpath;
using android::base::StringPrintf;
using namespace simpleperf;
using namespace PerfFileFormat;

static std::unique_ptr<Command> MonitorCmd() {
  return CreateCommandInstance("monitor");
}

static const char* GetDefaultEvent() {
  return HasHardwareCounter() ? "cpu-cycles" : "task-clock";
}

static ::testing::AssertionResult RunMonitorCmd(std::vector<std::string> v, std::string& output) {
  bool has_event = false;
  for (auto& arg : v) {
    if (arg == "-e") {
      has_event = true;
      break;
    }
  }
  if (!has_event) {
    v.insert(v.end(), {"-e", GetDefaultEvent()});
  }

  v.insert(v.end(), {"--duration", SLEEP_SEC});

  CaptureStdout capture;
  if (!capture.Start()) {
    return ::testing::AssertionFailure() << "Unable to capture stdout";
  }
  auto result = MonitorCmd()->Run(v);
  output.append(capture.Finish());
  return (result ? ::testing::AssertionSuccess() : ::testing::AssertionFailure());
}

TEST(monitor_cmd, no_options) {
  std::string output;
  ASSERT_FALSE(RunMonitorCmd({}, output));
}

TEST(monitor_cmd, global_no_root) {
  std::string output;
  ASSERT_TRUE(RunMonitorCmd({"-a"}, output));
}

TEST(monitor_cmd, global_root) {
  TEST_REQUIRE_ROOT();
  std::string output;
  ASSERT_TRUE(RunMonitorCmd({"-a"}, output));
  ASSERT_GT(output.size(), 0);
}

TEST(monitor_cmd, with_callchain) {
  TEST_REQUIRE_ROOT();
  std::string output;
  ASSERT_TRUE(RunMonitorCmd({"-a", "-g"}, output));
  ASSERT_GT(output.size(), 0);
}

TEST(monitor_cmd, count) {
  TEST_REQUIRE_ROOT();
  std::string output;

  // The default event is record for SLEEP_SEC (0.001s).
  ASSERT_TRUE(RunMonitorCmd({"-a", "-f", "1"}, output));
  auto small_sample_size = android::base::Split(output, "\n").size() - 1;
  ASSERT_LT(small_sample_size, 1);
  output.clear();

  ASSERT_TRUE(RunMonitorCmd({"-a", "-f", "4000"}, output));
  auto large_sample_size = android::base::Split(output, "\n").size() - 1;
  ASSERT_GT(large_sample_size, 1);
  ASSERT_GT(large_sample_size, small_sample_size);
}
