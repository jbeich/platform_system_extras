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

class HelpCmd : public Command {
 public:
  HelpCmd()
      : Command("help",
                "print help information for simpleperf",
                "Usage: simpleperf help [subcommand]\n"
                "    Without subcommand, print brief help information for every subcommand.\n"
                "    With subcommand, print detailed help information for the subcommand.\n\n"
                ) {
  }

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

