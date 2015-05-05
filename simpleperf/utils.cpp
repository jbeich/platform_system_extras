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

#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/logging.h>

void PrintIndented(size_t indent, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("%*s", static_cast<int>(indent * 2), "");
  vprintf(fmt, ap);
  va_end(ap);
}

bool IsPowerOfTwo(uint64_t value) {
  return (value != 0 && ((value & (value - 1)) == 0));
}

bool NextArgumentOrError(const std::vector<std::string>& args, size_t* pi) {
  if (*pi + 1 == args.size()) {
    LOG(ERROR) << "No argument following " << args[*pi] << " option. Try `simpleperf help "
               << args[0] << "`";
    return false;
  }
  ++*pi;
  return true;
}

std::vector<std::string> GetEntriesInDir(std::string dirpath) {
  if (dirpath.back() != '/') {
    dirpath += "/";
  }
  std::vector<std::string> result;
  DIR* dir = opendir(dirpath.c_str());
  if (dir == nullptr) {
    PLOG(DEBUG) << "can't open dir " << dirpath;
    return result;
  }
  dirent entry, *entry_p;
  while (readdir_r(dir, &entry, &entry_p) == 0 && entry_p != nullptr) {
    std::string subname = entry_p->d_name;
    if (subname == "." || subname == "..") {
      continue;
    }
    std::string subpath = dirpath + subname;
    struct stat st;
    if (stat(subpath.c_str(), &st) != 0) {
      PLOG(DEBUG) << "stat() failed for " << subpath;
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      subname += "/";
    }
    result.push_back(subname);
  }
  closedir(dir);
  return result;
}
