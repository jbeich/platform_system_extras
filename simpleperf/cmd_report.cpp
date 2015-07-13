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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <base/strings.h>

#include "command.h"
#include "environment.h"
#include "event_attr.h"
#include "event_type.h"
#include "record.h"
#include "record_file.h"
#include "sample_tree.h"

class Displayable {
 public:
  Displayable(const std::string& name) : name_(name), width_(name.size()) {
  }

  virtual ~Displayable() {
  }

  const std::string& Name() const {
    return name_;
  }
  size_t Width() const {
    return width_;
  }

  virtual std::string Show(const SampleEntry& sample) const = 0;
  void AdjustWidth(const SampleEntry& sample) {
    size_t size = Show(sample).size();
    width_ = std::max(width_, size);
  }

 private:
  const std::string name_;
  size_t width_;
};

class AccumulatedOverheadItem : public Displayable {
 public:
  AccumulatedOverheadItem(const SampleTree& sample_tree)
      : Displayable("Children"), sample_tree_(sample_tree) {
  }

  std::string Show(const SampleEntry& sample) const override {
    uint64_t period = sample.period + sample.accumulated_period;
    uint64_t total_period = sample_tree_.TotalPeriod();
    double percentage = (total_period != 0) ? 100.0 * period / total_period : 0.0;
    return android::base::StringPrintf("%.2lf%%", percentage);
  }

 private:
  const SampleTree& sample_tree_;
};

class SelfOverheadItem : public Displayable {
 public:
  SelfOverheadItem(const SampleTree& sample_tree, const std::string& name = "Self")
      : Displayable(name), sample_tree_(sample_tree) {
  }

  std::string Show(const SampleEntry& sample) const override {
    uint64_t period = sample.period;
    uint64_t total_period = sample_tree_.TotalPeriod();
    double percentage = (total_period != 0) ? 100.0 * period / total_period : 0.0;
    return android::base::StringPrintf("%.2lf%%", percentage);
  }

 private:
  const SampleTree& sample_tree_;
};

class SampleCountItem : public Displayable {
 public:
  SampleCountItem() : Displayable("Sample") {
  }

  std::string Show(const SampleEntry& sample) const override {
    return android::base::StringPrintf("%" PRId64, sample.sample_count);
  }
};

class Comparable {
 public:
  virtual ~Comparable() {
  }

  virtual int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const = 0;
};

class PidItem : public Displayable, public Comparable {
 public:
  PidItem() : Displayable("Pid") {
  }

  int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const override {
    return sample1.thread->pid - sample2.thread->pid;
  }

  std::string Show(const SampleEntry& sample) const override {
    return android::base::StringPrintf("%d", sample.thread->pid);
  }
};

class TidItem : public Displayable, public Comparable {
 public:
  TidItem() : Displayable("Tid") {
  }

  int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const override {
    return sample1.thread->tid - sample2.thread->tid;
  }

  std::string Show(const SampleEntry& sample) const override {
    return android::base::StringPrintf("%d", sample.thread->tid);
  }
};

class CommItem : public Displayable, public Comparable {
 public:
  CommItem() : Displayable("Command") {
  }

  int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const override {
    return strcmp(sample1.thread_comm, sample2.thread_comm);
  }

  std::string Show(const SampleEntry& sample) const override {
    return sample.thread_comm;
  }
};

class DsoItem : public Displayable, public Comparable {
 public:
  DsoItem(const std::string& name = "Shared Object") : Displayable(name) {
  }

  int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const override {
    return strcmp(sample1.map->dso->path.c_str(), sample2.map->dso->path.c_str());
  }

  std::string Show(const SampleEntry& sample) const override {
    return sample.map->dso->path;
  }
};

class SymbolItem : public Displayable, public Comparable {
 public:
  SymbolItem(const std::string& name = "Symbol") : Displayable(name) {
  }

  int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const override {
    return strcmp(sample1.symbol->name.c_str(), sample2.symbol->name.c_str());
  }

  std::string Show(const SampleEntry& sample) const override {
    return sample.symbol->name;
  }
};

class DsoFromItem : public Displayable, public Comparable {
 public:
  DsoFromItem() : Displayable("Source Shared Object") {
  }

  int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const override {
    return strcmp(sample1.branch_from.map->dso->path.c_str(),
                  sample2.branch_from.map->dso->path.c_str());
  }

  std::string Show(const SampleEntry& sample) const override {
    return sample.branch_from.map->dso->path;
  }
};

class DsoToItem : public DsoItem {
 public:
  DsoToItem() : DsoItem("Target Shared Object") {
  }
};

class SymbolFromItem : public Displayable, public Comparable {
 public:
  SymbolFromItem() : Displayable("Source Symbol") {
  }

  int Compare(const SampleEntry& sample1, const SampleEntry& sample2) const override {
    return strcmp(sample1.branch_from.symbol->name.c_str(),
                  sample2.branch_from.symbol->name.c_str());
  }

  std::string Show(const SampleEntry& sample) const override {
    return sample.branch_from.symbol->name;
  }
};

class SymbolToItem : public SymbolItem {
 public:
  SymbolToItem() : SymbolItem("Target Symbol") {
  }
};

static std::set<std::string> branch_sort_keys = {
    "dso_from", "dso_to", "symbol_from", "symbol_to",
};

class ReportCommand : public Command {
 public:
  ReportCommand()
      : Command(
            "report", "report sampling information in perf.data",
            "Usage: simpleperf report [options]\n"
            "    -b            Use the branch-to addresses in sampled take branches instead of\n"
            "                  the instruction addresses. Only valid for perf.data recorded with\n"
            "                  -b/-j option.\n"
            "    --children    Print the overhead accumulated by appearing in the callchain.\n"
            "    -g            Print call graph.\n"
            "    -i <file>     Specify path of record file, default is perf.data.\n"
            "    -n            Print the sample count for each item.\n"
            "    --no-demangle        Don't demangle symbol names.\n"
            "    --sort key1,key2,...\n"
            "                  Select the keys to sort and print the report. Possible keys\n"
            "                  include pid, tid, comm, dso, symbol, dso_from, dso_to, symbol_from\n"
            "                  symbol_to. dso_from, dso_to, symbol_from, symbol_to can only be\n"
            "                  used with -b option. Default keys are \"comm,pid,tid,dso,symbol\"\n"
            "    --symfs <dir> Look for files with symbols relative to this directory.\n"
            "    --vmlinux <file>\n"
            "                  Parse kernel symbols from <file>.\n"),
        record_filename_("perf.data"),
        use_branch_address_(false),
        accumulate_callchain_(false),
        print_callgraph_(false) {
    compare_sample_func_t compare_sample_callback = std::bind(
        &ReportCommand::CompareSampleEntry, this, std::placeholders::_1, std::placeholders::_2);
    sample_tree_ = std::unique_ptr<SampleTree>(new SampleTree(compare_sample_callback));
  }

  bool Run(const std::vector<std::string>& args);

 private:
  bool ParseOptions(const std::vector<std::string>& args);
  bool ReadEventAttrFromRecordFile();
  void ReadSampleTreeFromRecordFile();
  void ProcessSampleRecord(const SampleRecord& r);
  void ReadFeaturesFromRecordFile();
  int CompareSampleEntry(const SampleEntry& sample1, const SampleEntry& sample2);
  void PrintReport();
  void PrintReportContext();
  void CollectReportWidth();
  void CollectReportEntryWidth(const SampleEntry& sample);
  void PrintReportHeader();
  void PrintReportEntry(const SampleEntry& sample);
  void PrintCallGraph(const SampleEntry& sample);

  std::string record_filename_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  perf_event_attr event_attr_;
  std::vector<std::unique_ptr<Displayable>> displayable_items_;
  std::vector<Comparable*> comparable_items_;
  std::unique_ptr<SampleTree> sample_tree_;
  bool use_branch_address_;
  std::string record_cmdline_;
  bool accumulate_callchain_;
  bool print_callgraph_;
};

bool ReportCommand::Run(const std::vector<std::string>& args) {
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
  // Read features first to prepare build ids used when building SampleTree.
  ReadFeaturesFromRecordFile();
  ReadSampleTreeFromRecordFile();

  // 3. Show collected information.
  PrintReport();

  return true;
}

bool ReportCommand::ParseOptions(const std::vector<std::string>& args) {
  bool demangle = true;
  std::string symfs_dir;
  std::string vmlinux;
  bool print_sample_count = false;
  std::vector<std::string> sort_keys = {"comm", "pid", "tid", "dso", "symbol"};
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "-b") {
      use_branch_address_ = true;
    } else if (args[i] == "--children") {
      accumulate_callchain_ = true;
    } else if (args[i] == "-g") {
      print_callgraph_ = true;
      accumulate_callchain_ = true;
    } else if (args[i] == "-i") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      record_filename_ = args[i];

    } else if (args[i] == "-n") {
      print_sample_count = true;

    } else if (args[i] == "--no-demangle") {
      demangle = false;

    } else if (args[i] == "--sort") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      sort_keys = android::base::Split(args[i], ",");
    } else if (args[i] == "--symfs") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      symfs_dir = args[i];
    } else if (args[i] == "--vmlinux") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      vmlinux = args[i];
    } else {
      ReportUnknownOption(args, i);
      return false;
    }
  }

  DsoFactory::GetInstance()->SetDemangle(demangle);
  if (!DsoFactory::GetInstance()->SetSymFsDir(symfs_dir)) {
    return false;
  }
  if (!vmlinux.empty()) {
    DsoFactory::GetInstance()->SetVmlinux(vmlinux);
  }

  if (!accumulate_callchain_) {
    displayable_items_.push_back(
        std::unique_ptr<Displayable>(new SelfOverheadItem(*sample_tree_, "Overhead")));
  } else {
    displayable_items_.push_back(
        std::unique_ptr<Displayable>(new AccumulatedOverheadItem(*sample_tree_)));
    displayable_items_.push_back(std::unique_ptr<Displayable>(new SelfOverheadItem(*sample_tree_)));
  }
  if (print_sample_count) {
    displayable_items_.push_back(std::unique_ptr<Displayable>(new SampleCountItem));
  }
  for (auto& key : sort_keys) {
    if (!use_branch_address_ && branch_sort_keys.find(key) != branch_sort_keys.end()) {
      LOG(ERROR) << "sort key '" << key << "' can only be used with -b option.";
      return false;
    }
    if (key == "pid") {
      PidItem* item = new PidItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "tid") {
      TidItem* item = new TidItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "comm") {
      CommItem* item = new CommItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "dso") {
      DsoItem* item = new DsoItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "symbol") {
      SymbolItem* item = new SymbolItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "dso_from") {
      DsoFromItem* item = new DsoFromItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "dso_to") {
      DsoToItem* item = new DsoToItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "symbol_from") {
      SymbolFromItem* item = new SymbolFromItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else if (key == "symbol_to") {
      SymbolToItem* item = new SymbolToItem;
      displayable_items_.push_back(std::unique_ptr<Displayable>(item));
      comparable_items_.push_back(item);
    } else {
      LOG(ERROR) << "Unknown sort key: " << key;
      return false;
    }
  }
  return true;
}

bool ReportCommand::ReadEventAttrFromRecordFile() {
  std::vector<const PerfFileFormat::FileAttr*> attrs = record_file_reader_->AttrSection();
  if (attrs.size() != 1) {
    LOG(ERROR) << "record file contains " << attrs.size() << " attrs";
    return false;
  }
  event_attr_ = attrs[0]->attr;
  if (use_branch_address_ && (event_attr_.sample_type & PERF_SAMPLE_BRANCH_STACK) == 0) {
    LOG(ERROR) << record_filename_ << " is not recorded with branch stack sampling option.";
    return false;
  }
  return true;
}

void ReportCommand::ReadSampleTreeFromRecordFile() {
  sample_tree_->AddThread(0, 0, "swapper");

  std::vector<std::unique_ptr<const Record>> records = record_file_reader_->DataSection();
  for (auto& record : records) {
    if (record->header.type == PERF_RECORD_MMAP) {
      const MmapRecord& r = *static_cast<const MmapRecord*>(record.get());
      if ((r.header.misc & PERF_RECORD_MISC_CPUMODE_MASK) == PERF_RECORD_MISC_KERNEL) {
        sample_tree_->AddKernelMap(r.data.addr, r.data.len, r.data.pgoff,
                                   r.sample_id.time_data.time, r.filename);
      } else {
        sample_tree_->AddThreadMap(r.data.pid, r.data.tid, r.data.addr, r.data.len, r.data.pgoff,
                                   r.sample_id.time_data.time, r.filename);
      }
    } else if (record->header.type == PERF_RECORD_MMAP2) {
      const Mmap2Record& r = *static_cast<const Mmap2Record*>(record.get());
      if ((r.header.misc & PERF_RECORD_MISC_CPUMODE_MASK) == PERF_RECORD_MISC_KERNEL) {
        sample_tree_->AddKernelMap(r.data.addr, r.data.len, r.data.pgoff,
                                   r.sample_id.time_data.time, r.filename);
      } else {
        std::string filename =
            (r.filename == DEFAULT_EXECNAME_FOR_THREAD_MMAP) ? "[unknown]" : r.filename;
        sample_tree_->AddThreadMap(r.data.pid, r.data.tid, r.data.addr, r.data.len, r.data.pgoff,
                                   r.sample_id.time_data.time, filename);
      }
    } else if (record->header.type == PERF_RECORD_SAMPLE) {
      ProcessSampleRecord(*static_cast<const SampleRecord*>(record.get()));
    } else if (record->header.type == PERF_RECORD_COMM) {
      const CommRecord& r = *static_cast<const CommRecord*>(record.get());
      sample_tree_->AddThread(r.data.pid, r.data.tid, r.comm);
    } else if (record->header.type == PERF_RECORD_FORK) {
      const ForkRecord& r = *static_cast<const ForkRecord*>(record.get());
      sample_tree_->ForkThread(r.data.pid, r.data.tid, r.data.ppid, r.data.ptid);
    }
  }
}

void ReportCommand::ProcessSampleRecord(const SampleRecord& r) {
  if (use_branch_address_ && (r.sample_type & PERF_SAMPLE_BRANCH_STACK)) {
    for (auto& item : r.branch_stack_data.stack) {
      if (item.from != 0 && item.to != 0) {
        sample_tree_->AddBranchSample(r.tid_data.pid, r.tid_data.tid, item.from, item.to,
                                      item.flags, r.time_data.time, r.period_data.period);
      }
    }
  } else {
    bool in_kernel = (r.header.misc & PERF_RECORD_MISC_CPUMODE_MASK) == PERF_RECORD_MISC_KERNEL;
    SampleEntry* sample = sample_tree_->AddSample(r.tid_data.pid, r.tid_data.tid, r.ip_data.ip,
                                                  r.time_data.time, r.period_data.period, in_kernel);
    CHECK(sample != nullptr);
    if (accumulate_callchain_ && (r.sample_type & PERF_SAMPLE_CALLCHAIN) != 0) {
      std::vector<SampleEntry*> callchain;
      callchain.push_back(sample);
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
              LOG(ERROR) << "Unexpected perf_context in callchain: " << ip;
          }
        } else {
          if (first_ip) {
            // Remove duplication with sampled ip.
            if (ip == r.ip_data.ip) {
              continue;
            }
            first_ip = false;
          }
          SampleEntry* sample =
              sample_tree_->AddCallChainSample(r.tid_data.pid, r.tid_data.tid, ip, r.time_data.time,
                                               r.period_data.period, in_kernel, callchain);
          callchain.push_back(sample);
        }
      }
      if (print_callgraph_) {
        std::set<SampleEntry*> added_set;
        while (callchain.size() >= 2) {
          SampleEntry* sample = callchain[0];
          callchain.erase(callchain.begin());
          // Add only once for recursive calls on callchain.
          if (added_set.find(sample) != added_set.end()) {
            continue;
          }
          added_set.insert(sample);
          sample_tree_->InsertCallChainForSample(sample, callchain, r.period_data.period);
        }
      }
    }
  }
}

void ReportCommand::ReadFeaturesFromRecordFile() {
  std::vector<BuildIdRecord> records = record_file_reader_->ReadBuildIdFeature();
  std::vector<std::pair<std::string, BuildId>> build_ids;
  for (auto& r : records) {
    build_ids.push_back(std::make_pair(r.filename, r.build_id));
  }
  DsoFactory::GetInstance()->SetBuildIds(build_ids);
  std::vector<std::string> cmdline = record_file_reader_->ReadCmdlineFeature();
  if (!cmdline.empty()) {
    record_cmdline_ = android::base::Join(cmdline, ' ');
  }
}

int ReportCommand::CompareSampleEntry(const SampleEntry& sample1, const SampleEntry& sample2) {
  for (auto& item : comparable_items_) {
    int result = item->Compare(sample1, sample2);
    if (result != 0) {
      return result;
    }
  }
  return 0;
}

void ReportCommand::PrintReport() {
  PrintReportContext();
  CollectReportWidth();
  PrintReportHeader();
  sample_tree_->VisitAllSamples(
      std::bind(&ReportCommand::PrintReportEntry, this, std::placeholders::_1));
  fflush(stdout);
}

void ReportCommand::PrintReportContext() {
  const EventType* event_type = FindEventTypeByConfig(event_attr_.type, event_attr_.config);
  std::string event_type_name;
  if (event_type != nullptr) {
    event_type_name = event_type->name;
  } else {
    event_type_name =
        android::base::StringPrintf("(type %u, config %llu)", event_attr_.type, event_attr_.config);
  }
  if (!record_cmdline_.empty()) {
    printf("Cmdline: %s\n", record_cmdline_.c_str());
  }
  printf("Samples: %" PRIu64 " of event '%s'\n", sample_tree_->TotalSamples(),
         event_type_name.c_str());
  printf("Event count: %" PRIu64 "\n\n", sample_tree_->TotalPeriod());
}

void ReportCommand::CollectReportWidth() {
  sample_tree_->VisitAllSamples(
      std::bind(&ReportCommand::CollectReportEntryWidth, this, std::placeholders::_1));
}

void ReportCommand::CollectReportEntryWidth(const SampleEntry& sample) {
  for (auto& item : displayable_items_) {
    item->AdjustWidth(sample);
  }
}

void ReportCommand::PrintReportHeader() {
  for (size_t i = 0; i < displayable_items_.size(); ++i) {
    auto& item = displayable_items_[i];
    if (i != displayable_items_.size() - 1) {
      printf("%-*s  ", static_cast<int>(item->Width()), item->Name().c_str());
    } else {
      printf("%s\n", item->Name().c_str());
    }
  }
}

void ReportCommand::PrintReportEntry(const SampleEntry& sample) {
  for (size_t i = 0; i < displayable_items_.size(); ++i) {
    auto& item = displayable_items_[i];
    if (i != displayable_items_.size() - 1) {
      printf("%-*s  ", static_cast<int>(item->Width()), item->Show(sample).c_str());
    } else {
      printf("%s\n", item->Show(sample).c_str());
    }
  }
  if (print_callgraph_) {
    PrintCallGraph(sample);
  }
}

static void PrintCallGraphEntry(size_t depth, std::string prefix,
                                const std::unique_ptr<CallChainNode>& node, uint64_t parent_period,
                                bool last) {
  if (depth > 20) {
    LOG(WARNING) << "truncated callgraph at depth " << depth;
    return;
  }
  prefix += "|";
  printf("%s\n", prefix.c_str());
  if (last) {
    prefix.back() = ' ';
  }
  std::string percentage_s = "-- ";
  if (node->period + node->children_period != parent_period) {
    double percentage = 100.0 * (node->period + node->children_period) / parent_period;
    percentage_s = android::base::StringPrintf("--%.2lf%%-- ", percentage);
  }
  printf("%s%s%s\n", prefix.c_str(), percentage_s.c_str(), node->chain[0]->symbol->name.c_str());
  prefix.append(percentage_s.size(), ' ');
  for (size_t i = 1; i < node->chain.size(); ++i) {
    printf("%s%s\n", prefix.c_str(), node->chain[i]->symbol->name.c_str());
  }

  for (size_t i = 0; i < node->children.size(); ++i) {
    PrintCallGraphEntry(depth + 1, prefix, node->children[i], node->children_period,
                        (i + 1 == node->children.size()));
  }
}

void ReportCommand::PrintCallGraph(const SampleEntry& sample) {
  std::string prefix = "       ";
  printf("%s|\n", prefix.c_str());
  printf("%s-- %s\n", prefix.c_str(), sample.symbol->name.c_str());
  prefix.append(3, ' ');
  for (size_t i = 0; i < sample.callchain.children.size(); ++i) {
    PrintCallGraphEntry(1, prefix, sample.callchain.children[i], sample.callchain.children_period,
                        (i + 1 == sample.callchain.children.size()));
  }
}

__attribute__((constructor)) static void RegisterReportCommand() {
  RegisterCommand("report", [] { return std::unique_ptr<Command>(new ReportCommand()); });
}
