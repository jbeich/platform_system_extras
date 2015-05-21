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

#include "command.h"

class ReportCommandTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    Command* record_cmd = Command::FindCommandByName("record");
    ASSERT_TRUE(record_cmd != nullptr);
    ASSERT_TRUE(record_cmd->Run({"record", "-a", "sleep", "1"}));
    ASSERT_TRUE(record_cmd->Run({"record", "-a", "-o", "perf2.data", "sleep", "1"}));
  }

  virtual void SetUp() {
    report_cmd = Command::FindCommandByName("report");
    ASSERT_TRUE(report_cmd != nullptr);
  }

  Command* report_cmd;
};

TEST_F(ReportCommandTest, no_options) {
  ASSERT_TRUE(report_cmd->Run({"report"}));
}

TEST_F(ReportCommandTest, input_file_option) {
  ASSERT_TRUE(report_cmd->Run({"report", "-i", "perf2.data"}));
}

TEST_F(ReportCommandTest, sort_option_pid) {
  ASSERT_TRUE(report_cmd->Run({"report", "--sort", "pid"}));
}

TEST_F(ReportCommandTest, sort_option_all) {
  ASSERT_TRUE(report_cmd->Run({"report", "--sort", "comm,pid,dso"}));
}
