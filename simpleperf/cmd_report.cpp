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

#include <inttypes.h>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>

#include "command.h"
#include "event_attr.h"
#include "event_type.h"
#include "record.h"
#include "record_file.h"
#include "sample_tree.h"
#include "utils.h"

// ReportFormatter is the base class of all formatters, which are used to format output of report
// command.
class ReportFormatter {
 public:
  virtual ~ReportFormatter() {
  }
  virtual void ParseSampleTree(const SampleTree& sample_tree) = 0;
  virtual void PrintReport() = 0;
};

// ProcessReportFormatter report per process.
class ProcessReportFormatter : public ReportFormatter {
 public:
  ProcessReportFormatter(const std::string event_type_name,
                         const std::unordered_map<int, std::string>& comms, bool exclude_fileinfo)
      : event_type_name_(event_type_name),
        comms_(comms),
        exclude_fileinfo_(exclude_fileinfo),
        total_sample_(0),
        total_period_(0) {
  }
  void ParseSampleTree(const SampleTree& sample_tree) override;
  void PrintReport() override;

 private:
  void SampleCallback(const SampleInMap& sample);

  struct SampleInfo {
    size_t sample_count;
    uint64_t period;
    SampleInfo() : sample_count(0), period(0) {
    }
  };

  const std::string event_type_name_;
  const std::unordered_map<int, std::string>& comms_;
  bool exclude_fileinfo_;
  std::unordered_map<int, std::map<std::string, SampleInfo>> pid_map_;
  std::vector<std::pair<int, SampleInfo>> pid_info_;
  std::unordered_map<int, std::vector<std::pair<std::string, SampleInfo>>> file_info_;
  uint64_t total_sample_;
  uint64_t total_period_;
};

void ProcessReportFormatter::ParseSampleTree(const SampleTree& sample_tree) {
  sample_tree.VisitAllSamples(
      std::bind(&ProcessReportFormatter::SampleCallback, this, std::placeholders::_1));
  for (auto& pid_pair : pid_map_) {
    SampleInfo info;
    for (auto& file_pair : pid_pair.second) {
      info.sample_count += file_pair.second.sample_count;
      info.period += file_pair.second.period;
    }
    pid_info_.push_back(std::make_pair(pid_pair.first, info));
  }
  std::sort(pid_info_.begin(), pid_info_.end(),
            [](const std::pair<int, SampleInfo>& p1, const std::pair<int, SampleInfo>& p2) {
              return p1.second.period > p2.second.period;
            });

  if (!exclude_fileinfo_) {
    for (auto& pid_pair : pid_map_) {
      std::vector<std::pair<std::string, SampleInfo>> file_info;
      for (auto file_pair : pid_pair.second) {
        file_info.push_back(std::make_pair(file_pair.first, file_pair.second));
      }
      std::sort(file_info.begin(), file_info.end(),
                [](const std::pair<std::string, SampleInfo>& p1,
                   const std::pair<std::string, SampleInfo>& p2) {
                  return p1.second.period > p2.second.period;
                });
      file_info_.insert(std::make_pair(pid_pair.first, std::move(file_info)));
    }
  }
}

void ProcessReportFormatter::SampleCallback(const SampleInMap& sample_in_map) {
  auto pid_it = pid_map_.find(sample_in_map.pid);
  if (pid_it == pid_map_.end()) {
    auto result =
        pid_map_.insert(std::make_pair(sample_in_map.pid, std::map<std::string, SampleInfo>()));
    pid_it = result.first;
  }
  auto file_it = pid_it->second.find(sample_in_map.map->filename);
  if (file_it == pid_it->second.end()) {
    auto result = pid_it->second.insert(std::make_pair(sample_in_map.map->filename, SampleInfo()));
    file_it = result.first;
  }
  SampleInfo& info = file_it->second;
  uint64_t increased_sample = sample_in_map.samples.size();
  info.sample_count += increased_sample;
  total_sample_ += increased_sample;
  uint64_t increased_period = 0;
  for (auto& sample : sample_in_map.samples) {
    increased_period += sample.period;
  }
  info.period += increased_period;
  total_period_ += increased_period;
}

static double ToPercentage(uint64_t numerator, uint64_t denominator) {
  double percentage = 0.0;
  if (denominator != 0) {
    percentage = 100.0 * numerator / denominator;
  }
  return percentage;
}

void ProcessReportFormatter::PrintReport() {
  printf("Samples: %" PRIu64 " of event '%s'\n", total_sample_, event_type_name_.c_str());
  printf("Event count: %" PRIu64 "\n", total_period_);
  printf("\n");

  const char* format_of_pid = "%7.2lf%% %10zu %20s %8d\n";
  const char* format_of_file = "%7.2lf%% %10zu %20s %8d  %s\n";
  printf("%8s %10s %20s %8s%s\n", "Overhead", "Samples", "Command", "Pid",
         (exclude_fileinfo_ ? "" : "  File"));
  for (auto& pid_pair : pid_info_) {
    int pid = pid_pair.first;
    double overhead = ToPercentage(pid_pair.second.period, total_period_);
    std::string comm = "Unknown";
    auto comm_it = comms_.find(pid);
    if (comm_it == comms_.end()) {
      LOG(WARNING) << "can't find command of pid " << pid;
    } else {
      comm = comm_it->second;
    }
    printf(format_of_pid, overhead, pid_pair.second.sample_count, comm.c_str(), pid);

    if (!exclude_fileinfo_) {
      auto& file_info = file_info_[pid];
      for (auto& file_pair : file_info) {
        double overhead = ToPercentage(file_pair.second.period, total_period_);
        printf(format_of_file, overhead, file_pair.second.sample_count, comm.c_str(), pid,
               file_pair.first.c_str());
      }
      printf("\n");
    }
  }
  fflush(stdout);
}

class ReportCommandImpl {
 public:
  ReportCommandImpl() : exclude_fileinfo_(false), record_filename_("perf.data") {
  }

  bool Run(const std::vector<std::string>& args);

 private:
  bool ParseOptions(const std::vector<std::string>& args);
  bool ReadEventAttrFromRecordFile();
  void ReadSampleTreeFromRecordFile();
  void PrintReport();

  // Report format options.
  bool exclude_fileinfo_;

  std::string record_filename_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  perf_event_attr event_attr_;
  SampleTree sample_tree_;
  std::unordered_map<int, std::string> comms_;
};

bool ReportCommandImpl::Run(const std::vector<std::string>& args) {
  // 1. Parse options.
  if (!ParseOptions(args)) {
    return false;
  }

  // 2. Read record file and build SampleTree.
  record_file_reader_ = RecordFileReader::CreateInstance(record_filename_);
  if (record_file_reader_ == nullptr) {
    return false;
  }
  if (!ReadEventAttrFromRecordFile()) {
    return false;
  }
  ReadSampleTreeFromRecordFile();

  // 3. Read symbol table from elf files.

  // 4. Show collected information.
  PrintReport();

  return true;
}

bool ReportCommandImpl::ParseOptions(const std::vector<std::string>& args) {
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-i") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      record_filename_ = args[i];
    } else if (args[i] == "--exclude-fileinfo") {
      exclude_fileinfo_ = true;
    } else {
      ReportUnknownOptionForCommand(args, i);
      return false;
    }
  }
  return true;
}

bool ReportCommandImpl::ReadEventAttrFromRecordFile() {
  std::vector<const PerfFileFormat::FileAttr*> attrs = record_file_reader_->AttrSection();
  if (attrs.size() != 1) {
    LOG(ERROR) << "record file contains " << attrs.size() << " attrs";
    return false;
  }
  event_attr_ = attrs[0]->attr;
  return true;
}

void ReportCommandImpl::ReadSampleTreeFromRecordFile() {
  std::vector<std::unique_ptr<const Record>> records = record_file_reader_->DataSection();
  for (auto& record : records) {
    if (record->header.type == PERF_RECORD_MMAP) {
      const MmapRecord& r = *static_cast<const MmapRecord*>(record.get());
      sample_tree_.AddMap(r.data.pid, r.data.addr, r.data.len, r.data.pgoff, r.filename,
                          r.sample_id.time_data.time);
    } else if (record->header.type == PERF_RECORD_SAMPLE) {
      const SampleRecord& r = *static_cast<const SampleRecord*>(record.get());
      sample_tree_.AddSample(r.tid_data.pid, r.tid_data.tid, r.ip_data.ip, r.time_data.time,
                             r.cpu_data.cpu, r.period_data.period);
    } else if (record->header.type == PERF_RECORD_COMM) {
      const CommRecord& r = *static_cast<const CommRecord*>(record.get());
      comms_[r.data.tid] = r.comm;
    }
  }
  // Add swapper as process 0. swapper has the same map information as the kernel.
  comms_[0] = "swapper";
}

void ReportCommandImpl::PrintReport() {
  DumpPerfEventAttr(event_attr_);
  printf("\n");
  const EventType* event_type =
      EventTypeFactory::FindEventTypeByConfig(event_attr_.type, event_attr_.config);
  std::string event_type_name;
  if (event_type != nullptr) {
    event_type_name = event_type->name;
  } else {
    event_type_name =
        android::base::StringPrintf("(type %u, config %llu)", event_attr_.type, event_attr_.config);
  }
  ProcessReportFormatter formatter(event_type_name, comms_, exclude_fileinfo_);
  formatter.ParseSampleTree(sample_tree_);
  formatter.PrintReport();
}

class ReportCommand : public Command {
 public:
  ReportCommand()
      : Command("report", "report sampling information in perf.data",
                "Usage: simpleperf report [options]\n"
                "    -i <file>     specify path of record file, default is perf.data\n"
                "    --exclude-fileinfo  don't show file specific sample info\n") {
  }

  bool Run(const std::vector<std::string>& args) override {
    ReportCommandImpl impl;
    return impl.Run(args);
  }
};

ReportCommand report_command;
