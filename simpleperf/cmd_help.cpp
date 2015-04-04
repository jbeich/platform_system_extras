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

class HelpCmd : public Command {
 public:
  HelpCmd()
      : Command("help",
                "print help information for simpleperf",
                "Usage: simpleperf help [subcommand]\n"
                "    Without subcommand, print brief help information for every subcommand.\n"
                "    With subcommand, print detailed help information for the subcommand.\n\n"
                ) { }

  bool RunCommand(std::vector<std::string>& args) override {
    if (args.size() == 0) {
      PrintShortHelp();
    } else {
      Command* need_help_command = CommandCollection::FindCommand(args[0]);
      if (need_help_command == nullptr) {
        PrintShortHelp();
        return false;
      } else {
        PrintDetailedHelp(*need_help_command);
      }
    }
    return true;
  }

  void PrintShortHelp() {
    printf("Usage: simpleperf [--help] subcommand [args_for_subcommand]\n\n");
    for (auto command : CommandCollection::AllCommands()) {
      printf("%-20s%s\n", command->Name(), command->ShortHelpInfo());
    }
  }

  void PrintDetailedHelp(const Command& command) {
    printf("%s\n", command.DetailedHelpInfo());
  }
};

HelpCmd helpcmd;

