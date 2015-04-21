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

#include <chrono>

#include <command.h>

class StatCommandTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    stat_cmd = Command::FindCommandByName("stat");
    ASSERT_TRUE(stat_cmd != nullptr);
  }

 protected:
  Command* stat_cmd;
};

TEST_F(StatCommandTest, no_options) {
  ASSERT_TRUE(stat_cmd->Run({}));
}

TEST_F(StatCommandTest, event_option) {
  ASSERT_TRUE(stat_cmd->Run({"-e", "cpu-clock,task-clock"}));
}

TEST_F(StatCommandTest, system_wide_option) {
  ASSERT_TRUE(stat_cmd->Run({"-a"}));
}

TEST_F(StatCommandTest, sleep_duration_option) {
  auto start_time = std::chrono::steady_clock::now();
  ASSERT_TRUE(stat_cmd->Run({"-s", "2"}));
  auto end_time = std::chrono::steady_clock::now();
  ASSERT_TRUE(end_time >= start_time + std::chrono::seconds(2));
}

TEST_F(StatCommandTest, verbose_option) {
  ASSERT_TRUE(stat_cmd->Run({"--verbose"}));
}

TEST_F(StatCommandTest, help_option) {
  ASSERT_TRUE(stat_cmd->Run({"--help"}));
}
