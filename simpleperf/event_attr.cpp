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

#include "event_attr.h"

#include <inttypes.h>
#include <stdio.h>
#include <string>
#include <unordered_map>

#include "event.h"
#include "utils.h"

static std::string SampleTypeToString(uint64_t sample_type) {
  std::unordered_map<int, std::string> map = {
      {PERF_SAMPLE_IP, "ip"},
      {PERF_SAMPLE_TID, "tid"},
      {PERF_SAMPLE_TIME, "time"},
      {PERF_SAMPLE_ADDR, "addr"},
      {PERF_SAMPLE_READ, "read"},
      {PERF_SAMPLE_CALLCHAIN, "callchain"},
      {PERF_SAMPLE_ID, "id"},
      {PERF_SAMPLE_CPU, "cpu"},
      {PERF_SAMPLE_PERIOD, "period"},
      {PERF_SAMPLE_STREAM_ID, "stream_id"},
      {PERF_SAMPLE_RAW, "raw"},
  };

  std::string result;
  for (auto p : map) {
    if (sample_type & p.first) {
      sample_type &= ~p.first;
      result += p.second + ", ";
    }
  }
  if (sample_type != 0) {
    LOGW("unknow sample_type bits: 0x%" PRIx64, sample_type);
  }

  if (result.size() > 2) {
    result = result.substr(0, result.size() - 2);
  }
  return result;
}

EventAttr::EventAttr(const Event* event) : event(event) {
  memset(&attr, 0, sizeof(attr));
  attr.size = sizeof(perf_event_attr);
  attr.type = event->Type();
  attr.config = event->Config();
  attr.mmap = 1;
  attr.comm = 1;
  attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;
  attr.sample_type |= PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_PERIOD;
}

EventAttr::EventAttr(const perf_event_attr* attr) : attr(*attr) {
  event = Event::FindEventByConfig(attr->type, attr->config);
  if (event == nullptr) {
    LOGE("can't find event with type %u, config %llu\n", attr->type, attr->config);
  }
}

std::string EventAttr::Name() const {
  return (event != nullptr) ? event->Name() : "unknown event";
}

void EventAttr::Dump(int space) const {
  PrintWithSpace(space, "event_attr: %s\n", Name().c_str());

  PrintWithSpace(space + 2, "type %u, size %u, config %llu\n", attr.type, attr.size, attr.config);

  if (attr.freq != 0) {
    PrintWithSpace(space + 2, "sample_freq %llu\n", attr.sample_freq);
  } else {
    PrintWithSpace(space + 2, "sample_period %llu\n", attr.sample_period);
  }

  PrintWithSpace(space + 2, "sample_type (0x%llx) %s\n", attr.sample_type,
                 SampleTypeToString(attr.sample_type).c_str());

  PrintWithSpace(space + 2, "read_format (0x%llx)\n", attr.read_format);

  PrintWithSpace(space + 2, "disabled %llu, inherit %llu, pinned %llu, exclusive %llu\n", attr.disabled,
                 attr.inherit, attr.pinned, attr.exclusive);

  PrintWithSpace(space + 2, "exclude_user %llu, exclude_kernel %llu, exclude_hv %llu\n", attr.exclude_user,
                 attr.exclude_kernel, attr.exclude_hv);

  PrintWithSpace(space + 2, "exclude_idle %llu, mmap %llu, comm %llu, freq %llu\n", attr.exclude_idle,
                 attr.mmap, attr.comm, attr.freq);

  PrintWithSpace(space + 2, "inherit_stat %llu, enable_on_exec %llu, task %llu\n", attr.inherit_stat,
                 attr.enable_on_exec, attr.task);

  PrintWithSpace(space + 2, "watermark %llu, precise_ip %llu, mmap_data %llu\n", attr.watermark,
                 attr.precise_ip, attr.mmap_data);

  PrintWithSpace(space + 2, "sample_id_all %llu, exclude_host %llu, exclude_guest %llu\n", attr.sample_id_all,
                 attr.exclude_host, attr.exclude_guest);
}
