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

#include "util.h"

#include <ctype.h>
#include <time.h>

std::vector<std::string> SplitString(const char* s) {
  std::vector<std::string> args;
  std::string arg;

  for (const char* p = s; *p != '\0'; ++p) {
    if (isspace(*p)) {
      if (arg.size() > 0) {
        args.push_back(arg);
        arg.clear();
      }
    } else {
      arg.push_back(*p);
    }
  }
  if (arg.size() > 0) {
    args.push_back(arg);
  }
  return args;
}

int64_t NanoTime() {
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<int64_t>(t.tv_sec) * 1000000000 + t.tv_nsec;
}
