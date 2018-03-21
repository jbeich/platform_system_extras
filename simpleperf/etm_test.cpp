#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <memory>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "event_fd.h"
#include "perf_event.h"
#include "record.h"
#include "utils.h"
#include "workload.h"

static void Init(uint64_t* cs_etm_type) {
  // Enable ETR TMC to support self-hosted trace.
  if (!android::base::WriteStringToFile("1",
                                        "/sys/bus/coresight/devices/ec033000.etr/enable_sink")) {
    PLOG(FATAL) << "Failed to enable ETR TMC";
  }

  // Find perf event type for ETM.
  std::string cs_etm_type_str;
  if (!android::base::ReadFileToString("/sys/bus/event_source/devices/cs_etm/type",
                                       &cs_etm_type_str) ||
      !android::base::ParseUint(android::base::Trim(cs_etm_type_str), cs_etm_type)) {
    PLOG(FATAL) << "Failed to find perf event type for ETM";
  }
  LOG(INFO) << "cs_etm_type: " << *cs_etm_type;
}

static std::unique_ptr<Workload> CreateWorkload(int cpu) {
  auto thread_function = [&]() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    int ret = sched_setaffinity(gettid(), sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
      LOG(FATAL) << "Failed to put thread on cpu 0";
    }
    LOG(INFO) << "Workload process started";
    while (true) {
      usleep(10);
      volatile int i;
      for (i = 0; i < 10000000; ++i) {
      }
    }
  };
  std::unique_ptr<Workload> workload = Workload::CreateWorkload(thread_function);
  if (!workload || !workload->Start()) {
    LOG(FATAL) << "Failed to create workload";
  }
  return workload;
}

static std::unique_ptr<EventFd> OpenETMEventFile(uint64_t cs_etm_type, pid_t pid, int cpu) {
  perf_event_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.size = sizeof(perf_event_attr);
  attr.type = cs_etm_type;
  attr.mmap = 1;
  attr.comm = 1;
  attr.disabled = 1;
  attr.read_format =
      PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;
  attr.sample_type |= PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_PERIOD |
      PERF_SAMPLE_CPU | PERF_SAMPLE_ID;
  attr.freq = 1;
  attr.sample_freq = 1;
  std::unique_ptr<EventFd> event_fd = EventFd::OpenEventFile(attr, pid, cpu, nullptr);
  if (!event_fd) {
    LOG(FATAL) << "Failed to open perf_event_file for ETM event type";
  }
  return event_fd;
}

static void CreateMappedBuffers(EventFd& event_fd, size_t primary_buffer_pages, size_t aux_buffer_pages,
                                void** aux_buffer_addr) {
  CHECK(IsPowerOfTwo(primary_buffer_pages));
  CHECK(IsPowerOfTwo(aux_buffer_pages));
  if (!event_fd.CreateMappedBuffer(primary_buffer_pages, true)) {
    LOG(FATAL) << "Failed to create primary buffer: " << primary_buffer_pages;
  }
  perf_event_mmap_page* metadata_page = event_fd.GetMetaDataPage();
  metadata_page->aux_offset = (primary_buffer_pages + 1) * PAGE_SIZE;
  metadata_page->aux_size = aux_buffer_pages * PAGE_SIZE;
  void* addr = mmap(nullptr, metadata_page->aux_size, PROT_READ | PROT_WRITE, MAP_SHARED, event_fd.Fd(),
                    metadata_page->aux_offset);
  if (addr == MAP_FAILED) {
    PLOG(FATAL) << "Failed to create aux buffer: " << aux_buffer_pages;
  }
  *aux_buffer_addr = addr;
}


static void StartRecording(EventFd& event_fd) {
  int ret = ioctl(event_fd.Fd(), PERF_EVENT_IOC_ENABLE, 0);
  if (ret != 0) {
    PLOG(FATAL) << "Failed to enable event file";
  }
}

static void StopRecording(EventFd& event_fd) {
  int ret = ioctl(event_fd.Fd(), PERF_EVENT_IOC_DISABLE, 0);
  if (ret != 0) {
    PLOG(FATAL) << "Failed to disable event file";
  }
}

static void ShowRecords(EventFd& event_fd) {
  std::vector<char> buffer;
  size_t buffer_pos = 0;
  size_t read_size = event_fd.GetAvailableMmapData(buffer, buffer_pos);
  LOG(INFO) << "Read record size " << read_size;
  if (read_size > 0u) {
    std::vector<std::unique_ptr<Record>> records = ReadRecordsFromBuffer(event_fd.attr(),
                                                                         buffer.data(), read_size);
    for (auto& record : records) {
      record->Dump();
    }
  }
}

/*
static void ShowAuxData(char* addr, size_t size) {
  for (size_t i = 0; i < size; i += 16) {
    for (size_t j = i; j < size && j < i + 16; ++j) {
      printf("%02x ", static_cast<uint8_t>(addr[j]));
    }
    printf("\n");
  }
}
*/

int main(int, char** argv) {
  android::base::InitLogging(argv, android::base::StderrLogger);
  android::base::ScopedLogSeverity severity(android::base::VERBOSE);

  uint64_t cs_etm_type;
  Init(&cs_etm_type);
  const int kCpu = 0;
  std::unique_ptr<Workload> workload = CreateWorkload(kCpu);
  std::unique_ptr<EventFd> event_fd = OpenETMEventFile(cs_etm_type, workload->GetPid(), kCpu);

  void* aux_buffer_addr = nullptr;
  CreateMappedBuffers(*event_fd, 128, 128, &aux_buffer_addr);

  LOG(INFO) << "Start recording";
  StartRecording(*event_fd);
  LOG(INFO) << "Wait 10 seconds for ETM data...";
  sleep(10);
  LOG(INFO) << "Disable perf event file to get ETM data";
  StopRecording(*event_fd);
  LOG(INFO) << "Show ETM data";
  sleep(1);
  ShowRecords(*event_fd);
  //ShowAuxData(static_cast<char*>(addr), 1024);
  return 0;
}
