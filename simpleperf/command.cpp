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

#include "command.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

static std::vector<Command*>& Commands() {
  static std::vector<Command*> commands;
  return commands;
}

void CommandCollection::RegisterCommand(Command& command) {
  Commands().push_back(&command);
}

Command* CommandCollection::FindCommand(const std::string& cmd_name) {
  return FindCommand(cmd_name.c_str());
}

Command* CommandCollection::FindCommand(const char* cmd_name) {
  for (auto command : Commands()) {
    if (!strcmp(command->Name(), cmd_name)) {
      return command;
    }
  }
  return nullptr;
}

static bool CompareCommands(Command* cmd1, Command* cmd2) {
  return strcmp(cmd1->Name(), cmd2->Name()) < 0;
}

const std::vector<Command*>& CommandCollection::AllCommands() {
  std::sort(Commands().begin(), Commands().end(), CompareCommands);
  return Commands();
}

