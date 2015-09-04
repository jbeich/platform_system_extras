// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/stringprintf.h>

#include "log.h"
#include "tasklist.h"

bool TaskList::Scan(std::map<pid_t, std::vector<pid_t>>& tgid_map) {
  tgid_map.clear();

  std::unique_ptr<DIR, decltype(&closedir)> proc_dir(opendir("/proc"), closedir);
  if (!proc_dir) {
    return false;
  }

  struct dirent* entry;
  while ((entry = readdir(proc_dir.get())) != nullptr) {
    if (isdigit(entry->d_name[0])) {
      pid_t tgid = atoi(entry->d_name);

      std::vector<pid_t> pid_list;
      if (!ScanPid(tgid, pid_list)) {
        continue;
      }
      tgid_map.insert(std::make_pair(tgid, pid_list));
    }
  }

  return true;
}

bool TaskList::ScanPid(pid_t tgid, std::vector<pid_t>& pid_list) {
  std::string filename = android::base::StringPrintf("/proc/%d/task", tgid);

  std::unique_ptr<DIR, decltype(&closedir)> task_dir(opendir(filename.c_str()), closedir);
  if (!task_dir) {
    return false;
  }

  struct dirent* entry;
  while ((entry = readdir(task_dir.get())) != nullptr) {
    if (isdigit(entry->d_name[0])) {
      pid_t pid = atoi(entry->d_name);
      pid_list.push_back(pid);
    }
  }

  return true;
}
