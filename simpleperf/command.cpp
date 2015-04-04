/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

static bool CompareCommands(Command* cmd1, Command* cmd2) {
  return strcmp(cmd1->Name(), cmd2->Name()) < 0;
}

void CommandCollection::RegisterCommand(Command& command) {
  Commands().push_back(&command);
  std::sort(Commands().begin(), Commands().end(), CompareCommands);
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

const std::vector<Command*>& CommandCollection::AllCommands() {
  return Commands();
}

