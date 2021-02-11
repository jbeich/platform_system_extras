/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "RecordFilter.h"

#include "environment.h"
#include "utils.h"

namespace simpleperf {

bool RecordFilter::ParseOptions(OptionValueMap& options) {
  for (bool exclude : {true, false}) {
    std::string prefix = exclude ? "--exclude-" : "--include-";
    for (const OptionValue& value : options.PullValues(prefix + "pid")) {
      if (auto pids = GetTidsFromString(*value.str_value, false); pids) {
        AddPids(pids.value(), exclude);
      } else {
        return false;
      }
    }
    for (const OptionValue& value : options.PullValues(prefix + "tid")) {
      if (auto tids = GetTidsFromString(*value.str_value, false); tids) {
        AddTids(tids.value(), exclude);
      } else {
        return false;
      }
    }
    if (auto value = options.PullValue(prefix + "process-name"); value) {
      AddProcessNameRegex(*value->str_value, exclude);
    }
    if (auto value = options.PullValue(prefix + "thread-name"); value) {
      AddThreadNameRegex(*value->str_value, exclude);
    }
    for (const OptionValue& value : options.PullValues(prefix + "uid")) {
      if (auto uids = ParseNonNegativeIntVector<uid_t>(*value.str_value); uids) {
        AddUids(uids.value(), exclude);
      } else {
        return false;
      }
    }
  }
  return true;
}

void RecordFilter::AddPids(const std::set<pid_t>& pids, bool exclude) {
  if (exclude) {
    exclude_condition_.pids.insert(pids.begin(), pids.end());
    has_exclude_condition_ = true;
  } else {
    include_condition_.pids.insert(pids.begin(), pids.end());
    has_include_condition_ = true;
  }
}

void RecordFilter::AddTids(const std::set<pid_t>& tids, bool exclude) {
  if (exclude) {
    exclude_condition_.tids.insert(tids.begin(), tids.end());
    has_exclude_condition_ = true;
  } else {
    include_condition_.tids.insert(tids.begin(), tids.end());
    has_include_condition_ = true;
  }
}

void RecordFilter::AddProcessNameRegex(const std::string& process_name, bool exclude) {
  if (exclude) {
    exclude_condition_.process_name_regs.emplace_back(process_name);
    has_exclude_condition_ = true;
  } else {
    include_condition_.process_name_regs.emplace_back(process_name);
    has_include_condition_ = true;
  }
}

void RecordFilter::AddThreadNameRegex(const std::string& thread_name, bool exclude) {
  if (exclude) {
    exclude_condition_.thread_name_regs.emplace_back(thread_name);
    has_exclude_condition_ = true;
  } else {
    include_condition_.thread_name_regs.emplace_back(thread_name);
    has_include_condition_ = true;
  }
}

void RecordFilter::AddUids(const std::set<uid_t>& uids, bool exclude) {
  if (exclude) {
    exclude_condition_.uids.insert(uids.begin(), uids.end());
    has_exclude_condition_ = true;
  } else {
    include_condition_.uids.insert(uids.begin(), uids.end());
    has_include_condition_ = true;
  }
}

bool RecordFilter::Check(const SampleRecord* r) {
  if (has_exclude_condition_ && CheckCondition(r, exclude_condition_)) {
    return false;
  }
  if (has_include_condition_ && !CheckCondition(r, include_condition_)) {
    return false;
  }
  return true;
}

bool RecordFilter::CheckCondition(const SampleRecord* r, const RecordFilterCondition& condition) {
  if (condition.pids.count(r->tid_data.pid) == 1) {
    return true;
  }
  if (condition.tids.count(r->tid_data.tid) == 1) {
    return true;
  }
  if (!condition.process_name_regs.empty()) {
    if (ThreadEntry* process = thread_tree_.FindThread(r->tid_data.pid); process != nullptr) {
      if (SearchInRegs(process->comm, condition.process_name_regs)) {
        return true;
      }
    }
  }
  if (!condition.thread_name_regs.empty()) {
    if (ThreadEntry* thread = thread_tree_.FindThread(r->tid_data.tid); thread != nullptr) {
      if (SearchInRegs(thread->comm, condition.thread_name_regs)) {
        return true;
      }
    }
  }
  if (!condition.uids.empty()) {
    if (auto uid_value = GetUidForProcess(r->tid_data.pid); uid_value) {
      if (condition.uids.count(uid_value.value()) == 1) {
        return true;
      }
    }
  }
  return false;
}

bool RecordFilter::SearchInRegs(const std::string& s, const std::vector<std::regex>& regs) {
  for (auto& reg : regs) {
    if (std::regex_search(s, reg)) {
      return true;
    }
  }
  return false;
}

std::optional<uid_t> RecordFilter::GetUidForProcess(pid_t pid) {
  if (auto it = pid_to_uid_map_.find(pid); it != pid_to_uid_map_.end()) {
    return it->second;
  }
  auto uid = GetProcessUid(pid);
  pid_to_uid_map_[pid] = uid;
  return uid;
}

}  // namespace simpleperf
