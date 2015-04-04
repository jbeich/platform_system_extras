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

#ifndef SIMPLE_PERF_COMMAND_H_
#define SIMPLE_PERF_COMMAND_H_

#include <string>
#include <vector>

class Command;

class CommandCollection {
 public:
  static void RegisterCommand(Command& command);

  static Command* FindCommand(const std::string& cmd_name);
  static Command* FindCommand(const char* cmd_name);

  static const std::vector<Command*>& AllCommands();
};

class Command {
 public:
  Command(const std::string& name, const std::string& short_help_info,
          const std::string& detailed_help_info)
      :name(name), short_help_info(short_help_info), detailed_help_info(detailed_help_info) {
    CommandCollection::RegisterCommand(*this);
  }

  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

  virtual ~Command() {}

  const char* Name() const { return name.c_str(); }
  const char* ShortHelpInfo() const { return short_help_info.c_str(); }
  const char* DetailedHelpInfo() const { return detailed_help_info.c_str(); }

  virtual bool RunCommand(std::vector<std::string>& args) = 0;

 private:
  const std::string name;
  const std::string short_help_info;
  const std::string detailed_help_info;
};

#endif  // SIMPLE_PERF_COMMAND_H_
