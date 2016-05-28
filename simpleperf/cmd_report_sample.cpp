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

#include "command.h"
#include "record_file.h"
#include "thread_tree.h"
#include "utils.h"

namespace {

class ReportSampleCommand : public Command {
 public:
  ReportSampleCommand() : Command("report-sample", "report raw sample information in perf.data",
                                  // clang-format off
"Usage: simpleperf report-sample [options]\n"
"-i <file>  Specify path of record file, default is perf.data.\n"
"--show-callchain  Print callchain samples.\n"
                                  // clang-format on

  ),
  record_filename_("perf.data"), show_callchain_(false) {}

  bool Run(const std::vector<std::string>& args) override;

 private:
  bool ParseOptions(const std::vector<std::string>& args);
  bool ProcessRecord(std::unique_ptr<Record> record);
  bool PrintSampleRecord(const SampleRecord& record);

  std::string record_filename_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  bool show_callchain_;
  ThreadTree thread_tree_;
};

bool ReportSampleCommand::Run(const std::vector<std::string>& args) {
  // 1. Parse options.
  if (!ParseOptions(args)) {
    return false;
  }
  // 2. Read record file, build ThreadTree, and print samples online.
  record_file_reader_ = RecordFileReader::CreateInstance(record_filename_);
  if (record_file_reader_ == nullptr) {
    return false;
  }
  if (!record_file_reader_->ReadDataSection([this](std::unique_ptr<Record> record) {
    return ProcessRecord(std::move(record));
  })) {
    return false;
  }
  return true;
}

bool ReportSampleCommand::ParseOptions(const std::vector<std::string>& args) {
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "-i") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      record_filename_ = args[i];
    } else if (args[i] == "--show-callchain") {
      show_callchain_ = true;
    } else {
      ReportUnknownOption(args, i);
      return false;
    }
  }
  return true;
}

bool ReportSampleCommand::ProcessRecord(std::unique_ptr<Record> record) {
  thread_tree_.BuildThreadTree(*record);
  if (record->type() == PERF_RECORD_SAMPLE) {
    return PrintSampleRecord(*static_cast<const SampleRecord*>(record.get()));
  } else if (record->type() == SIMPLE_PERF_RECORD_KERNEL_INFO) {
    auto& r = *static_cast<const KernelInfoRecord*>(record.get());
    Dso::SetKallsyms(std::move(r.kallsyms));
  }
  return true;
}

bool ReportSampleCommand::PrintSampleRecord(const SampleRecord& r) {
  uint64_t time = r.time_data.time;
  bool in_kernel = r.InKernel();
  const ThreadEntry* thread = thread_tree_.FindThreadOrNew(r.tid_data.pid, r.tid_data.tid);
  const MapEntry* map = thread_tree_.FindMap(thread, r.ip_data.ip, in_kernel);
  const Symbol* symbol = thread_tree_.FindSymbol(map, r.ip_data.ip);
  PrintIndented(0, "sample:\n");
  PrintIndented(1, "time: %" PRIu64 "\n", time);
  PrintIndented(1, "ip: %" PRIx64 "\n", r.ip_data.ip);
  PrintIndented(1, "dso: %s\n", map->dso->Path().c_str());
  PrintIndented(1, "symbol: %s\n", symbol->DemangledName());
  if (show_callchain_) {
    PrintIndented(1, "callchain:\n");
    const std::vector<uint64_t>& ips = r.callchain_data.ips;
    bool first_ip = true;
    for (auto& ip : ips) {
      if (ip >= PERF_CONTEXT_MAX) {
        switch (ip) {
          case PERF_CONTEXT_KERNEL:
            in_kernel = true;
            break;
          case PERF_CONTEXT_USER:
            in_kernel = false;
            break;
          default:
            LOG(DEBUG) << "Unexpected perf_context in callchain: " << std::hex << ip;
        }
      } else {
        if (first_ip) {
          first_ip = false;
          // Remove duplication with sample ip.
          if (ip == r.ip_data.ip) {
            continue;
          }
        }
        const MapEntry* map = thread_tree_.FindMap(thread, ip, in_kernel);
        const Symbol* symbol = thread_tree_.FindSymbol(map, ip);
        PrintIndented(2, "ip: %" PRIx64 "\n", ip);
        PrintIndented(2, "dso: %s\n", map->dso->Path().c_str());
        PrintIndented(2, "symbol: %s\n", symbol->DemangledName());
      }
    }
  }
  return true;
}

}  // namespace

void RegisterReportSampleCommand() {
  RegisterCommand("report-sample", [] { return std::unique_ptr<Command>(new ReportSampleCommand()); });
}
