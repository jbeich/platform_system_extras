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

class MockCommand : public Command {
 public:
  MockCommand() : Command("mock", "mock_short_help", "mock_long_help") {
  }

  bool Run(const std::vector<std::string>&) override {
    return true;
  }
};

TEST(command, CreateCommandInstance) {
  ASSERT_TRUE(CreateCommandInstance("mock1") == nullptr);
  RegisterCommand("mock1", [] { return std::unique_ptr<Command>(new MockCommand); });
  ASSERT_TRUE(CreateCommandInstance("mock1") != nullptr);
  UnRegisterCommand("mock1");
  ASSERT_TRUE(CreateCommandInstance("mock1") == nullptr);
}

TEST(command, GetAllCommands) {
  size_t command_count = GetAllCommandNames().size();
  RegisterCommand("mock1", [] { return std::unique_ptr<Command>(new MockCommand); });
  ASSERT_EQ(command_count + 1, GetAllCommandNames().size());
  UnRegisterCommand("mock1");
  ASSERT_EQ(command_count, GetAllCommandNames().size());
}

TEST(command, GetValueForOption) {
  MockCommand command;
  uint64_t value;
  size_t i;
  for (bool allow_suffixes : {true, false}) {
    i = 0;
    ASSERT_TRUE(command.GetValueForOption({"-s", "156"}, &i, &value, 0, allow_suffixes));
    ASSERT_EQ(i, 1u);
    ASSERT_EQ(value, 156u);
  }
  i = 0;
  ASSERT_TRUE(command.GetValueForOption({"-s", "156k"}, &i, &value, 0, true));
  ASSERT_EQ(value, 156 * (1ULL << 10));
  i = 0;
  ASSERT_FALSE(command.GetValueForOption({"-s"}, &i, &value, 0, true));
  i = 0;
  ASSERT_FALSE(command.GetValueForOption({"-s", "0"}, &i, &value, 1, true));
}
