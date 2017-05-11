/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SIMPLE_PERF_SCHED_INFO_MERGER_H_
#define SIMPLE_PERF_SCHED_INFO_MERGER_H_

#include <unordered_map>
#include <vector>

#include <android-base/logging.h>
#include <android-base/parseint.h>

#include "event_selection_set.h"
#include "record.h"
#include "record_file.h"
#include "thread_tree.h"
#include "tracing.h"

class SchedInfoMerger {
 private:
  struct SchedInfo {
    uint64_t timestamp_in_ns;
    int next_pid;
  };

  struct ThreadInfo {
    int pid; // thread group id
    std::vector<uint64_t> ips;  // last schedule out callchain
    std::vector<char> raw;  // last schedule out raw data
  };

 public:
  SchedInfoMerger(RecordFileWriter* old_writer, RecordFileWriter* new_writer,
                  const std::string& trace_filename,
                  const std::unordered_map<int, std::unique_ptr<ThreadEntry>>& threads,
                  const std::vector<EventAttrWithId>& event_attrs)
      : old_writer_(old_writer), new_writer_(new_writer), trace_filename_(trace_filename),
        has_sched_info_(true) {
    // Get sched_switch_attr.
    const EventType* type = FindEventTypeByName("sched:sched_switch");
    CHECK(type != nullptr);
    bool has_sched_switch_attr = false;
    for (auto& attr : event_attrs) {
      if (attr.attr->type == type->type && attr.attr->config == type->config) {
        sched_switch_attr_ = attr;
        has_sched_switch_attr = true;
        break;
      }
    }
    CHECK(has_sched_switch_attr);
    LOG(ERROR) << "sched_switch_attr type = " << sched_switch_attr_.attr->type
        << ", config = " << sched_switch_attr_.attr->config
        << ", id = " << sched_switch_attr_.ids[0];

    // Get thread information.
    for (auto& pair : threads) {
      ThreadInfo& info = thread_info_map_[pair.first];
      info.pid = pair.second->pid;
      LOG(ERROR) << "thread_info_map. thread " << pair.first;
    }
  }

  bool Merge() {
    ParseNextSchedInfo();
    return old_writer_->ReadDataSection(
        [this](const Record* record) {
          return ReadRecordCallback(record);
        });
  }

 private:
  bool ReadRecordCallback(const Record* r) {
    if (has_sched_info_) {
      LOG(INFO) << "compare sched_info time " << current_sched_info_.timestamp_in_ns
          << ", record  time " << r->Timestamp() << ", less = "
          << (current_sched_info_.timestamp_in_ns < r->Timestamp());
    }
    while (has_sched_info_ && (current_sched_info_.timestamp_in_ns < r->Timestamp())) {
      LOG(INFO) << "will write sched info";
      if (!WriteSchedInfo()) {
        return false;
      }
      ParseNextSchedInfo();
    }
    if (has_sched_info_) {
      if (r->type() == PERF_RECORD_SAMPLE) {
        auto sample = static_cast<const SampleRecord*>(r);
        sample->Dump(0);
        LOG(ERROR) << "sample->attr_config_for_tracepoint = " << sample->attr_config_for_tracepoint
            << ", sched_switch config " << sched_switch_attr_.attr->config;
        if (sample->attr_config_for_tracepoint == sched_switch_attr_.attr->config) {
          UpdateThreadInfo(sample);
        }
      }
    }
    if (r->type() == PERF_RECORD_TRACING_DATA) {
      if (!ProcessTracingData(static_cast<const TracingDataRecord*>(r))) {
        return false;
      }
    }
    return new_writer_->WriteRecord(*r);
  }

  bool ProcessTracingData(const TracingDataRecord* r) {
    Tracing tracing(std::vector<char>(r->data, r->data + r->data_size));
    const TracingFormat* format = tracing.GetTracingFormatHavingId(
        sched_switch_attr_.attr->config);
    if (format == nullptr) {
      return false;
    }
    format->GetField("prev_pid", prev_pid_place_);
    format->GetField("next_pid", next_pid_place_);
    return true;
  }

  void UpdateThreadInfo(const SampleRecord* r) {
    LOG(INFO) << "update thread info for tid = " << r->tid_data.tid;
    ThreadInfo& info = thread_info_map_[r->tid_data.tid];
    info.ips.clear();
    info.ips[0] = r->ip_data.ip;
    if (r->sample_type & PERF_SAMPLE_CALLCHAIN) {
      info.ips.insert(info.ips.end(), r->callchain_data.ips,
                      r->callchain_data.ips + r->callchain_data.ip_nr);
    }
    info.raw.assign(r->raw_data.data, r->raw_data.data + r->raw_data.size);
  }

  bool WriteSchedInfo() {
    int tid = current_sched_info_.next_pid;
    ThreadInfo& info = thread_info_map_[tid];
    if (info.raw.empty()) {
      // No sched out information for thread [tid], so skip its schedule in info.
      LOG(INFO) << "raw data is empty";
      return true;
    }
    // Update prev_pid and next_pid.
    prev_pid_place_.WriteToData(info.raw.data(), UINT64_MAX);
    next_pid_place_.WriteToData(info.raw.data(), tid);
    SampleRecord r(*sched_switch_attr_.attr, sched_switch_attr_.ids[0], info.ips[0],
                   info.pid, tid, current_sched_info_.timestamp_in_ns, 0, 1, info.ips,
                   info.raw);
    LOG(ERROR) << "Write sched info id = " << sched_switch_attr_.ids[0] << ", in record " << r.id_data.id;
    info.raw.clear();
    return new_writer_->WriteRecord(r);
  }

  void ParseNextSchedInfo() {
    CHECK(has_sched_info_);
    has_sched_info_ = false;
    if (trace_reader_ == nullptr) {
      FILE* fp = fopen(trace_filename_.c_str(), "r");
      if (fp == nullptr) {
        PLOG(ERROR) << "fopen";
        return;
      }
      trace_reader_.reset(new LineReader(fp));
    }
    while (true) {
      char* line = trace_reader_->ReadLine();
      if (line == nullptr) {
        break;
      }
      // Parse line like: ... 2174872.383765: sched_switch: ...  next_pid=0 next_prio=120
      std::string s = line;
      LOG(ERROR) << "tracer: " << s;
      size_t event_pos = s.find("sched_switch");
      if (event_pos == std::string::npos) {
        continue;
      }
      if (!ParseNextPid(s, event_pos, &current_sched_info_.next_pid)) {
        continue;
      }
      LOG(ERROR) << "next_pid = " << current_sched_info_.next_pid;
      if (thread_info_map_.find(current_sched_info_.next_pid) == thread_info_map_.end()) {
        LOG(ERROR) << "not related";
        // Skip unrelated thread schedule info.
        continue;
      }
      size_t time_end_pos = s.rfind(' ', event_pos);
      if (time_end_pos == std::string::npos || time_end_pos == 0) {
        LOG(ERROR) << "time_end failed: " << s.substr(0, event_pos);
        continue;
      }
      time_end_pos--;
      size_t time_start_pos = s.rfind(' ', time_end_pos);
      if (time_start_pos == std::string::npos) {
        LOG(ERROR) << "time start failed: " << s.substr(0, time_end_pos);
        continue;
      }
      time_start_pos++;
      if (!ParseTime(s.substr(time_start_pos, time_end_pos - time_start_pos + 1),
                     &current_sched_info_.timestamp_in_ns)) {
        LOG(ERROR) << "time parse failed: " << s.substr(time_start_pos, time_end_pos - time_start_pos);
        continue;
      }
      LOG(ERROR) << "time = " << current_sched_info_.timestamp_in_ns;
      has_sched_info_ = true;
      break;
    }
    LOG(ERROR) << "end parsing sched info, has_sched_info = " << has_sched_info_;
  }

  bool ParseNextPid(const std::string& s, size_t start, int* pid) {
    // Parse string like: ...  next_pid=0 next_prio=120
    start = s.find("next_pid=", start);
    if (start == std::string::npos) {
      return false;
    }
    start += strlen("next_pid=");
    size_t end = s.find(' ', start);
    if (end == std::string::npos) {
      return false;
    }
    return android::base::ParseInt(s.substr(start, end - start), pid, 0);
  }

  bool ParseTime(const std::string& s, uint64_t* time_in_ns) {
    // Parse string like: 2174872.383765:
    size_t start = 0;
    size_t dot_pos = s.find('.', start);
    if (dot_pos == std::string::npos) {
      return false;
    }
    uint64_t sec_part;
    if (!android::base::ParseUint(s.substr(start, dot_pos - start), &sec_part)) {
      return false;
    }
    start = dot_pos + 1;
    size_t end = s.find(':', start);
    if (end == std::string::npos) {
      return false;
    }
    uint64_t usec_part;
    if (!android::base::ParseUint(s.substr(start, end - start), &usec_part)) {
      return false;
    }
    *time_in_ns = ((sec_part * 1000000) + usec_part) * 1000;
    return true;
  }

  RecordFileWriter* old_writer_;
  RecordFileWriter* new_writer_;
  std::string trace_filename_;
  EventAttrWithId sched_switch_attr_;
  std::unique_ptr<LineReader> trace_reader_;
  std::unordered_map<int, ThreadInfo> thread_info_map_;
  SchedInfo current_sched_info_;
  bool has_sched_info_;
  TracingFieldPlace prev_pid_place_;
  TracingFieldPlace next_pid_place_;
};

#endif  // SIMPLE_PERF_SCHED_INFO_MERGER_H_
