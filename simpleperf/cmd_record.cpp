#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include "command.h"
#include "event.h"
#include "event_attr.h"
#include "event_fd.h"
#include "record_file.h"
#include "workload.h"

class EventFileMmapArea {
 public:
  void* base;
  size_t len;
  uint64_t prev_head;

 public:
  EventFileMmapArea()
    : base(nullptr), len(0), prev_head(0) { }

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
              "    --help                Print this help info.\n"
              ) {
    option_help = false;
    option_mmap_pages = 256;
    option_sample_freq = 4000;
    option_output_file = "perf.data";

    measured_event = nullptr;
    page_size = sysconf(_SC_PAGE_SIZE);
  }

  bool RunCommand(std::vector<std::string>& args) override;

 private:
  bool ParseOptions(const std::vector<std::string>& args, std::vector<std::string>& non_option_args);
  void SetDefaultMeasuredEvent();
  bool OpenEventFilesForProcess(pid_t pid);
  bool MmapEventFiles();
  bool PreparePollForEventFiles();
  bool StartSampling();
  bool ReadMmapAreas();
  bool ReadSingleMmapArea(std::unique_ptr<EventFileMmapArea>& area);
  bool OpenOutput();
  bool WriteOutput(const char* buf, size_t len);
  bool CloseOutput();

 private:
  const Event* measured_event;
  std::vector<std::unique_ptr<EventFd>> event_fds;

  std::vector<std::unique_ptr<EventFileMmapArea>> mmap_areas;
  std::vector<pollfd> pollfds;

  bool option_help;
  int option_mmap_pages;  // should be 2^n, excluding the first page.
  uint64_t option_sample_freq;
  std::string option_output_file;

  size_t page_size;
  std::unique_ptr<RecordFile> record_file;
};

bool RecordCommand::RunCommand(std::vector<std::string>& args) {
  printf("record command run\n");

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

  std::unique_ptr<WorkLoad> work_load;
  if (non_option_args.size() == 0) {
    std::vector<std::string> default_args{"sleep", "1"};
    work_load = WorkLoad::CreateWorkLoadInNewProcess(default_args);
  } else {
    work_load = WorkLoad::CreateWorkLoadInNewProcess(non_option_args);
  }


  if (!OpenEventFilesForProcess(work_load->GetWorkProcess())) {
    fprintf(stderr, "RecordCommand::OpenEventFileForProcess() failed: %s\n", strerror(errno));
    return false;
  }

  if (!MmapEventFiles()) {
    return false;
  }

  if (!PreparePollForEventFiles()) {
    fprintf(stderr, "RecordCommand::PreparePollForEventFiles() failed: %s\n", strerror(errno));
    return false;
  }

  // Sampling has enable_on_exec flag. If work_load doesn't call exec(), we need to start sampling manually.
  if (!work_load->UseExec()) {
    if (!StartSampling()) {
      fprintf(stderr, "RecordCommand::StartSampling() failed: %s\n", strerror(errno));
      return false;
    }
  }

  if (!OpenOutput()) {
    return false;
  }

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
  }

  if (!CloseOutput()) {
    return false;
  }

  return true;
}

bool RecordCommand::ParseOptions(const std::vector<std::string>& args, std::vector<std::string>& non_option_args) {
  size_t i;
  for (i = 0; i < args.size() && args[i][0] == '-'; ++i) {
    if (args[i] == "--help") {
      option_help = true;
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
  EventAttr attr(measured_event, false);
  attr.EnableOnExec();
  attr.SetSampleFreq(option_sample_freq);
  attr.SampleAll();
  auto event_fd = EventFd::OpenEventFileForProcess(attr, pid);
  if (event_fd == nullptr) {
    return false;
  }
  event_fds.clear();
  event_fds.push_back(std::move(event_fd));
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
    area->prev_head = 0;
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

  uint64_t prev_head = area->prev_head;
  uint64_t head = metadata_page->data_head;
  std::atomic_thread_fence(std::memory_order_acquire);
  if (head == prev_head) {
    // No data available.
    return true;
  }

  if ((head & buf_mask) < (prev_head & buf_mask)) {
    // Wrap over the end of the buffer.
    if (!WriteOutput(&buf[prev_head & buf_mask], buf_len - (prev_head & buf_mask))) {
      return false;
    }
    prev_head = 0;
  }
  if (!WriteOutput(&buf[prev_head & buf_mask], (head & buf_mask) - (prev_head & buf_mask))) {
    return false;
  }

  std::atomic_thread_fence(std::memory_order_release);
  metadata_page->data_tail = head;

  area->prev_head = head;
  return true;
}

bool RecordCommand::OpenOutput() {
  record_file = RecordFile::CreateFile(option_output_file);
  if (record_file == nullptr) {
    return false;
  }

  if (!record_file->WriteHeader(event_fds)) {
    return false;
  }

  return true;
}

bool RecordCommand::WriteOutput(const char* buf, size_t len) {
  return record_file->WriteData(buf, len);
}

bool RecordCommand::CloseOutput() {

  // WriteHeader again to update data size.
  if (!record_file->WriteHeader(event_fds)) {
    return false;
  }

  return record_file->Close();
}

RecordCommand record_cmd;
