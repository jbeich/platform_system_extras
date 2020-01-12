/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <stdio.h>

#include <memory>
#include <string>

#include "ETMDecoder.h"
#include "command.h"
#include "record_file.h"
#include "thread_tree.h"
#include "utils.h"

using namespace simpleperf;

namespace {

using AddrPair = std::pair<uint64_t, uint64_t>;

struct AddrPairHash {
  size_t operator()(const AddrPair& ap) const noexcept {
    size_t seed = 0;
    HashCombine(seed, ap.first);
    HashCombine(seed, ap.second);
    return seed;
  }
};

struct BinaryInfo {
  std::unordered_map<AddrPair, uint64_t, AddrPairHash> range_count_map;
  std::unordered_map<AddrPair, uint64_t, AddrPairHash> branch_count_map;
};

class InjectCommand : public Command {
 public:
  InjectCommand()
      : Command("inject", "convert etm instruction tracing data into instr ranges",
                // clang-format off
"Usage: simpleperf inject [options]\n"
"--binary binary_name         Generate data only for binaries containing binary_name.\n"
"-i <file>                    input perf.data, generated by recording cs-etm event type.\n"
"                             Default is perf.data.\n"
"-o <file>                    output file. Default is perf_inject.data.\n"
"                             The output is in text format accepted by AutoFDO.\n"
"--dump-etm type1,type2,...   Dump etm data. A type is one of raw, packet and element.\n"
"--symdir <dir>               Look for binaries in a directory recursively.\n"
                // clang-format on
                ),
        output_fp_(nullptr, fclose) {}

  bool Run(const std::vector<std::string>& args) override {
    if (!ParseOptions(args)) {
      return false;
    }
    record_file_reader_ = RecordFileReader::CreateInstance(input_filename_);
    if (!record_file_reader_) {
      return false;
    }
    record_file_reader_->LoadBuildIdAndFileFeatures(thread_tree_);
    output_fp_.reset(fopen(output_filename_.c_str(), "w"));
    if (!output_fp_) {
      PLOG(ERROR) << "failed to write to " << output_filename_;
      return false;
    }
    if (!record_file_reader_->ReadDataSection([this](auto r) { return ProcessRecord(r.get()); })) {
      return false;
    }
    PostProcess();
    output_fp_.reset(nullptr);
    return true;
  }

 private:
  bool ParseOptions(const std::vector<std::string>& args) {
    for (size_t i = 0; i < args.size(); i++) {
      if (args[i] == "--binary") {
        if (!NextArgumentOrError(args, &i)) {
          return false;
        }
        binary_name_filter_ = args[i];
      } else if (args[i] == "-i") {
        if (!NextArgumentOrError(args, &i)) {
          return false;
        }
        input_filename_ = args[i];
      } else if (args[i] == "-o") {
        if (!NextArgumentOrError(args, &i)) {
          return false;
        }
        output_filename_ = args[i];
      } else if (args[i] == "--dump-etm") {
        if (!NextArgumentOrError(args, &i) || !ParseEtmDumpOption(args[i], &etm_dump_option_)) {
          return false;
        }
      } else if (args[i] == "--symdir") {
        if (!NextArgumentOrError(args, &i) || !Dso::AddSymbolDir(args[i])) {
          return false;
        }
      } else {
        ReportUnknownOption(args, i);
        return false;
      }
    }
    return true;
  }

  bool ProcessRecord(Record* r) {
    thread_tree_.Update(*r);
    if (r->type() == PERF_RECORD_AUXTRACE_INFO) {
      auto instr_range_callback = [this](auto& range) { ProcessInstrRange(range); };
      etm_decoder_ = ETMDecoder::Create(*static_cast<AuxTraceInfoRecord*>(r), thread_tree_);
      if (!etm_decoder_) {
        return false;
      }
      etm_decoder_->EnableDump(etm_dump_option_);
      etm_decoder_->RegisterCallback(instr_range_callback);
    } else if (r->type() == PERF_RECORD_AUX) {
      AuxRecord* aux = static_cast<AuxRecord*>(r);
      uint64_t aux_size = aux->data->aux_size;
      if (aux_size > 0) {
        if (aux_data_buffer_.size() < aux_size) {
          aux_data_buffer_.resize(aux_size);
        }
        if (!record_file_reader_->ReadAuxData(aux->Cpu(), aux->data->aux_offset,
                                              aux_data_buffer_.data(), aux_size)) {
          LOG(ERROR) << "failed to read aux data";
          return false;
        }
        return etm_decoder_->ProcessData(aux_data_buffer_.data(), aux_size);
      }
    }
    return true;
  }

  void ProcessInstrRange(const ETMInstrRange& instr_range) {
    if (instr_range.dso->GetDebugFilePath().find(binary_name_filter_) == std::string::npos) {
      return;
    }
    auto& binary = binary_map_[instr_range.dso->GetDebugFilePath()];
    binary.range_count_map[AddrPair(instr_range.start_addr, instr_range.end_addr)] +=
        instr_range.branch_taken_count + instr_range.branch_not_taken_count;
    if (instr_range.branch_taken_count > 0) {
      binary.branch_count_map[AddrPair(instr_range.end_addr, instr_range.branch_to_addr)] +=
          instr_range.branch_taken_count;
    }
  }

  void PostProcess() {
    for (const auto& pair : binary_map_) {
      const std::string& binary_path = pair.first;
      const BinaryInfo& binary = pair.second;

      // Write range_count_map.
      fprintf(output_fp_.get(), "%zu\n", binary.range_count_map.size());
      for (const auto& pair2 : binary.range_count_map) {
        const AddrPair& addr_range = pair2.first;
        uint64_t count = pair2.second;

        fprintf(output_fp_.get(), "%" PRIx64 "-%" PRIx64 ":%" PRIu64 "\n", addr_range.first,
                addr_range.second, count);
      }

      // Write addr_count_map.
      fprintf(output_fp_.get(), "0\n");

      // Write branch_count_map.
      fprintf(output_fp_.get(), "%zu\n", binary.branch_count_map.size());
      for (const auto& pair2 : binary.branch_count_map) {
        const AddrPair& branch = pair2.first;
        uint64_t count = pair2.second;

        fprintf(output_fp_.get(), "%" PRIx64 "->%" PRIx64 ":%" PRIu64 "\n", branch.first,
                branch.second, count);
      }

      // Write the binary path in comment.
      fprintf(output_fp_.get(), "// %s\n\n", binary_path.c_str());
    }
  }

  std::string binary_name_filter_;
  std::string input_filename_ = "perf.data";
  std::string output_filename_ = "perf_inject.data";
  ThreadTree thread_tree_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  ETMDumpOption etm_dump_option_;
  std::unique_ptr<ETMDecoder> etm_decoder_;
  std::vector<uint8_t> aux_data_buffer_;
  std::unique_ptr<FILE, decltype(&fclose)> output_fp_;

  // Store results for AutoFDO.
  std::unordered_map<std::string, BinaryInfo> binary_map_;
};

}  // namespace

void RegisterInjectCommand() {
  return RegisterCommand("inject", [] { return std::unique_ptr<Command>(new InjectCommand); });
}
