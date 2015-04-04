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
