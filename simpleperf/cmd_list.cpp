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

#include <stdio.h>
#include <string>
#include <vector>

#include "command.h"
#include "event.h"

class ListCommand : public Command {
 public:
  ListCommand()
      : Command("list",
                "list all available perf events",
                "Usage: simpleperf list\n"
                "    List all available perf events on this machine.\n") { }

  bool RunCommand(std::vector<std::string>& args) override {
    if (args.size() != 0) {
      return false;
    }

    PrintEvents("hardware events", Event::HardwareEvents());
    PrintEvents("software events", Event::SoftwareEvents());
    PrintEvents("hw-cache events", Event::HwcacheEvents());
    return true;
  }

 private:
  void PrintEvents(const char* event_category, const std::vector<const Event*>& events) {
    printf("List of %s:\n", event_category);
    for (auto event : events) {
      if (event->Supported()) {
        printf("  %s\n", event->Name());
      }
    }
    printf("\n");
  }
};

ListCommand list_cmd;
