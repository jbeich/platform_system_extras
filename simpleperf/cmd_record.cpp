/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include "build_id.h"
#include "command.h"
#include "environment.h"
#include "event.h"
#include "event_attr.h"
#include "event_fd.h"
#include "read_elf.h"
#include "record.h"
#include "record_file.h"
#include "trace.h"
#include "util.h"
#include "workload.h"

class EventFileMmapArea {
 public:
  void* base;
  size_t len;
  uint64_t read_head;  // position to start reading mmap record data.

 public:
  EventFileMmapArea() : base(nullptr), len(0), read_head(0) { }

  ~EventFileMmapArea() {
    if (base != nullptr) {
      munmap(base, len);
    }
  }

  EventFileMmapArea(const EventFileMmapArea&) = delete;
  EventFileMmapArea& operator=(const EventFileMmapArea&) = delete;
};

class RecordCommand : public Command {
 public:
  RecordCommand()
    	: Command("record",
      	        "record sampling info in perf.data",
        	      "Usage: simpleperf record [options] [command [command-args]]\n"
          	    "    Gather sampling information when running [command]. If [command]\n"
            	  "is not specified, sleep 1 is used instead.\n"
           		  "    -a                    System-wide collection."
           	    "    -c count              Set event peroid to sample.\n"
                "    -e event              Select the event the sample (Use `simpleperf list`\n"
                "                          to find possible event names.\n"
                "    -f freq               Set event frequency to sample.\n"
                "    -F freq               Same as -f freq.\n"
                "    -o output_file_name   Set output record file name.\n"
                "    --help                Print this help info.\n"
              ) {
    option_help = false;
    option_mmap_pages = 256;
    option_sample_freq = 4000;
    use_freq = true;
    option_output_file = "perf.data";
    option_system_wide = false;

    measured_event = nullptr;
    page_size = sysconf(_SC_PAGE_SIZE);
  }

  bool RunCommand(std::vector<std::string>& args) override;

 private:
  bool ParseOptions(const std::vector<std::string>& args, std::vector<std::string>& non_option_args);
  void SetDefaultMeasuredEvent();
  bool OpenEventFilesForProcess(pid_t pid);
  bool OpenEventFilesForCpus(const std::vector<int>& cpu_list);
  bool MmapEventFiles();
  bool PreparePollForEventFiles();
  bool StartSampling();
  bool StopSampling();
  bool ReadMmapAreas();
  bool ReadSingleMmapArea(std::unique_ptr<EventFileMmapArea>& area);
  bool OpenOutput();
  bool WriteOutput(const char* buf, size_t len);
  bool CloseOutput();
  bool DumpKernelMmapInfo();
  bool DumpThreadInfo();
  bool DumpAdditionalFeatures();

 private:
  const Event* measured_event;
  std::unique_ptr<EventAttr> event_attr;
  std::vector<std::unique_ptr<EventFd>> event_fds;

  std::vector<std::unique_ptr<EventFileMmapArea>> mmap_areas;
  std::vector<pollfd> pollfds;

  bool option_help;
  int option_mmap_pages;  // should be 2^n, excluding the first page.
  uint64_t option_sample_freq;
  uint64_t option_sample_period;
  bool use_freq;
  std::string option_output_file;
  bool option_system_wide;

  size_t page_size;
  std::unique_ptr<RecordFile> record_file;
};

bool RecordCommand::RunCommand(std::vector<std::string>& args) {
  TRACE("record command start running\n");
  std::vector<std::string> non_option_args;
  if (!ParseOptions(args, non_option_args)) {
    fprintf(stderr, "RecordCommand::ParseOptions() failed\n");
    fprintf(stderr, "%s\n", DetailedHelpInfo());
    return false;
  }
  if (option_help) {
    printf("%s\n", DetailedHelpInfo());
    return true;
  }

  if (measured_event == nullptr) {
    SetDefaultMeasuredEvent();
  }

  TRACE("CreatWorkLoad\n");

  std::unique_ptr<WorkLoad> work_load;
  if (non_option_args.size() == 0) {
    std::vector<std::string> default_args{"sleep", "1"};
    work_load = WorkLoad::CreateWorkLoadInNewProcess(default_args);
  } else {
    work_load = WorkLoad::CreateWorkLoadInNewProcess(non_option_args);
  }

  TRACE("OpenEventFiles\n");

  if (option_system_wide) {
    if (!OpenEventFilesForCpus(GetOnlineCpus())) {
      return false;
    }
  } else {
    if (!OpenEventFilesForProcess(work_load->GetWorkProcess())) {
      return false;
    }
  }

  TRACE("MmapEventFiles\n");

  if (!MmapEventFiles()) {
    return false;
  }

  TRACE("PreparePollForEventFiles\n");

  if (!PreparePollForEventFiles()) {
    fprintf(stderr, "RecordCommand::PreparePollForEventFiles() failed: %s\n", strerror(errno));
    return false;
  }

  TRACE("OpenOutput\n");

  if (!OpenOutput()) {
    return false;
  }

  TRACE("DumpKernelMmapInfo\n");

  if (!DumpKernelMmapInfo()) {
    return false;
  }

  TRACE("DumpThreadInfo\n");

  if (option_system_wide && !DumpThreadInfo()) {
    return false;
  }

  // Sampling has enable_on_exec flag. If work_load doesn't call exec(), we need to start sampling manually.
  if (!work_load->UseExec()) {
    if (!StartSampling()) {
      fprintf(stderr, "RecordCommand::StartSampling() failed: %s\n", strerror(errno));
      return false;
    }
  }

  TRACE("work_load Start\n");

  if (!work_load->Start()) {
    fprintf(stderr, "RecordCommand start workload failed\n");
    return false;
  }

  while (true) {
    if (!ReadMmapAreas()) {
      return false;
    }

    if (work_load->Finished()) {
      break;
    }

    poll(&pollfds[0], pollfds.size(), 1000);  // Timeout is necessary to detect finish.
  }

  TRACE("StopSampling\n");

  if (!StopSampling()) {
    return false;
  }

  if (!DumpAdditionalFeatures()) {
    return false;
  }

  if (!CloseOutput()) {
    return false;
  }

  TRACE("record command run successfully!\n");

  return true;
}

bool RecordCommand::ParseOptions(const std::vector<std::string>& args, std::vector<std::string>& non_option_args) {
  size_t i;
  for (i = 0; i < args.size() && args[i][0] == '-'; ++i) {
    if (args[i] == "--help") {
      option_help = true;
    } else if (args[i] == "-a") {
      option_system_wide = true;
    } else if (args[i] == "-e") {
      if (i + 1 == args.size()) {
        return false;
      }
      const Event* event = Event::FindEventByName(args[i + 1].c_str());
      if (event == nullptr || !event->Supported()) {
        fprintf(stderr, "event \"%s\" is not supported\n", args[i + 1].c_str());
        return false;
      }
      measured_event = event;
      ++i;
    } else if (args[i] == "-c") {
      if (i + 1 == args.size()) {
        return false;
      }
      char* endptr;
      option_sample_period = strtoul(args[i + 1].c_str(), &endptr, 0);
      if (*endptr != '\0' || option_sample_period == 0) {
        fprintf(stderr, "invalid sample period: \"%s\"\n", args[i + 1].c_str());
        return false;
      }
      use_freq = false;
      ++i;
    } else if (args[i] == "-f" || args[i] == "-F") {
      if (i + 1 == args.size()) {
        return false;
      }
      char* endptr;
      option_sample_freq = strtoul(args[i + 1].c_str(), &endptr, 0);
      if (*endptr != '\0' || option_sample_freq == 0) {
        fprintf(stderr, "invalid sample freq: \"%s\"\n", args[i + 1].c_str());
        return false;
      }
      use_freq = true;
      ++i;
    } else if (args[i] == "-o") {
      if (i + 1 == args.size()) {
        return false;
      }
      option_output_file = args[i + 1];
      ++i;
    }
  }

  non_option_args.clear();
  for (; i < args.size(); ++i) {
    non_option_args.push_back(args[i]);
  }
  return true;
}

void RecordCommand::SetDefaultMeasuredEvent() {
  const char* default_measured_event_name = "cpu-cycles";
  const Event* event = Event::FindEventByName(default_measured_event_name);
  if (event->Supported()) {
    measured_event = event;
  }
}

bool RecordCommand::OpenEventFilesForProcess(pid_t pid) {
  event_attr = std::unique_ptr<EventAttr>(new EventAttr(measured_event, false));
  event_attr->EnableOnExec();
  if (use_freq) {
    event_attr->SetSampleFreq(option_sample_freq);
  } else {
    event_attr->SetSamplePeriod(option_sample_period);
  }
  event_attr->SetSampleAll();
  auto event_fd = EventFd::OpenEventFileForProcess(*event_attr, pid);
  if (event_fd == nullptr) {
    return false;
  }
  event_fds.clear();
  event_fds.push_back(std::move(event_fd));
  return true;
}

bool RecordCommand::OpenEventFilesForCpus(const std::vector<int>& cpu_list) {
  event_attr = std::unique_ptr<EventAttr>(new EventAttr(measured_event, true));
  event_attr->EnableOnExec();
  if (use_freq) {
    event_attr->SetSampleFreq(option_sample_freq);
  } else {
    event_attr->SetSamplePeriod(option_sample_period);
  }
  event_attr->SetSampleAll();
  event_fds.clear();
  for (auto cpu : cpu_list) {
    auto event_fd = EventFd::OpenEventFileForCpu(*event_attr, cpu);
    if (event_fd == nullptr) {
      event_fds.clear();
      return false;
    }
    event_fds.push_back(std::move(event_fd));
  }
  return true;
}

bool RecordCommand::MmapEventFiles() {
  if ((option_mmap_pages & (option_mmap_pages - 1)) != 0) {
    fprintf(stderr, "invalid option_mmap_pages: %d\n", option_mmap_pages);
    return false;
  }
  size_t mmap_len = (option_mmap_pages + 1) * page_size;
  mmap_areas.clear();
  for (auto& event_fd : event_fds) {
    void* p = mmap(NULL, mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED, event_fd->Fd(), 0);
    if (p == MAP_FAILED) {
      fprintf(stderr, "RecordCommand::MmapEventFile() mmap failed: %s\n", strerror(errno));
      return false;
    }
    std::unique_ptr<EventFileMmapArea> area(new EventFileMmapArea());
    area->base = p;
    area->len = mmap_len;
    mmap_areas.push_back(std::move(area));
  }
  return true;
}

bool RecordCommand::PreparePollForEventFiles() {
  pollfds.clear();
  for (auto& event_fd : event_fds) {
    if (fcntl(event_fd->Fd(), F_SETFL, O_NONBLOCK) != 0) {
      return false;
    }
    pollfd poll_fd;
    memset(&poll_fd, 0, sizeof(poll_fd));
    poll_fd.fd = event_fd->Fd();
    poll_fd.events = POLLIN;
    pollfds.push_back(poll_fd);
  }
  return true;
}

bool RecordCommand::StartSampling() {
  for (auto& event_fd : event_fds) {
    if (!event_fd->EnableEvent()) {
      return false;
    }
  }
  return true;
}

bool RecordCommand::StopSampling() {
  for (auto& event_fd : event_fds) {
    if (!event_fd->DisableEvent()) {
      return false;
    }
  }
  return true;
}

bool RecordCommand::ReadMmapAreas() {
  for (auto& area : mmap_areas) {
    if (!ReadSingleMmapArea(area)) {
      return false;
    }
  }
  return true;
}

bool RecordCommand::ReadSingleMmapArea(std::unique_ptr<EventFileMmapArea>& area) {
  perf_event_mmap_page* metadata_page = reinterpret_cast<perf_event_mmap_page*>(area->base);
  char* buf = reinterpret_cast<char*>(area->base) + page_size;
  uint64_t buf_len = area->len - page_size;
  uint64_t buf_mask = buf_len - 1;

  // The mmap pages are used as a ring buffer. The kernel continuously writes records to the buffer,
  // and the user continuously read records out.
  //         ___________________________________________
  // buffer | can write   |  can read   |   can write   |
  //                      ^             ^
  //                  read_head      new_head
  //
  // So the user can read records in [read_head, new_head), and the kernel can write records in
  // [new_head, read_head). The kernel is responsible for updating new_head, and the user is
  // responsible for updating read_head.
  uint64_t read_head = area->read_head;
  uint64_t new_head = metadata_page->data_head;

  std::atomic_thread_fence(std::memory_order_acquire);
  if (read_head == new_head) {
    // No data available.
    return true;
  }

  if ((new_head & buf_mask) < (read_head & buf_mask)) {
    // Wrap over the end of the buffer.
    if (!WriteOutput(&buf[read_head & buf_mask], buf_len - (read_head & buf_mask))) {
      return false;
    }
    read_head = 0;
  }
  if (!WriteOutput(&buf[read_head & buf_mask], (new_head & buf_mask) - (read_head & buf_mask))) {
    return false;
  }

  std::atomic_thread_fence(std::memory_order_release);
  metadata_page->data_tail = read_head;

  area->read_head = new_head;
  return true;
}

bool RecordCommand::OpenOutput() {
  record_file = RecordFile::CreateFile(option_output_file);
  if (record_file == nullptr) {
    return false;
  }

  if (!record_file->WriteHeader(*event_attr)) {
    return false;
  }

  return true;
}

bool RecordCommand::WriteOutput(const char* buf, size_t len) {
  return record_file->WriteData(buf, len);
}

bool RecordCommand::CloseOutput() {
  // WriteHeader again to update data size.
  if (!record_file->WriteHeader(*event_attr)) {
    return false;
  }

  return record_file->Close();
}

bool RecordCommand::DumpKernelMmapInfo() {
  KernelMmap kernel_mmap;
  std::vector<ModuleMmap> module_mmaps;

  if (!GetMmapInfo(kernel_mmap, module_mmaps)) {
    return false;
  }

  TRACE("CreateKernelMmapRecord\n");

  auto mmap_record = CreateKernelMmapRecord(kernel_mmap, *event_attr);
  if (mmap_record == nullptr) {
    return false;
  }
  if (!WriteOutput(mmap_record->GetBuf(), mmap_record->GetBufSize())) {
    return false;
  }

  TRACE("CreateModuleMmapRecord\n");

  for (auto& module_mmap : module_mmaps) {
    auto mmap_record = CreateModuleMmapRecord(module_mmap, *event_attr);
    if (mmap_record == nullptr) {
      return false;
    }
    if (!WriteOutput(mmap_record->GetBuf(), mmap_record->GetBufSize())) {
      return false;
    }
  }

  return true;
}

bool RecordCommand::DumpThreadInfo() {
  std::vector<ThreadComm> thread_comms;
  if (!GetThreadComms(thread_comms)) {
    return false;
  }
  for (auto& thread : thread_comms) {
    auto comm_record = CreateThreadCommRecord(thread, *event_attr);
    if (comm_record == nullptr) {
      return false;
    }
    if (!WriteOutput(comm_record->GetBuf(), comm_record->GetBufSize())) {
      fprintf(stderr, "Write thread comm_record failed\n");
      comm_record->Print();
      return false;
    }

    if (thread.is_process) {
      std::vector<ThreadMmap> thread_mmaps;
      if (!GetProcessMmaps(thread.tid, thread_mmaps)) {
        continue;
      }

      for (auto& thread_mmap : thread_mmaps) {
        if (thread_mmap.executable == 0) {
          continue;
        }
        auto mmap_record = CreateThreadMmapRecord(thread.tgid, thread.tid, thread_mmap, *event_attr);
        if (mmap_record == nullptr) {
          return false;
        }
        if (!WriteOutput(mmap_record->GetBuf(), mmap_record->GetBufSize())) {
          return false;
        }
      }
    }
  }
  return true;
}

bool RecordCommand::DumpAdditionalFeatures() {
  std::vector<std::string> hit_kernel_modules;
  std::vector<std::string> hit_user_files;
  if (!record_file->ReadHitFiles(hit_kernel_modules, hit_user_files)) {
    return false;
  }

  std::vector<std::unique_ptr<Record>> build_id_records;
  BuildId build_id;

  for (auto& filename : hit_kernel_modules) {
    if (filename == DEFAULT_KERNEL_MMAP_NAME) {
      if (!GetKernelBuildId(build_id)) {
        return false;
      }
      auto record = CreateBuildIdRecord(-1, build_id, DEFAULT_KERNEL_FILENAME_FOR_BUILD_ID, true);
      if (record == nullptr) {
        return false;
      }
      build_id_records.push_back(std::move(record));
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
        continue;
      }
      auto record = CreateBuildIdRecord(-1, build_id, filename.c_str(), true);
      if (record == nullptr) {
        return false;
      }
      build_id_records.push_back(std::move(record));
    }
  }

  for (auto& user_file : hit_user_files) {
    if (user_file == DEFAULT_EXEC_NAME_FOR_THREAD_MMAP) {
      continue;
    }
    if (!GetBuildIdFromElfFile(user_file.c_str(), build_id)) {
      fprintf(stderr, "can't read build id for file \"%s\"\n", user_file.c_str());
      continue;
    }
    auto record = CreateBuildIdRecord(-1, build_id, user_file.c_str(), false);
    if (record == nullptr) {
      return false;
    }
    build_id_records.push_back(std::move(record));
  }

  if (!record_file->WriteFeatureHeader(1)) {
    return false;
  }

  if (!record_file->WriteBuildIdFeature(build_id_records)) {
    return false;
  }
  return true;
}

RecordCommand record_cmd;

namespace simpleperf {

bool record(const char* record_cmd_string) {
  if (record_cmd_string == nullptr) {
    return false;
  }
  auto args = SplitString(record_cmd_string);
  return record_cmd.RunCommand(args);
}

}  // namespace simpleperf
