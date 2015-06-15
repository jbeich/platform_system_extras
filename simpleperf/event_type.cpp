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

#include "event_type.h"

#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>

#include <base/file.h>
#include <base/logging.h>

#include "event_attr.h"
#include "event_fd.h"
#include "utils.h"

#define EVENT_TYPE_TABLE_ENTRY(name, type, config) \
  { name, type, config }                           \
  ,

static const std::vector<EventType> static_event_type_array = {
#include "event_type_table.h"
};

static bool IsEventTypeSupportedByKernel(const EventType& event_type) {
  auto event_fd =
      EventFd::OpenEventFileForProcess(CreateDefaultPerfEventAttr(event_type), getpid(), false);
  return event_fd != nullptr;
}

bool EventType::IsSupportedByKernel() const {
  return IsEventTypeSupportedByKernel(*this);
}

static const std::vector<EventType> GetTracepointEventTypes() {
  std::vector<EventType> result;
  const std::string tracepoint_dirname = "/sys/kernel/debug/tracing/events";
  std::vector<std::string> system_dirs;
  GetEntriesInDir(tracepoint_dirname, nullptr, &system_dirs);
  for (auto& system_name : system_dirs) {
    std::string system_path = tracepoint_dirname + "/" + system_name;
    std::vector<std::string> event_dirs;
    GetEntriesInDir(system_path, nullptr, &event_dirs);
    for (auto& event_name : event_dirs) {
      std::string id_path = system_path + "/" + event_name + "/id";
      std::string id_content;
      if (!android::base::ReadFileToString(id_path, &id_content)) {
        continue;
      }
      char* endptr;
      uint64_t id = strtoull(id_content.c_str(), &endptr, 10);
      if (endptr == id_content.c_str()) {
        LOG(DEBUG) << "unexpected id '" << id_content << "' in " << id_path;
        continue;
      }
      result.push_back(EventType(system_name + ":" + event_name, PERF_TYPE_TRACEPOINT, id));
    }
  }
  std::sort(result.begin(), result.end(),
            [](const EventType& type1, const EventType& type2) { return type1.name < type2.name; });
  return result;
}

const std::vector<EventType>& GetAllEventTypes() {
  static std::vector<EventType> event_type_array;
  if (event_type_array.empty()) {
    event_type_array.insert(event_type_array.end(), static_event_type_array.begin(),
                            static_event_type_array.end());
    const std::vector<EventType> tracepoint_array = GetTracepointEventTypes();
    event_type_array.insert(event_type_array.end(), tracepoint_array.begin(),
                            tracepoint_array.end());
  }
  return event_type_array;
}

const EventType* FindEventTypeByConfig(uint32_t type, uint64_t config) {
  for (auto& event_type : GetAllEventTypes()) {
    if (event_type.type == type && event_type.config == config) {
      return &event_type;
    }
  }
  return nullptr;
}

static const EventType* FindEventTypeByName(const std::string& name, bool report_unsupported_type) {
  const EventType* result = nullptr;
  for (auto& event_type : GetAllEventTypes()) {
    if (event_type.name == name) {
      result = &event_type;
      break;
    }
  }
  if (result == nullptr) {
    LOG(ERROR) << "Unknown event_type '" << name
               << "', try `simpleperf list` to list all possible event type names";
    return nullptr;
  }
  if (!result->IsSupportedByKernel()) {
    (report_unsupported_type ? PLOG(ERROR) : PLOG(DEBUG)) << "Event type '" << result->name
                                                          << "' is not supported by the kernel";
    return nullptr;
  }
  return result;
}

std::unique_ptr<EventTypeAndModifier> ParseEventType(const std::string& event_type_str,
                                                     bool report_unsupported_type) {
  static std::string modifier_characters = "ukhGHp";
  std::unique_ptr<EventTypeAndModifier> event_type_modifier(new EventTypeAndModifier);
  std::string name = event_type_str;
  std::string modifier;
  size_t comm_pos = event_type_str.rfind(':');
  if (comm_pos != std::string::npos) {
    bool match_modifier = true;
    for (size_t i = comm_pos + 1; i < event_type_str.size(); ++i) {
      char c = event_type_str[i];
      if (c != ' ' && modifier_characters.find(c) == std::string::npos) {
        match_modifier = false;
        break;
      }
    }
    if (match_modifier) {
      name = event_type_str.substr(0, comm_pos);
      modifier = event_type_str.substr(comm_pos + 1);
    }
  }
  const EventType* event_type = FindEventTypeByName(name, report_unsupported_type);
  if (event_type == nullptr) {
    // Try if the modifier belongs to the event type name, like some tracepoint events.
    if (!modifier.empty()) {
      name = event_type_str;
      modifier.clear();
      event_type = FindEventTypeByName(name, report_unsupported_type);
    }
    if (event_type == nullptr) {
      return nullptr;
    }
  }
  event_type_modifier->event_type = *event_type;
  bool exclude_ukh = false;
  bool exclude_GH = false;
  for (auto& c : modifier) {
    if (c == 'u' || c == 'k' || c == 'h') {
      if (!exclude_ukh) {
        event_type_modifier->exclude_user = true;
        event_type_modifier->exclude_kernel = true;
        event_type_modifier->exclude_hv = true;
        exclude_ukh = true;
      }
      switch (c) {
        case 'u':
          event_type_modifier->exclude_user = false;
          break;
        case 'k':
          event_type_modifier->exclude_kernel = false;
          break;
        case 'h':
          event_type_modifier->exclude_hv = false;
          break;
      }
    } else if (c == 'G' || c == 'H') {
      if (!exclude_GH) {
        event_type_modifier->exclude_guest = true;
        event_type_modifier->exclude_host = true;
        exclude_GH = true;
      }
      switch (c) {
        case 'G':
          event_type_modifier->exclude_guest = false;
          break;
        case 'H':
          event_type_modifier->exclude_host = false;
          break;
      }
    } else if (c == 'p') {
      event_type_modifier->precise_ip++;
    } else if (c != ' ') {
      LOG(ERROR) << "Unknown event_type modifier '" << c << "'";
      return nullptr;
    }
  }
  return event_type_modifier;
}
