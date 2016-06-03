/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <memory>

#include "system/extras/simpleperf/report_sample.pb.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "command.h"
#include "record_file.h"
#include "thread_tree.h"
#include "utils.h"

namespace proto = simpleperf_report_proto;

namespace {

class ReportSampleCommand : public Command {
 public:
  ReportSampleCommand()
      : Command(
            "report-sample", "report raw sample information in perf.data",
            // clang-format off
"Usage: simpleperf report-sample [options]\n"
"-i <file>  Specify path of record file, default is perf.data.\n"
"-o report_file_name  Set report file name, default is stdout.\n"
"--protobuf  Use protobuf format in report_sample.proto to output samples.\n"
"            Need to set a report_file_name when using this option.\n"
"--show-callchain  Print callchain samples.\n"
            // clang-format on
            ),
        record_filename_("perf.data"),
        show_callchain_(false),
        use_protobuf_(false),
        report_fp_(nullptr),
        coded_os_(nullptr),
        sample_count_(0) {}

  bool Run(const std::vector<std::string>& args) override;

 private:
  bool ParseOptions(const std::vector<std::string>& args);
  bool ProcessRecord(std::unique_ptr<Record> record);
  bool PrintSampleRecordInProtobuf(const SampleRecord& record);
  bool PrintSampleRecord(const SampleRecord& record);

  std::string record_filename_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  bool show_callchain_;
  bool use_protobuf_;
  ThreadTree thread_tree_;
  std::string report_filename_;
  FILE* report_fp_;
  google::protobuf::io::CodedOutputStream* coded_os_;
  size_t sample_count_;
};

bool ReportSampleCommand::Run(const std::vector<std::string>& args) {
  // 1. Parse options.
  if (!ParseOptions(args)) {
    return false;
  }
  if (use_protobuf_) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
  }

  // 2. Open record file.
  record_file_reader_ = RecordFileReader::CreateInstance(record_filename_);
  if (record_file_reader_ == nullptr) {
    return false;
  }

  // 3. Prepare report output stream.
  report_fp_ = stdout;
  std::unique_ptr<FILE, decltype(&fclose)> fp(nullptr, fclose);
  std::unique_ptr<FileHelper> protobuf_file_helper;
  std::unique_ptr<google::protobuf::io::FileOutputStream> protobuf_file_os;
  std::unique_ptr<google::protobuf::io::CodedOutputStream> protobuf_coded_os;
  if (!use_protobuf_ && !report_filename_.empty()) {
    fp.reset(fopen(report_filename_.c_str(), "w"));
    if (fp == nullptr) {
      PLOG(ERROR) << "failed to open " << report_filename_;
      return false;
    }
    report_fp_ = fp.get();
  } else if (use_protobuf_) {
    protobuf_file_helper.reset(
        new FileHelper(FileHelper::OpenWriteOnly(report_filename_)));
    if (protobuf_file_helper->fd() == -1) {
      PLOG(ERROR) << "failed to open " << report_filename_;
      return false;
    }
    protobuf_file_os.reset(
        new google::protobuf::io::FileOutputStream(protobuf_file_helper->fd()));
    protobuf_coded_os.reset(
        new google::protobuf::io::CodedOutputStream(protobuf_file_os.get()));
    coded_os_ = protobuf_coded_os.get();
  }

  // 4. Read record file, and print samples online.
  if (!record_file_reader_->ReadDataSection(
          [this](std::unique_ptr<Record> record) {
            return ProcessRecord(std::move(record));
          })) {
    return false;
  }
  LOG(INFO) << "report " << sample_count_ << " samples in all.";

  if (use_protobuf_) {
    coded_os_->WriteLittleEndian32(0);
    if (coded_os_->HadError()) {
      LOG(ERROR) << "print protobuf report failed";
      return false;
    }
    protobuf_coded_os.reset(nullptr);
    protobuf_file_os.reset(nullptr);
    protobuf_file_helper.reset(nullptr);
    google::protobuf::ShutdownProtobufLibrary();
  } else {
    fflush(report_fp_);
    if (ferror(report_fp_) != 0) {
      PLOG(ERROR) << "print report failed";
      return false;
    }
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
    } else if (args[i] == "-o") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      report_filename_ = args[i];
    } else if (args[i] == "--protobuf") {
      use_protobuf_ = true;
    } else if (args[i] == "--show-callchain") {
      show_callchain_ = true;
    } else {
      ReportUnknownOption(args, i);
      return false;
    }
  }

  if (use_protobuf_ && report_filename_.empty()) {
    LOG(ERROR) << "please specify a report filename to write protobuf data";
    return false;
  }
  return true;
}

bool ReportSampleCommand::ProcessRecord(std::unique_ptr<Record> record) {
  thread_tree_.Update(*record);
  if (record->type() == PERF_RECORD_SAMPLE) {
    sample_count_++;
    auto& r = *static_cast<const SampleRecord*>(record.get());
    if (use_protobuf_) {
      return PrintSampleRecordInProtobuf(r);
    } else {
      return PrintSampleRecord(r);
    }
  }
  return true;
}

bool ReportSampleCommand::PrintSampleRecordInProtobuf(const SampleRecord& r) {
  proto::Record proto_record;
  proto_record.set_type(proto::Record_Type_SAMPLE);
  proto::Sample* sample = proto_record.mutable_sample();
  sample->set_time(r.time_data.time);
  proto::Sample_CallChainEntry* callchain = sample->add_callchain();
  callchain->set_ip(r.ip_data.ip);

  bool in_kernel = r.InKernel();
  const ThreadEntry* thread =
      thread_tree_.FindThreadOrNew(r.tid_data.pid, r.tid_data.tid);
  const MapEntry* map = thread_tree_.FindMap(thread, r.ip_data.ip, in_kernel);
  const Symbol* symbol = thread_tree_.FindSymbol(map, r.ip_data.ip);
  callchain->set_symbol(symbol->DemangledName());
  callchain->set_file(map->dso->Path());

  if (show_callchain_) {
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
            LOG(DEBUG) << "Unexpected perf_context in callchain: " << std::hex
                       << ip << std::dec;
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
        callchain = sample->add_callchain();
        callchain->set_ip(ip);
        callchain->set_symbol(symbol->DemangledName());
        callchain->set_file(map->dso->Path());
      }
    }
  }
  coded_os_->WriteLittleEndian32(proto_record.ByteSize());
  if (!proto_record.SerializeToCodedStream(coded_os_)) {
    LOG(ERROR) << "failed to write sample to protobuf";
    return false;
  }
  return true;
}

bool ReportSampleCommand::PrintSampleRecord(const SampleRecord& r) {
  bool in_kernel = r.InKernel();
  const ThreadEntry* thread =
      thread_tree_.FindThreadOrNew(r.tid_data.pid, r.tid_data.tid);
  const MapEntry* map = thread_tree_.FindMap(thread, r.ip_data.ip, in_kernel);
  const Symbol* symbol = thread_tree_.FindSymbol(map, r.ip_data.ip);
  FprintIndented(report_fp_, 0, "sample:\n");
  FprintIndented(report_fp_, 1, "time: %" PRIu64 "\n", r.time_data.time);
  FprintIndented(report_fp_, 1, "ip: %" PRIx64 "\n", r.ip_data.ip);
  FprintIndented(report_fp_, 1, "dso: %s\n", map->dso->Path().c_str());
  FprintIndented(report_fp_, 1, "symbol: %s\n", symbol->DemangledName());

  if (show_callchain_) {
    FprintIndented(report_fp_, 1, "callchain:\n");
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
            LOG(DEBUG) << "Unexpected perf_context in callchain: " << std::hex
                       << ip;
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
        FprintIndented(report_fp_, 2, "ip: %" PRIx64 "\n", ip);
        FprintIndented(report_fp_, 2, "dso: %s\n", map->dso->Path().c_str());
        FprintIndented(report_fp_, 2, "symbol: %s\n", symbol->DemangledName());
      }
    }
  }
  return true;
}

}  // namespace

void RegisterReportSampleCommand() {
  RegisterCommand("report-sample", [] {
    return std::unique_ptr<Command>(new ReportSampleCommand());
  });
}
