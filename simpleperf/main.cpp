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

#include <string.h>
#include <string>
#include <vector>

#include "command.h"
#include "utils.h"

int main(int argc, char** argv) {
  std::vector<std::string> args;

  if (argc == 1 || (argc == 2 && strcmp(argv[1], "--help") == 0)) {
    args.push_back("help");
  } else {
    for (int i = 1; i < argc; ++i) {
      args.push_back(argv[i]);
    }
  }

  Command* command = Command::FindCommand(args[0]);
  if (command == nullptr) {
    LOGE("malformed command line: unknown command \"%s\"", args[0].c_str());
    return 1;
  }
  args.erase(args.begin());
  bool result = command->Run(args);
  if (result == true) {
    LOGI("run command %s successfully", args[0].c_str());
  } else {
    LOGI("run command %s unsuccessfully", args[0].c_str());
  }
  return result ? 0 : 1;
}
