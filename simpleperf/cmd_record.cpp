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

#include <poll.h>
#include <signal.h>
#include <string>
#include <vector>

#include <base/logging.h>

#include "command.h"
#include "environment.h"
#include "event_attr.h"
#include "event_fd.h"
#include "event_type.h"
#include "read_elf.h"
#include "record_file.h"
#include "workload.h"

static std::string default_measured_event_type = "cpu-cycles";

void DummySigChildHandler(int) {
}

class RecordCommandImpl {
 public:
  RecordCommandImpl()
      : use_sample_freq_(true),
        sample_freq_(1000),
        record_filename_("perf.data"),
        system_wide_collection_(false),
        measured_event_type_(nullptr),
        perf_mmap_pages_(256) {
    // We need signal SIGCHLD to break poll().
    saved_sigchild_handler_ = signal(SIGCHLD, DummySigChildHandler);
  }

  ~RecordCommandImpl() {
    signal(SIGCHLD, saved_sigchild_handler_);
  }

  bool Run(const std::vector<std::string>& args);

 private:
  bool ParseOptions(const std::vector<std::string>& args, std::vector<std::string>* non_option_args);
  bool SetMeasuredEventType(const std::string& event_type_name);
  void CreateEventAttr();
  bool OpenEventFilesForCpus(const std::vector<int>& cpus);
  bool OpenEventFilesForProcess(pid_t pid);
  bool MmapEventFiles();
  void PreparePollForEventFiles();
  bool DumpKernelMmaps();
  bool DumpThreadCommAndMmaps();

  bool StartRecording();
  bool DumpMmapAreaInEventFiles();
  bool DumpMmapAreaInEventFile(const std::unique_ptr<EventFd>& event_fd, bool* have_data);
  bool DumpAdditionalFeatures();

  bool use_sample_freq_;    // Use sample_freq_ when true, otherwise using sample_period_.
  uint64_t sample_freq_;    // Sample 'sample_freq_' times per second.
  uint64_t sample_period_;  // Sample once when 'sample_period_' events occur.

  std::string record_filename_;
  bool system_wide_collection_;
  const EventType* measured_event_type_;
  // mmap pages used by each perf_event_file, should be power of 2.
  const size_t perf_mmap_pages_;
  sighandler_t saved_sigchild_handler_;

  EventAttr event_attr_;
  std::vector<std::unique_ptr<EventFd>> event_fds_;
  std::vector<pollfd> pollfds_;
  std::unique_ptr<RecordFileWriter> record_file_writer_;
};

bool RecordCommandImpl::Run(const std::vector<std::string>& args) {
  // 1. Parse options, and use default measured event type if not given.
  std::vector<std::string> workload_args;
  if (!ParseOptions(args, &workload_args)) {
    return false;
  }
  if (measured_event_type_ == nullptr) {
    if (!SetMeasuredEventType(default_measured_event_type)) {
      return false;
    }
  }
  CreateEventAttr();

  // 2. Create workload.
  if (workload_args.empty()) {
    workload_args = std::vector<std::string>({"sleep", "99999"});
  }
  std::unique_ptr<Workload> workload = Workload::CreateWorkload(workload_args);
  if (workload == nullptr) {
    return false;
  }

  // 3. Open perf_event_files, create memory mapped buffers for perf_event_files, add prepare poll
  //    for perf_event_files.
  if (system_wide_collection_) {
    std::vector<int> cpus = GetOnlineCpus();
    if (cpus.empty() || !OpenEventFilesForCpus(cpus)) {
      return false;
    }
  } else {
    if (!OpenEventFilesForProcess(workload->GetWorkPid())) {
      return false;
    }
  }
  if (!MmapEventFiles()) {
    return false;
  }
  PreparePollForEventFiles();

  // 4. Open record file writer, dump kernel mmap information, dump thread mmap information.
  record_file_writer_ = RecordFileWriter::CreateInstance(record_filename_, event_attr_, event_fds_);
  if (record_file_writer_ == nullptr) {
    return false;
  }
  if (!DumpKernelMmaps()) {
    return false;
  }
  if (system_wide_collection_) {
    if (!DumpThreadCommAndMmaps()) {
      return false;
    }
  }

  // 5. Dump records in mmap buffers of perf_event_files to output file while workload is running.

  // If monitoring only one process, we use the enable_on_exec flag, and don't need to start
  // recording manually.
  if (system_wide_collection_) {
    if (!StartRecording()) {
      return false;
    }
  }
  if (!workload->Start()) {
    return false;
  }
  while (true) {
    if (!DumpMmapAreaInEventFiles()) {
      return false;
    }
    if (workload->IsFinished()) {
      break;
    }
    poll(&pollfds_[0], pollfds_.size(), -1);
  }

  // 6. Dump additional features to output file.
  if (!DumpAdditionalFeatures()) {
    return false;
  }

  if (!record_file_writer_->Close()) {
    return false;
  }

  return true;
}

static bool NextArgumentOrError(const std::vector<std::string>& args, size_t* pi) {
  if (*pi + 1 == args.size()) {
    LOG(ERROR) << "No argument following " << args[*pi] << " option. Try `simpleperf help record`";
    return false;
  }
  ++*pi;
  return true;
}

bool RecordCommandImpl::ParseOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>* non_option_args) {
  size_t i;
  for (i = 0; i < args.size() && args[i].size() > 0 && args[i][0] == '-'; ++i) {
    if (args[i] == "-a") {
      system_wide_collection_ = true;
    } else if (args[i] == "-c") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      char* endptr;
      sample_period_ = strtoul(args[i].c_str(), &endptr, 0);
      if (*endptr != '\0' || sample_period_ == 0) {
        LOG(ERROR) << "Invalid sample period: '" << args[i] << "'";
        return false;
      }
      use_sample_freq_ = false;
    } else if (args[i] == "-e") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      if (!SetMeasuredEventType(args[i])) {
        return false;
      }
    } else if (args[i] == "-f" || args[i] == "-F") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      char* endptr;
      sample_freq_ = strtoul(args[i].c_str(), &endptr, 0);
      if (*endptr != '\0' || sample_freq_ == 0) {
        LOG(ERROR) << "Invalid sample frequency: '" << args[i] << "'";
        return false;
      }
      use_sample_freq_ = true;
    } else if (args[i] == "-o") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      record_filename_ = args[i];
    } else {
      LOG(ERROR) << "Unknown option for record command: '" << args[i] << "'\n";
      LOG(ERROR) << "Try `simpleperf help record`";
      return false;
    }
  }

  if (non_option_args != nullptr) {
    non_option_args->clear();
    for (; i < args.size(); ++i) {
      non_option_args->push_back(args[i]);
    }
  }
  return true;
}

bool RecordCommandImpl::SetMeasuredEventType(const std::string& event_type_name) {
  const EventType* event_type = EventTypeFactory::FindEventTypeByName(event_type_name);
  if (event_type == nullptr) {
    LOG(ERROR) << "Unknown event_type: " << event_type_name;
    LOG(ERROR) << "Try `simpleperf list` to list all possible event type names";
    return false;
  }
  if (!event_type->IsSupportedByKernel()) {
    LOG(ERROR) << "Event type '" << event_type_name << "' is not supported by the kernel";
    return false;
  }
  measured_event_type_ = event_type;
  return true;
}

void RecordCommandImpl::CreateEventAttr() {
  event_attr_ = EventAttr::CreateDefaultAttrToMonitorEvent(*measured_event_type_);
  if (use_sample_freq_) {
    event_attr_.SetSampleFreq(sample_freq_);
  } else {
    event_attr_.SetSamplePeriod(sample_period_);
  }
  event_attr_.SetSampleAll();
  if (!system_wide_collection_) {
    event_attr_.SetEnableOnExec();
  }
}

bool RecordCommandImpl::OpenEventFilesForCpus(const std::vector<int>& cpus) {
  std::vector<std::unique_ptr<EventFd>> event_fds;
  for (auto& cpu : cpus) {
    auto event_fd = EventFd::OpenEventFileForCpu(event_attr_, cpu);
    if (event_fd != nullptr) {
      event_fds.push_back(std::move(event_fd));
    }
  }
  // As the online cpus can be enabled or disabled at runtime, we may not open perf_event_file
  // for all cpus successfully. But we should open at least one cpu successfully.
  if (event_fds.empty()) {
    LOG(ERROR) << "failed to open perf_event_files for event_type " << measured_event_type_->name
               << " on all cpus";
    return false;
  }
  event_fds_ = std::move(event_fds);
  return true;
}

bool RecordCommandImpl::OpenEventFilesForProcess(pid_t pid) {
  std::vector<std::unique_ptr<EventFd>> event_fds;
  auto event_fd = EventFd::OpenEventFileForProcess(event_attr_, pid);
  if (event_fd == nullptr) {
    PLOG(ERROR) << "failed to open perf_event_file for event_type " << measured_event_type_->name
                << " on pid " << pid;
    return false;
  }
  event_fds.push_back(std::move(event_fd));
  event_fds_ = std::move(event_fds);
  return true;
}

bool RecordCommandImpl::MmapEventFiles() {
  for (auto& event_fd : event_fds_) {
    if (!event_fd->MmapContent(perf_mmap_pages_)) {
      return false;
    }
  }
  return true;
}

void RecordCommandImpl::PreparePollForEventFiles() {
  std::vector<pollfd> pollfds(event_fds_.size());
  for (size_t i = 0; i < pollfds.size(); ++i) {
    event_fds_[i]->PreparePollForMmapData(&pollfds[i]);
  }
  pollfds_ = std::move(pollfds);
}

bool RecordCommandImpl::StartRecording() {
  for (auto& event_fd : event_fds_) {
    if (!event_fd->EnableEvent()) {
      return false;
    }
  }
  return true;
}

bool RecordCommandImpl::DumpKernelMmaps() {
  KernelMmap kernel_mmap;
  std::vector<ModuleMmap> module_mmaps;

  if (!GetKernelMmaps(kernel_mmap, module_mmaps)) {
    return false;
  }

  MmapRecord mmap_record = CreateKernelMmapRecord(kernel_mmap, event_attr_);
  if (!record_file_writer_->WriteData(mmap_record.BinaryFormat())) {
    return false;
  }
  for (auto& module_mmap : module_mmaps) {
    MmapRecord mmap_record = CreateModuleMmapRecord(module_mmap, event_attr_);
    if (!record_file_writer_->WriteData(mmap_record.BinaryFormat())) {
      return false;
    }
  }
  return true;
}

bool RecordCommandImpl::DumpThreadCommAndMmaps() {
  std::vector<ThreadComm> thread_comms;
  if (!GetThreadComms(thread_comms)) {
    return false;
  }
  for (auto& thread : thread_comms) {
    CommRecord record = CreateThreadCommRecord(thread, event_attr_);
    if (!record_file_writer_->WriteData(record.BinaryFormat())) {
      return false;
    }
    if (thread.is_process) {
      std::vector<ThreadMmap> thread_mmaps;
      if (!GetProcessMmaps(thread.tid, thread_mmaps)) {
        // The thread exited before we get its info.
        continue;
      }
      for (auto& thread_mmap : thread_mmaps) {
        if (thread_mmap.executable == 0) {
          continue;
        }
        MmapRecord mmap_record = CreateThreadMmapRecord(thread, thread_mmap, event_attr_);
        if (!record_file_writer_->WriteData(mmap_record.BinaryFormat())) {
          continue;
        }
      }
    }
  }
  return true;
}

bool RecordCommandImpl::DumpMmapAreaInEventFiles() {
  bool have_data = true;
  while (have_data) {
    have_data = false;
    for (auto& event_fd : event_fds_) {
      if (!DumpMmapAreaInEventFile(event_fd, &have_data)) {
        return false;
      }
    }
  }
  return true;
}

bool RecordCommandImpl::DumpMmapAreaInEventFile(const std::unique_ptr<EventFd>& event_fd,
                                                bool* have_data) {
  char* data;
  size_t size;
  while (event_fd->GetAvailableMmapData(&data, &size)) {
    if (!record_file_writer_->WriteData(data, size)) {
      return false;
    }
    *have_data = true;
    event_fd->CommitMmapData(size);
  }
  return true;
}

bool RecordCommandImpl::DumpAdditionalFeatures() {
  std::vector<std::string> hit_kernel_modules;
  std::vector<std::string> hit_user_files;
  if (!record_file_writer_->GetHitModules(hit_kernel_modules, hit_user_files)) {
    return false;
  }
  std::vector<BuildIdRecord> build_id_records;
  BuildId build_id;
  // Add build_ids for kernel/modules.
  for (auto& filename : hit_kernel_modules) {
    if (filename == DEFAULT_KERNEL_MMAP_NAME) {
      if (!GetKernelBuildId(build_id)) {
        return false;
      }
      build_id_records.push_back(CreateBuildIdRecordForFeatureSection(
          -1, build_id, DEFAULT_KERNEL_FILENAME_FOR_BUILD_ID, true));
    } else {
      std::string module_name = filename;
      size_t pos = module_name.rfind("/");
      if (pos != std::string::npos) {
        module_name = module_name.substr(pos);
      }
      pos = module_name.find(".ko");
      if (pos != std::string::npos) {
        module_name = module_name.substr(0, pos);
      }
      if (!GetModuleBuildId(module_name.c_str(), build_id)) {
        LOG(DEBUG) << "Can't read build_id for module '" << module_name << "'";
        continue;
      }
      build_id_records.push_back(CreateBuildIdRecordForFeatureSection(-1, build_id, filename, true));
    }
  }
  // Add build_ids for user elf files.
  for (auto& user_file : hit_user_files) {
    if (user_file == DEFAULT_EXEC_NAME_FOR_THREAD_MMAP) {
      continue;
    }
    if (!GetBuildIdFromElfFile(user_file.c_str(), build_id)) {
      LOG(DEBUG) << "Can't read build_id for file '" << user_file << "'";
      continue;
    }
    build_id_records.push_back(CreateBuildIdRecordForFeatureSection(-1, build_id, user_file, false));
  }
  if (!record_file_writer_->WriteFeatureHeader(1)) {
    return false;
  }
  if (!record_file_writer_->WriteBuildIdFeature(build_id_records)) {
    return false;
  }
  return true;
}

class RecordCommand : public Command {
 public:
  RecordCommand()
      : Command("record", "record sampling info in perf.data",
                "Usage: simpleperf record [options] [command [command-args]]\n"
                "    Gather sampling information when running [command]. If [command]\n"
                "    is not specified, sleep 99999 is used instead.\n"
                "    -a           System-wide collection.\n"
                "    -c count     Set event sample period.\n"
                "    -e event     Select the event to sample (Use `simpleperf list`)\n"
                "                 to find all possible event names.\n"
                "    -f freq      Set event sample frequency.\n"
                "    -F freq      Same as '-f freq'.\n"
                "    -o record_file_name    Set record file name, default is perf.data.\n") {
  }

  bool Run(const std::vector<std::string>& args) override {
    RecordCommandImpl impl;
    return impl.Run(args);
  }
};

RecordCommand record_command;
