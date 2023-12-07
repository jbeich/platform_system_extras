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

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>
#include <optional>
#include <string>

#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "BranchListFile.h"
#include "ETMDecoder.h"
#include "RegEx.h"
#include "command.h"
#include "record_file.h"
#include "system/extras/simpleperf/branch_list.pb.h"
#include "thread_tree.h"
#include "utils.h"

namespace simpleperf {

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

enum class OutputFormat {
  AutoFDO,
  BranchList,
};

struct AutoFDOBinaryInfo {
  uint64_t first_load_segment_addr = 0;
  std::unordered_map<uint64_t, uint64_t> address_count_map;
  std::unordered_map<AddrPair, uint64_t, AddrPairHash> range_count_map;
  std::unordered_map<AddrPair, uint64_t, AddrPairHash> branch_count_map;

  void AddAddress(uint64_t addr) { OverflowSafeAdd(address_count_map[addr], 1); }

  void AddRange(uint64_t begin, uint64_t end) {
    OverflowSafeAdd(range_count_map[std::make_pair(begin, end)], 1);
  }

  void AddBranch(uint64_t from, uint64_t to) {
    OverflowSafeAdd(branch_count_map[std::make_pair(from, to)], 1);
  }

  void AddInstrRange(const ETMInstrRange& instr_range) {
    uint64_t total_count = instr_range.branch_taken_count;
    OverflowSafeAdd(total_count, instr_range.branch_not_taken_count);
    OverflowSafeAdd(range_count_map[AddrPair(instr_range.start_addr, instr_range.end_addr)],
                    total_count);
    if (instr_range.branch_taken_count > 0) {
      OverflowSafeAdd(branch_count_map[AddrPair(instr_range.end_addr, instr_range.branch_to_addr)],
                      instr_range.branch_taken_count);
    }
  }

  void Merge(const AutoFDOBinaryInfo& other) {
    for (const auto& p : other.address_count_map) {
      auto res = address_count_map.emplace(p.first, p.second);
      if (!res.second) {
        OverflowSafeAdd(res.first->second, p.second);
      }
    }
    for (const auto& p : other.range_count_map) {
      auto res = range_count_map.emplace(p.first, p.second);
      if (!res.second) {
        OverflowSafeAdd(res.first->second, p.second);
      }
    }
    for (const auto& p : other.branch_count_map) {
      auto res = branch_count_map.emplace(p.first, p.second);
      if (!res.second) {
        OverflowSafeAdd(res.first->second, p.second);
      }
    }
  }
};

using AutoFDOBinaryCallback = std::function<void(const BinaryKey&, AutoFDOBinaryInfo&)>;
using ETMBinaryCallback = std::function<void(const BinaryKey&, ETMBinary&)>;
using LBRDataCallback = std::function<void(LBRData&)>;

static uint64_t GetFirstLoadSegmentVaddr(const Dso* dso) {
  ElfStatus status;
  if (auto elf = ElfFile::Open(dso->GetDebugFilePath(), &status); elf) {
    for (const auto& segment : elf->GetProgramHeader()) {
      if (segment.is_load) {
        return segment.vaddr;
      }
    }
  }
  return 0;
}

// Base class for reading perf.data and generating AutoFDO or branch list data.
class PerfDataReader {
 public:
  static std::string GetDataType(RecordFileReader& reader) {
    const EventAttrIds& attrs = reader.AttrSection();
    if (attrs.size() != 1) {
      return "unknown";
    }
    const perf_event_attr& attr = attrs[0].attr;
    if (IsEtmEventType(attr.type)) {
      return "etm";
    }
    if (attr.sample_type & PERF_SAMPLE_BRANCH_STACK) {
      return "lbr";
    }
    return "unknown";
  }

  PerfDataReader(std::unique_ptr<RecordFileReader> reader, bool exclude_perf,
                 const RegEx* binary_name_regex)
      : reader_(std::move(reader)),
        exclude_perf_(exclude_perf),
        binary_filter_(binary_name_regex) {}
  virtual ~PerfDataReader() {}

  std::string GetDataType() const { return GetDataType(*reader_); }

  void SetCallback(const AutoFDOBinaryCallback& callback) { autofdo_callback_ = callback; }
  virtual void SetCallback(const ETMBinaryCallback&) {}
  virtual void SetCallback(const LBRDataCallback&) {}

  virtual bool Read() {
    if (exclude_perf_) {
      const auto& info_map = reader_->GetMetaInfoFeature();
      if (auto it = info_map.find("recording_process"); it == info_map.end()) {
        LOG(ERROR) << reader_->FileName() << " doesn't support --exclude-perf";
        return false;
      } else {
        int pid;
        if (!android::base::ParseInt(it->second, &pid, 0)) {
          LOG(ERROR) << "invalid recording_process " << it->second << " in " << reader_->FileName();
          return false;
        }
        exclude_pid_ = pid;
      }
    }

    if (!reader_->LoadBuildIdAndFileFeatures(thread_tree_)) {
      return false;
    }
    if (!reader_->ReadDataSection([this](auto r) { return ProcessRecord(*r); })) {
      return false;
    }
    return PostProcess();
  }

 protected:
  virtual bool ProcessRecord(Record& r) = 0;
  virtual bool PostProcess() = 0;

  void ProcessAutoFDOBinaryInfo() {
    for (auto& p : autofdo_binary_map_) {
      const Dso* dso = p.first;
      AutoFDOBinaryInfo& binary = p.second;
      binary.first_load_segment_addr = GetFirstLoadSegmentVaddr(dso);
      autofdo_callback_(BinaryKey(dso, 0), binary);
    }
  }

  const std::string data_type_;
  std::unique_ptr<RecordFileReader> reader_;
  bool exclude_perf_;
  BinaryFilter binary_filter_;

  std::optional<int> exclude_pid_;
  ThreadTree thread_tree_;
  AutoFDOBinaryCallback autofdo_callback_;
  // Store results for AutoFDO.
  std::unordered_map<const Dso*, AutoFDOBinaryInfo> autofdo_binary_map_;
};

class ETMThreadTreeWithFilter : public ETMThreadTree {
 public:
  ETMThreadTreeWithFilter(ThreadTree& thread_tree, std::optional<int>& exclude_pid)
      : thread_tree_(thread_tree), exclude_pid_(exclude_pid) {}

  void DisableThreadExitRecords() override { thread_tree_.DisableThreadExitRecords(); }

  const ThreadEntry* FindThread(int tid) override {
    const ThreadEntry* thread = thread_tree_.FindThread(tid);
    if (thread != nullptr && exclude_pid_ && thread->pid == exclude_pid_) {
      return nullptr;
    }
    return thread;
  }

  const MapSet& GetKernelMaps() override { return thread_tree_.GetKernelMaps(); }

 private:
  ThreadTree& thread_tree_;
  std::optional<int>& exclude_pid_;
};

// Read perf.data with ETM data and generate AutoFDO or branch list data.
class ETMPerfDataReader : public PerfDataReader {
 public:
  ETMPerfDataReader(std::unique_ptr<RecordFileReader> reader, bool exclude_perf,
                    const RegEx* binary_name_regex, ETMDumpOption etm_dump_option)
      : PerfDataReader(std::move(reader), exclude_perf, binary_name_regex),
        etm_dump_option_(etm_dump_option),
        etm_thread_tree_(thread_tree_, exclude_pid_) {}

  void SetCallback(const ETMBinaryCallback& callback) override { etm_binary_callback_ = callback; }

  bool Read() override {
    if (reader_->HasFeature(PerfFileFormat::FEAT_ETM_BRANCH_LIST)) {
      return ProcessETMBranchListFeature();
    }
    return PerfDataReader::Read();
  }

 private:
  bool ProcessRecord(Record& r) override {
    thread_tree_.Update(r);
    if (r.type() == PERF_RECORD_AUXTRACE_INFO) {
      etm_decoder_ = ETMDecoder::Create(static_cast<AuxTraceInfoRecord&>(r), etm_thread_tree_);
      if (!etm_decoder_) {
        return false;
      }
      etm_decoder_->EnableDump(etm_dump_option_);
      if (autofdo_callback_) {
        etm_decoder_->RegisterCallback(
            [this](const ETMInstrRange& range) { ProcessInstrRange(range); });
      } else if (etm_binary_callback_) {
        etm_decoder_->RegisterCallback(
            [this](const ETMBranchList& branch) { ProcessETMBranchList(branch); });
      }
    } else if (r.type() == PERF_RECORD_AUX) {
      AuxRecord& aux = static_cast<AuxRecord&>(r);
      if (aux.data->aux_size > SIZE_MAX) {
        LOG(ERROR) << "invalid aux size";
        return false;
      }
      size_t aux_size = aux.data->aux_size;
      if (aux_size > 0) {
        bool error = false;
        if (!reader_->ReadAuxData(aux.Cpu(), aux.data->aux_offset, aux_size, aux_data_buffer_,
                                  error)) {
          return !error;
        }
        if (!etm_decoder_) {
          LOG(ERROR) << "ETMDecoder isn't created";
          return false;
        }
        return etm_decoder_->ProcessData(aux_data_buffer_.data(), aux_size, !aux.Unformatted(),
                                         aux.Cpu());
      }
    } else if (r.type() == PERF_RECORD_MMAP && r.InKernel()) {
      auto& mmap_r = static_cast<MmapRecord&>(r);
      if (android::base::StartsWith(mmap_r.filename, DEFAULT_KERNEL_MMAP_NAME)) {
        kernel_map_start_addr_ = mmap_r.data->addr;
      }
    }
    return true;
  }

  bool PostProcess() override {
    if (etm_decoder_ && !etm_decoder_->FinishData()) {
      return false;
    }
    if (autofdo_callback_) {
      ProcessAutoFDOBinaryInfo();
    } else if (etm_binary_callback_) {
      ProcessETMBinary();
    }
    return true;
  }

  bool ProcessETMBranchListFeature() {
    if (exclude_perf_) {
      LOG(WARNING) << "--exclude-perf has no effect on perf.data with etm branch list";
    }
    if (autofdo_callback_) {
      LOG(ERROR) << "convert to autofdo format isn't support on perf.data with etm branch list";
      return false;
    }
    CHECK(etm_binary_callback_);
    std::string s;
    if (!reader_->ReadFeatureSection(PerfFileFormat::FEAT_ETM_BRANCH_LIST, &s)) {
      return false;
    }
    ETMBinaryMap binary_map;
    if (!StringToETMBinaryMap(s, binary_map)) {
      return false;
    }
    for (auto& [key, binary] : binary_map) {
      if (!binary_filter_.Filter(key.path)) {
        continue;
      }
      etm_binary_callback_(key, binary);
    }
    return true;
  }

  void ProcessInstrRange(const ETMInstrRange& instr_range) {
    if (!binary_filter_.Filter(instr_range.dso)) {
      return;
    }

    autofdo_binary_map_[instr_range.dso].AddInstrRange(instr_range);
  }

  void ProcessETMBranchList(const ETMBranchList& branch_list) {
    if (!binary_filter_.Filter(branch_list.dso)) {
      return;
    }

    auto& branch_map = etm_binary_map_[branch_list.dso].branch_map;
    ++branch_map[branch_list.addr][branch_list.branch];
  }

  void ProcessETMBinary() {
    for (auto& p : etm_binary_map_) {
      Dso* dso = p.first;
      ETMBinary& binary = p.second;
      binary.dso_type = dso->type();
      BinaryKey key(dso, 0);
      if (binary.dso_type == DSO_KERNEL) {
        if (kernel_map_start_addr_ == 0) {
          LOG(WARNING) << "Can't convert kernel ip addresses without kernel start addr. So remove "
                          "branches for the kernel.";
          continue;
        }
        if (dso->GetDebugFilePath() == dso->Path()) {
          // vmlinux isn't available. We still use kernel ip addr. Put kernel start addr in proto
          // for address conversion later.
          key.kernel_start_addr = kernel_map_start_addr_;
        }
      }
      etm_binary_callback_(key, binary);
    }
  }

  ETMDumpOption etm_dump_option_;
  ETMBinaryCallback etm_binary_callback_;
  ETMThreadTreeWithFilter etm_thread_tree_;
  std::vector<uint8_t> aux_data_buffer_;
  std::unique_ptr<ETMDecoder> etm_decoder_;
  uint64_t kernel_map_start_addr_ = 0;
  // Store etm branch list data.
  std::unordered_map<Dso*, ETMBinary> etm_binary_map_;
};

class LBRPerfDataReader : public PerfDataReader {
 public:
  LBRPerfDataReader(std::unique_ptr<RecordFileReader> reader, bool exclude_perf,
                    const RegEx* binary_name_regex)
      : PerfDataReader(std::move(reader), exclude_perf, binary_name_regex) {}
  void SetCallback(const LBRDataCallback& callback) override { lbr_data_callback_ = callback; }

 private:
  bool ProcessRecord(Record& r) override {
    thread_tree_.Update(r);
    if (r.type() == PERF_RECORD_SAMPLE) {
      auto& sr = static_cast<SampleRecord&>(r);
      ThreadEntry* thread = thread_tree_.FindThread(sr.tid_data.tid);
      if (thread == nullptr) {
        return true;
      }
      auto& stack = sr.branch_stack_data;
      lbr_data_.samples.resize(lbr_data_.samples.size() + 1);
      LBRSample& sample = lbr_data_.samples.back();
      std::pair<uint32_t, uint64_t> binary_addr = IpToBinaryAddr(*thread, sr.ip_data.ip);
      sample.binary_id = binary_addr.first;
      bool has_valid_binary_id = sample.binary_id != 0;
      sample.vaddr_in_file = binary_addr.second;
      sample.branches.resize(stack.stack_nr);
      for (size_t i = 0; i < stack.stack_nr; ++i) {
        uint64_t from_ip = stack.stack[i].from;
        uint64_t to_ip = stack.stack[i].to;
        LBRBranch& branch = sample.branches[i];
        binary_addr = IpToBinaryAddr(*thread, from_ip);
        branch.from_binary_id = binary_addr.first;
        branch.from_vaddr_in_file = binary_addr.second;
        binary_addr = IpToBinaryAddr(*thread, to_ip);
        branch.to_binary_id = binary_addr.first;
        branch.to_vaddr_in_file = binary_addr.second;
        if (branch.from_binary_id != 0 || branch.to_binary_id != 0) {
          has_valid_binary_id = true;
        }
      }
      if (!has_valid_binary_id) {
        lbr_data_.samples.pop_back();
      }
    }
    return true;
  }

  bool PostProcess() override {
    if (autofdo_callback_) {
      ConvertLBRDataToAutoFDO();
      ProcessAutoFDOBinaryInfo();
    }
    return true;
  }

  std::pair<uint32_t, uint64_t> IpToBinaryAddr(ThreadEntry& thread, uint64_t ip) {
    const MapEntry* map = thread_tree_.FindMap(&thread, ip);
    Dso* dso = map->dso;
    if (thread_tree_.IsUnknownDso(dso) || !binary_filter_.Filter(dso)) {
      return std::make_pair(0, 0);
    }
    uint32_t binary_id = GetBinaryId(dso);
    uint64_t vaddr_in_file = dso->IpToVaddrInFile(ip, map->start_addr, map->pgoff);
    return std::make_pair(binary_id, vaddr_in_file);
  }

  uint32_t GetBinaryId(const Dso* dso) {
    if (auto it = dso_map_.find(dso); it != dso_map_.end()) {
      return it->second;
    }
    uint32_t binary_id = static_cast<uint32_t>(lbr_data_.binaries.size() + 1);
    dso_map_[dso] = binary_id;
    return binary_id;
  }

  void ConvertLBRDataToAutoFDO() {
    std::vector<AutoFDOBinaryInfo> binaries(dso_map_.size());
    for (const LBRSample& sample : lbr_data_.samples) {
      if (sample.binary_id != 0) {
        binaries[sample.binary_id - 1].AddAddress(sample.vaddr_in_file);
      }
      for (size_t i = 0; i < sample.branches.size(); ++i) {
        const LBRBranch& branch = sample.branches[i];
        if (branch.from_binary_id == 0) {
          continue;
        }
        if (branch.from_binary_id == branch.to_binary_id) {
          binaries[branch.from_binary_id - 1].AddBranch(branch.from_vaddr_in_file,
                                                        branch.to_vaddr_in_file);
        }
        if (i > 0 && branch.from_binary_id == sample.branches[i - 1].to_binary_id) {
          uint64_t begin = sample.branches[i - 1].to_vaddr_in_file;
          uint64_t end = branch.from_vaddr_in_file;
          // Use the same logic to skip bogus LBR data as AutoFDO.
          if (end < begin || end - begin > (1 << 20)) {
            continue;
          }
          binaries[branch.from_binary_id - 1].AddRange(begin, end);
        }
      }
    }
    for (const auto& [dso, binary_id] : dso_map_) {
      autofdo_binary_map_[dso] = std::move(binaries[binary_id - 1]);
    }
  }

  LBRDataCallback lbr_data_callback_;
  LBRData lbr_data_;
  // Map from dso to binary_id in lbr_data_.
  std::unordered_map<const Dso*, uint32_t> dso_map_;
};

// Read a protobuf file specified by etm_branch_list.proto, and generate ETMBinary.
class ETMBranchListReader {
 public:
  ETMBranchListReader(const std::string& filename, const RegEx* binary_name_regex)
      : filename_(filename), binary_filter_(binary_name_regex) {}

  void SetCallback(const ETMBinaryCallback& callback) { callback_ = callback; }

  bool Read() {
    std::string s;
    if (!android::base::ReadFileToString(filename_, &s)) {
      PLOG(ERROR) << "failed to read " << filename_;
      return false;
    }
    ETMBinaryMap binary_map;
    if (!StringToETMBinaryMap(s, binary_map)) {
      PLOG(ERROR) << "file is in wrong format: " << filename_;
      return false;
    }
    for (auto& [key, binary] : binary_map) {
      if (!binary_filter_.Filter(key.path)) {
        continue;
      }
      callback_(key, binary);
    }
    return true;
  }

 private:
  const std::string filename_;
  BinaryFilter binary_filter_;
  ETMBinaryCallback callback_;
};

// Convert ETMBinary into AutoFDOBinaryInfo.
class ETMBranchListToAutoFDOConverter {
 public:
  std::unique_ptr<AutoFDOBinaryInfo> Convert(const BinaryKey& key, ETMBinary& binary) {
    BuildId build_id = key.build_id;
    std::unique_ptr<Dso> dso = Dso::CreateDsoWithBuildId(binary.dso_type, key.path, build_id);
    if (!dso || !CheckBuildId(dso.get(), key.build_id)) {
      return nullptr;
    }
    std::unique_ptr<AutoFDOBinaryInfo> autofdo_binary(new AutoFDOBinaryInfo);
    autofdo_binary->first_load_segment_addr = GetFirstLoadSegmentVaddr(dso.get());

    if (dso->type() == DSO_KERNEL) {
      ModifyBranchMapForKernel(dso.get(), key.kernel_start_addr, binary);
    }

    auto process_instr_range = [&](const ETMInstrRange& range) {
      CHECK_EQ(range.dso, dso.get());
      autofdo_binary->AddInstrRange(range);
    };

    auto result = ConvertETMBranchMapToInstrRanges(dso.get(), binary.GetOrderedBranchMap(),
                                                   process_instr_range);
    if (!result.ok()) {
      LOG(WARNING) << "failed to build instr ranges for binary " << dso->Path() << ": "
                   << result.error();
      return nullptr;
    }
    return autofdo_binary;
  }

 private:
  bool CheckBuildId(Dso* dso, const BuildId& expected_build_id) {
    if (expected_build_id.IsEmpty()) {
      return true;
    }
    BuildId build_id;
    return GetBuildIdFromDsoPath(dso->GetDebugFilePath(), &build_id) &&
           build_id == expected_build_id;
  }

  void ModifyBranchMapForKernel(Dso* dso, uint64_t kernel_start_addr, ETMBinary& binary) {
    if (kernel_start_addr == 0) {
      // vmlinux has been provided when generating branch lists. Addresses in branch lists are
      // already vaddrs in vmlinux.
      return;
    }
    // Addresses are still kernel ip addrs in memory. Need to convert them to vaddrs in vmlinux.
    UnorderedETMBranchMap new_branch_map;
    for (auto& p : binary.branch_map) {
      uint64_t vaddr_in_file = dso->IpToVaddrInFile(p.first, kernel_start_addr, 0);
      new_branch_map[vaddr_in_file] = std::move(p.second);
    }
    binary.branch_map = std::move(new_branch_map);
  }
};

// Write instruction ranges to a file in AutoFDO text format.
class AutoFDOWriter {
 public:
  void AddAutoFDOBinary(const BinaryKey& key, AutoFDOBinaryInfo& binary) {
    auto it = binary_map_.find(key);
    if (it == binary_map_.end()) {
      binary_map_[key] = std::move(binary);
    } else {
      it->second.Merge(binary);
    }
  }

  bool Write(const std::string& output_filename) {
    std::unique_ptr<FILE, decltype(&fclose)> output_fp(fopen(output_filename.c_str(), "w"), fclose);
    if (!output_fp) {
      PLOG(ERROR) << "failed to write to " << output_filename;
      return false;
    }
    // autofdo_binary_map is used to store instruction ranges, which can have a large amount. And
    // it has a larger access time (instruction ranges * executed time). So it's better to use
    // unorder_maps to speed up access time. But we also want a stable output here, to compare
    // output changes result from code changes. So generate a sorted output here.
    std::vector<BinaryKey> keys;
    for (auto& p : binary_map_) {
      keys.emplace_back(p.first);
    }
    std::sort(keys.begin(), keys.end(),
              [](const BinaryKey& key1, const BinaryKey& key2) { return key1.path < key2.path; });
    if (keys.size() > 1) {
      fprintf(output_fp.get(),
              "// Please split this file. AutoFDO only accepts profile for one binary.\n");
    }
    for (const auto& key : keys) {
      const AutoFDOBinaryInfo& binary = binary_map_[key];
      // AutoFDO text format needs file_offsets instead of virtual addrs in a binary. And it uses
      // below formula: vaddr = file_offset + GetFirstLoadSegmentVaddr().
      uint64_t first_load_segment_addr = binary.first_load_segment_addr;

      auto to_offset = [&](uint64_t vaddr) -> uint64_t {
        if (vaddr == 0) {
          return 0;
        }
        CHECK_GE(vaddr, first_load_segment_addr);
        return vaddr - first_load_segment_addr;
      };

      // Write range_count_map.
      std::map<AddrPair, uint64_t> range_count_map(binary.range_count_map.begin(),
                                                   binary.range_count_map.end());
      fprintf(output_fp.get(), "%zu\n", range_count_map.size());
      for (const auto& pair2 : range_count_map) {
        const AddrPair& addr_range = pair2.first;
        uint64_t count = pair2.second;

        fprintf(output_fp.get(), "%" PRIx64 "-%" PRIx64 ":%" PRIu64 "\n",
                to_offset(addr_range.first), to_offset(addr_range.second), count);
      }

      // Write addr_count_map.
      std::map<uint64_t, uint64_t> address_count_map(binary.address_count_map.begin(),
                                                     binary.address_count_map.end());
      fprintf(output_fp.get(), "%zu\n", address_count_map.size());
      for (const auto& [addr, count] : address_count_map) {
        fprintf(output_fp.get(), "%" PRIx64 ":%" PRIu64 "\n", to_offset(addr), count);
      }

      // Write branch_count_map.
      std::map<AddrPair, uint64_t> branch_count_map(binary.branch_count_map.begin(),
                                                    binary.branch_count_map.end());
      fprintf(output_fp.get(), "%zu\n", branch_count_map.size());
      for (const auto& pair2 : branch_count_map) {
        const AddrPair& branch = pair2.first;
        uint64_t count = pair2.second;

        fprintf(output_fp.get(), "%" PRIx64 "->%" PRIx64 ":%" PRIu64 "\n", to_offset(branch.first),
                to_offset(branch.second), count);
      }

      // Write the binary path in comment.
      fprintf(output_fp.get(), "// build_id: %s\n", key.build_id.ToString().c_str());
      fprintf(output_fp.get(), "// %s\n\n", key.path.c_str());
    }
    return true;
  }

 private:
  std::unordered_map<BinaryKey, AutoFDOBinaryInfo, BinaryKeyHash> binary_map_;
};

// Merge ETMBinary.
struct ETMBranchListMerger {
  void AddETMBinary(const BinaryKey& key, ETMBinary& binary) {
    auto it = binary_map.find(key);
    if (it == binary_map.end()) {
      binary_map[key] = std::move(binary);
    } else {
      it->second.Merge(binary);
    }
  }

  ETMBinaryMap binary_map;
};

// Write branch lists to a protobuf file specified by etm_branch_list.proto.
class ETMBranchListWriter {
 public:
  bool Write(const std::string& output_filename, const ETMBinaryMap& binary_map) {
    // Don't produce empty output file.
    if (binary_map.empty()) {
      LOG(INFO) << "Skip empty output file.";
      unlink(output_filename.c_str());
      return true;
    }
    std::string s;
    if (!ETMBinaryMapToString(binary_map, s)) {
      LOG(ERROR) << "invalid ETMBinaryMap";
      return false;
    }
    if (!android::base::WriteStringToFile(s, output_filename)) {
      PLOG(ERROR) << "failed to write to " << output_filename;
      return false;
    }
    return true;
  }
};

class InjectCommand : public Command {
 public:
  InjectCommand()
      : Command("inject", "parse etm instruction tracing data",
                // clang-format off
"Usage: simpleperf inject [options]\n"
"--binary binary_name         Generate data only for binaries matching binary_name regex.\n"
"-i file1,file2,...           Input files. Default is perf.data. Support below formats:\n"
"                               1. perf.data generated by recording cs-etm event type.\n"
"                               2. branch_list file generated by `inject --output branch-list`.\n"
"                             If a file name starts with @, it contains a list of input files.\n"
"-o <file>                    output file. Default is perf_inject.data.\n"
"--output <format>            Select output file format:\n"
"                               autofdo      -- text format accepted by TextSampleReader\n"
"                                               of AutoFDO\n"
"                               branch-list  -- protobuf file in etm_branch_list.proto\n"
"                             Default is autofdo.\n"
"--dump-etm type1,type2,...   Dump etm data. A type is one of raw, packet and element.\n"
"--exclude-perf               Exclude trace data for the recording process.\n"
"--symdir <dir>               Look for binaries in a directory recursively.\n"
"\n"
"Examples:\n"
"1. Generate autofdo text output.\n"
"$ simpleperf inject -i perf.data -o autofdo.txt --output autofdo\n"
"\n"
"2. Generate branch list proto, then convert to autofdo text.\n"
"$ simpleperf inject -i perf.data -o branch_list.data --output branch-list\n"
"$ simpleperf inject -i branch_list.data -o autofdo.txt --output autofdo\n"
                // clang-format on
        ) {}

  bool Run(const std::vector<std::string>& args) override {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    if (!ParseOptions(args)) {
      return false;
    }

    CHECK(!input_filenames_.empty());
    if (IsPerfDataFile(input_filenames_[0])) {
      switch (output_format_) {
        case OutputFormat::AutoFDO:
          return ConvertPerfDataToAutoFDO();
        case OutputFormat::BranchList:
          return ConvertPerfDataToBranchList();
      }
    } else {
      switch (output_format_) {
        case OutputFormat::AutoFDO:
          return ConvertBranchListToAutoFDO();
        case OutputFormat::BranchList:
          return ConvertBranchListToBranchList();
      }
    }
  }

 private:
  bool ParseOptions(const std::vector<std::string>& args) {
    const OptionFormatMap option_formats = {
        {"--binary", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--dump-etm", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--exclude-perf", {OptionValueType::NONE, OptionType::SINGLE}},
        {"-i", {OptionValueType::STRING, OptionType::MULTIPLE}},
        {"-o", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--output", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--symdir", {OptionValueType::STRING, OptionType::MULTIPLE}},
    };
    OptionValueMap options;
    std::vector<std::pair<OptionName, OptionValue>> ordered_options;
    if (!PreprocessOptions(args, option_formats, &options, &ordered_options, nullptr)) {
      return false;
    }

    if (auto value = options.PullValue("--binary"); value) {
      binary_name_regex_ = RegEx::Create(*value->str_value);
      if (binary_name_regex_ == nullptr) {
        return false;
      }
    }
    if (auto value = options.PullValue("--dump-etm"); value) {
      if (!ParseEtmDumpOption(*value->str_value, &etm_dump_option_)) {
        return false;
      }
    }
    exclude_perf_ = options.PullBoolValue("--exclude-perf");

    for (const OptionValue& value : options.PullValues("-i")) {
      std::vector<std::string> files = android::base::Split(*value.str_value, ",");
      for (std::string& file : files) {
        if (android::base::StartsWith(file, "@")) {
          if (!ReadFileList(file.substr(1), &input_filenames_)) {
            return false;
          }
        } else {
          input_filenames_.emplace_back(file);
        }
      }
    }
    if (input_filenames_.empty()) {
      input_filenames_.emplace_back("perf.data");
    }
    options.PullStringValue("-o", &output_filename_);
    if (auto value = options.PullValue("--output"); value) {
      const std::string& output = *value->str_value;
      if (output == "autofdo") {
        output_format_ = OutputFormat::AutoFDO;
      } else if (output == "branch-list") {
        output_format_ = OutputFormat::BranchList;
      } else {
        LOG(ERROR) << "unknown format in --output option: " << output;
        return false;
      }
    }
    if (auto value = options.PullValue("--symdir"); value) {
      if (!Dso::AddSymbolDir(*value->str_value)) {
        return false;
      }
      // Symbol dirs are cleaned when Dso count is decreased to zero, which can happen between
      // processing input files. To make symbol dirs always available, create a placeholder dso to
      // prevent cleaning from happening.
      placeholder_dso_ = Dso::CreateDso(DSO_UNKNOWN_FILE, "unknown");
    }
    CHECK(options.values.empty());
    return true;
  }

  bool ReadFileList(const std::string& path, std::vector<std::string>* file_list) {
    std::string data;
    if (!android::base::ReadFileToString(path, &data)) {
      PLOG(ERROR) << "failed to read " << path;
      return false;
    }
    std::vector<std::string> tokens = android::base::Tokenize(data, " \t\n\r");
    file_list->insert(file_list->end(), tokens.begin(), tokens.end());
    return true;
  }

  bool ReadPerfDataFiles(const std::function<void(PerfDataReader&)> reader_callback) {
    if (input_filenames_.empty()) {
      return true;
    }

    std::string expected_data_type;
    for (const auto& filename : input_filenames_) {
      std::unique_ptr<RecordFileReader> file_reader = RecordFileReader::CreateInstance(filename);
      if (!file_reader) {
        return false;
      }
      std::string data_type = PerfDataReader::GetDataType(*file_reader);
      if (expected_data_type.empty()) {
        expected_data_type = data_type;
      } else if (expected_data_type != data_type) {
        LOG(ERROR) << "files have different data type: " << input_filenames_[0] << ", " << filename;
        return false;
      }
      std::unique_ptr<PerfDataReader> reader;
      if (data_type == "etm") {
        reader.reset(new ETMPerfDataReader(std::move(file_reader), exclude_perf_,
                                           binary_name_regex_.get(), etm_dump_option_));
      } else if (data_type == "lbr") {
        reader.reset(
            new LBRPerfDataReader(std::move(file_reader), exclude_perf_, binary_name_regex_.get()));
      } else {
        LOG(ERROR) << "unsupported data type " << data_type << " in " << filename;
        return false;
      }
      reader_callback(*reader);
      if (!reader->Read()) {
        return false;
      }
    }
    return true;
  }

  bool ConvertPerfDataToAutoFDO() {
    AutoFDOWriter autofdo_writer;
    auto afdo_callback = [&](const BinaryKey& key, AutoFDOBinaryInfo& binary) {
      autofdo_writer.AddAutoFDOBinary(key, binary);
    };
    auto reader_callback = [&](PerfDataReader& reader) { reader.SetCallback(afdo_callback); };
    if (!ReadPerfDataFiles(reader_callback)) {
      return false;
    }
    return autofdo_writer.Write(output_filename_);
  }

  bool ConvertPerfDataToBranchList() {
    ETMBranchListMerger branch_list_merger;
    auto etm_callback = [&](const BinaryKey& key, ETMBinary& binary) {
      branch_list_merger.AddETMBinary(key, binary);
    };
    auto reader_callback = [&](PerfDataReader& reader) { reader.SetCallback(etm_callback); };
    if (!ReadPerfDataFiles(reader_callback)) {
      return false;
    }
    ETMBranchListWriter branch_list_writer;
    return branch_list_writer.Write(output_filename_, branch_list_merger.binary_map);
  }

  bool ConvertBranchListToAutoFDO() {
    // Step1 : Merge branch lists from all input files.
    ETMBranchListMerger branch_list_merger;
    auto callback = [&](const BinaryKey& key, ETMBinary& binary) {
      branch_list_merger.AddETMBinary(key, binary);
    };
    for (const auto& input_filename : input_filenames_) {
      ETMBranchListReader reader(input_filename, binary_name_regex_.get());
      reader.SetCallback(callback);
      if (!reader.Read()) {
        return false;
      }
    }

    // Step2: Convert ETMBinary to AutoFDOBinaryInfo.
    AutoFDOWriter autofdo_writer;
    ETMBranchListToAutoFDOConverter converter;
    for (auto& p : branch_list_merger.binary_map) {
      const BinaryKey& key = p.first;
      ETMBinary& binary = p.second;
      std::unique_ptr<AutoFDOBinaryInfo> autofdo_binary = converter.Convert(key, binary);
      if (autofdo_binary) {
        // Create new BinaryKey with kernel_start_addr = 0. Because AutoFDO output doesn't care
        // kernel_start_addr.
        autofdo_writer.AddAutoFDOBinary(BinaryKey(key.path, key.build_id), *autofdo_binary);
      }
    }

    // Step3: Write AutoFDOBinaryInfo.
    return autofdo_writer.Write(output_filename_);
  }

  bool ConvertBranchListToBranchList() {
    // Step1 : Merge branch lists from all input files.
    ETMBranchListMerger branch_list_merger;
    auto callback = [&](const BinaryKey& key, ETMBinary& binary) {
      branch_list_merger.AddETMBinary(key, binary);
    };
    for (const auto& input_filename : input_filenames_) {
      ETMBranchListReader reader(input_filename, binary_name_regex_.get());
      reader.SetCallback(callback);
      if (!reader.Read()) {
        return false;
      }
    }
    // Step2: Write ETMBinary.
    ETMBranchListWriter branch_list_writer;
    return branch_list_writer.Write(output_filename_, branch_list_merger.binary_map);
  }

  std::unique_ptr<RegEx> binary_name_regex_;
  bool exclude_perf_ = false;
  std::vector<std::string> input_filenames_;
  std::string output_filename_ = "perf_inject.data";
  OutputFormat output_format_ = OutputFormat::AutoFDO;
  ETMDumpOption etm_dump_option_;

  std::unique_ptr<Dso> placeholder_dso_;
};

}  // namespace

void RegisterInjectCommand() {
  return RegisterCommand("inject", [] { return std::unique_ptr<Command>(new InjectCommand); });
}

}  // namespace simpleperf
