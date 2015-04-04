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

#include "record.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "environment.h"
#include "event_attr.h"
#include "util.h"

static const char* GetHeaderName(const perf_event_header* header) {
  switch (header->type) {
    case PERF_RECORD_MMAP: return "mmap";
    case PERF_RECORD_LOST: return "lost";
    case PERF_RECORD_COMM: return "comm";
    case PERF_RECORD_EXIT: return "exit";
    case PERF_RECORD_THROTTLE: return "throttle";
    case PERF_RECORD_UNTHROTTLE: return "unthrottle";
    case PERF_RECORD_FORK: return "fork";
    case PERF_RECORD_READ: return "read";
    case PERF_RECORD_SAMPLE: return "sample";
    case PERF_RECORD_BUILD_ID: return "build_id";
    default: return nullptr;
  }
}

static void PrintRecordHeader(const perf_event_header* header) {
  printf("record %s: type %u, misc %u, size %u\n", GetHeaderName(header), header->type,
                                                   header->misc, header->size);
}

std::unique_ptr<Record> BuildRecordOnBuffer(char* buf, size_t size, const EventAttr* attr) {
  perf_event_header* header = reinterpret_cast<perf_event_header*>(buf);
  if (header->type == PERF_RECORD_MMAP) {
    return std::unique_ptr<Record>(new RecordMmap(buf, size, *attr));
  } else if (header->type == PERF_RECORD_COMM) {
    return std::unique_ptr<Record>(new RecordComm(buf, size, *attr));
  } else if (header->type == PERF_RECORD_EXIT) {
    return std::unique_ptr<Record>(new RecordExit(buf, size, *attr));
  } else if (header->type == PERF_RECORD_SAMPLE) {
    return std::unique_ptr<Record>(new RecordSample(buf, size, *attr));
  } else if (header->type == PERF_RECORD_BUILD_ID) {
    return std::unique_ptr<Record>(new RecordBuildId(buf, size));
  }

  return std::unique_ptr<Record>(new Record(buf, size));
}

std::unique_ptr<Record> CreateKernelMmapRecord(const KernelMmap& kernel_mmap,
                                               const EventAttr& attr) {
  return RecordMmap::MakeRecord(attr, UINT_MAX, 0, kernel_mmap.start_addr, kernel_mmap.len,
                                kernel_mmap.pgoff, kernel_mmap.name.c_str(), true, false);
}

std::unique_ptr<Record> CreateModuleMmapRecord(const ModuleMmap& module_mmap,
                                               const EventAttr& attr) {
  std::string filename = module_mmap.filepath;
  if (filename.size() == 0) {
    filename = "[" + module_mmap.name + "]";
  }
  return RecordMmap::MakeRecord(attr, UINT_MAX, 0, module_mmap.start_addr, module_mmap.len, 0,
                                filename.c_str(), true, false);
}

std::unique_ptr<Record> CreateThreadCommRecord(const ThreadComm& thread_comm,
                                               const EventAttr& attr) {
  return RecordComm::MakeRecord(attr, thread_comm.tgid, thread_comm.tid, thread_comm.comm.c_str());
}

std::unique_ptr<Record> CreateThreadMmapRecord(pid_t pid, pid_t tid,
                                               const ThreadMmap& thread_mmap,
                                               const EventAttr& attr) {
  return RecordMmap::MakeRecord(attr, pid, tid, thread_mmap.start_addr, thread_mmap.len,
                                thread_mmap.pgoff, thread_mmap.name.c_str(), false,
                                (thread_mmap.executable) ? false : true);
}

std::unique_ptr<Record> CreateBuildIdRecord(pid_t pid, const BuildId& build_id,
                                            const char* filename, bool in_kernel) {
  return RecordBuildId::MakeRecord(pid, build_id, filename, in_kernel);
}

// RecordBuildId may come from build_id_table in feature section in perf.data, which may not
// set header.type.
std::unique_ptr<Record> BuildRecordBuildId(const char* buf, size_t size, const char** new_buf_p) {
  const perf_event_header* header = reinterpret_cast<const perf_event_header*>(buf);
  if (header->size > size) {
    return std::unique_ptr<Record>(nullptr);
  }
  char* record_buf = new char[header->size];
  memcpy(record_buf, buf, header->size);
  perf_event_header* record_header = reinterpret_cast<perf_event_header*>(record_buf);
  record_header->type = PERF_RECORD_BUILD_ID;
  auto result = BuildRecordOnBuffer(record_buf, record_header->size, nullptr);
  if (result != nullptr) {
    *new_buf_p = buf + record_header->size;
  }
  return result;
}

size_t SampleId::GetSizeOnBuffer(const EventAttr& attr) {
  size_t result = 0;
  if (attr.GetSampleAll()) {
    uint64_t sample_type = attr.SampleType();
    if (sample_type & PERF_SAMPLE_TID) {
      result += 8;
    }
    if (sample_type & PERF_SAMPLE_TIME) {
      result += 8;
    }
    if (sample_type & PERF_SAMPLE_ID) {
      result += 8;
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
      result += 8;
    }
    if (sample_type & PERF_SAMPLE_CPU) {
      result += 8;
    }
    if (sample_type & PERF_SAMPLE_IDENTIFIER) {
      result += 8;
    }
  }
  return result;
}

Record::Record(char* buf, size_t size)
    : header(reinterpret_cast<perf_event_header*>(buf)), buf(buf), size(size),
      sample_id_all(false), sample_type(0) {
}

Record::~Record() {
  delete[] buf;
}

void Record::Print() const {
  PrintRecordHeader(header);
  PrintData();
  PrintSampleId();
}

void Record::ParseSampleId(char* sample_id_buf, size_t sample_id_size, const EventAttr& attr) {
  sample_id_all = attr.GetSampleAll();
  sample_type = attr.SampleType();
  if (sample_id_all) {
    char* p = sample_id_buf;
    if (sample_type & PERF_SAMPLE_TID) {
      sample_id.pid = *reinterpret_cast<uint32_t*>(p);
      p += 4;
      sample_id.tid = *reinterpret_cast<uint32_t*>(p);
      p += 4;
    }
    if (sample_type & PERF_SAMPLE_TIME) {
      sample_id.time = *reinterpret_cast<uint64_t*>(p);
      p += 8;
    }
    if (sample_type & PERF_SAMPLE_ID) {
      sample_id.id = *reinterpret_cast<uint64_t*>(p);
      p += 8;
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
      sample_id.stream_id = *reinterpret_cast<uint64_t*>(p);
      p += 8;
    }
    if (sample_type & PERF_SAMPLE_CPU) {
      sample_id.cpu = *reinterpret_cast<uint32_t*>(p);
      p += 4;
      sample_id.res = *reinterpret_cast<uint32_t*>(p);
      p += 4;
    }
    if (sample_type & PERF_SAMPLE_IDENTIFIER) {
      sample_id.identifier = *reinterpret_cast<uint64_t*>(p);
      p += 8;
    }
    if (p != sample_id_buf + sample_id_size) {
      printf("Record has %zd bytes left\n", sample_id_buf + sample_id_size - p);
    }
  }
}

void Record::PrintSampleId() const {
  if (sample_id_all) {
    if (sample_type & PERF_SAMPLE_TID) {
      printf("  sample_id: pid %u, tid %u\n", sample_id.pid, sample_id.tid);
    }
    if (sample_type & PERF_SAMPLE_TIME) {
      printf("  sample_id: time %" PRId64 "\n", sample_id.time);
    }
    if (sample_type & PERF_SAMPLE_ID) {
      printf("  sample_id: id %" PRId64 "\n", sample_id.id);
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
      printf("  sample_id: stream_id %" PRId64 "\n", sample_id.stream_id);
    }
    if (sample_type & PERF_SAMPLE_CPU) {
      printf("  sample_id: cpu %u, res %u\n", sample_id.cpu, sample_id.res);
    }
    if (sample_type & PERF_SAMPLE_IDENTIFIER) {
      printf("  sample_id: identifier %" PRId64 "\n", sample_id.identifier);
    }
  }
}

RecordMmap::RecordMmap(char* buf, size_t size, const EventAttr& attr) : Record(buf, size) {
  char* p = buf + sizeof(perf_event_header);
  pid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  tid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  addr = *reinterpret_cast<uint64_t*>(p);
  p += 8;
  len = *reinterpret_cast<uint64_t*>(p);
  p += 8;
  pgoff = *reinterpret_cast<uint64_t*>(p);
  p += 8;
  filename = p;
  p += ALIGN(strlen(filename) + 1, 8);
  ParseSampleId(p, buf + size - p, attr);
}

void RecordMmap::PrintData() const {
  printf("  pid %u, tid %u, addr %p, len 0x%" PRIx64 "\n", pid, tid, reinterpret_cast<void*>(addr), len);
  printf("  pgoff 0x%" PRIx64 ", filename %s\n", pgoff, filename);
}

std::unique_ptr<Record> RecordMmap::MakeRecord(const EventAttr& attr,
                                               uint32_t pid, uint32_t tid, uint64_t addr,
                                               uint64_t len, uint64_t pgoff,
                                               const char* filename, bool in_kernel,
                                               bool is_data) {
  size_t buf_size = sizeof(perf_event_header) + 32 + ALIGN(strlen(filename) + 1, 8) +
                    SampleId::GetSizeOnBuffer(attr);
  char* buf = new char[buf_size];
  memset(buf, 0, buf_size);
  perf_event_header* header = reinterpret_cast<perf_event_header*>(buf);
  header->type = PERF_RECORD_MMAP;
  header->misc = in_kernel ? PERF_RECORD_MISC_KERNEL : PERF_RECORD_MISC_USER;
  if (is_data) {
    header->misc |= PERF_RECORD_MISC_MMAP_DATA;
  }
  header->size = buf_size;
  char* p = buf + sizeof(perf_event_header);
  *reinterpret_cast<uint32_t*>(p) = pid;
  p += 4;
  *reinterpret_cast<uint32_t*>(p) = tid;
  p += 4;
  *reinterpret_cast<uint64_t*>(p) = addr;
  p += 8;
  *reinterpret_cast<uint64_t*>(p) = len;
  p += 8;
  *reinterpret_cast<uint64_t*>(p) = pgoff;
  p += 8;
  strcpy(p, filename);
  p += ALIGN(strlen(filename) + 1, 8);

  // TODO: Write SampleId data.

  return BuildRecordOnBuffer(buf, buf_size, &attr);
}

RecordComm::RecordComm(char* buf, size_t size, const EventAttr& attr) : Record(buf, size) {
  char* p = buf + sizeof(perf_event_header);
  pid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  tid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  comm = p;
  p += ALIGN(strlen(comm) + 1, 8);
  ParseSampleId(p, buf + size - p, attr);
}

void RecordComm::PrintData() const {
  printf("  pid %u, tid %u, comm %s\n", pid, tid, comm);
}

std::unique_ptr<Record> RecordComm::MakeRecord(const EventAttr& attr, uint32_t pid, uint32_t tid,
                                               const char* comm) {
  size_t buf_size = sizeof(perf_event_header) + 8 + ALIGN(strlen(comm) + 1, 8) +
                    SampleId::GetSizeOnBuffer(attr);
  char* buf = new char[buf_size];
  memset(buf, 0, buf_size);
  perf_event_header* header = reinterpret_cast<perf_event_header*>(buf);
  header->type = PERF_RECORD_COMM;
  header->misc = 0;
  header->size = buf_size;
  char* p = buf + sizeof(perf_event_header);
  *reinterpret_cast<uint32_t*>(p) = pid;
  p += 4;
  *reinterpret_cast<uint32_t*>(p) = tid;
  p += 4;
  strcpy(p, comm);
  p += ALIGN(strlen(comm) + 1, 8);

  // TODO: Write SampleId data.

  return BuildRecordOnBuffer(buf, buf_size, &attr);
}

RecordExit::RecordExit(char* buf, size_t size, const EventAttr& attr) : Record(buf, size) {
  char* p = buf + sizeof(perf_event_header);
  pid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  ppid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  tid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  ptid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  time = *reinterpret_cast<uint64_t*>(p);
  p += 8;
  ParseSampleId(p, buf + size - p, attr);
}

void RecordExit::PrintData() const {
  printf("  pid %u, ppid %u, tid %u, ptid %u\n", pid, ppid, tid, ptid);
}

RecordSample::RecordSample(char* buf, size_t size, const EventAttr& attr) : Record(buf, size) {
  char* p = buf + sizeof(perf_event_header);
  sample_type = attr.SampleType();

  if (sample_type & PERF_SAMPLE_IP) {
    ip = *reinterpret_cast<uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_TID) {
    pid = *reinterpret_cast<uint32_t*>(p);
    p += 4;
    tid = *reinterpret_cast<uint32_t*>(p);
    p += 4;
  }
  if (sample_type & PERF_SAMPLE_TIME) {
    time = *reinterpret_cast<uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_ADDR) {
    addr = *reinterpret_cast<uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_ID) {
    id = *reinterpret_cast<uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_STREAM_ID) {
    stream_id = *reinterpret_cast<uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_CPU) {
    cpu = *reinterpret_cast<uint32_t*>(p);
    p += 4;
    res = *reinterpret_cast<uint32_t*>(p);
    p += 4;
  }
  if (sample_type & PERF_SAMPLE_PERIOD) {
    period = *reinterpret_cast<uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_READ) {
    read_values = *reinterpret_cast<ReadFormat*>(p);
    p += sizeof(ReadFormat);
  }
  if (sample_type & PERF_SAMPLE_CALLCHAIN) {
    callchain_nr = *reinterpret_cast<uint64_t*>(p);
    p += 8;
    callchain_ips = reinterpret_cast<uint64_t*>(p);
    p += 8 * callchain_nr;
  }
  if (p != buf + size) {
    fprintf(stdout, "RecordSample has %zd bytes left\n", buf + size - p);
  }
}

void RecordSample::PrintData() const {
  printf("  sample_type: 0x%" PRIx64 "\n", sample_type);

  if (sample_type & PERF_SAMPLE_IP) {
    printf("  ip %p\n", reinterpret_cast<void*>(ip));
  }
  if (sample_type & PERF_SAMPLE_TID) {
    printf("  pid %u, tid %u\n", pid, tid);
  }
  if (sample_type & PERF_SAMPLE_TIME) {
    printf("  time %" PRId64 "\n", time);
  }
  if (sample_type & PERF_SAMPLE_ADDR) {
    printf("  addr %p\n", reinterpret_cast<void*>(addr));
  }
  if (sample_type & PERF_SAMPLE_ID) {
    printf("  id %" PRId64 "\n", id);
  }
  if (sample_type & PERF_SAMPLE_STREAM_ID) {
    printf("  stream_id %" PRId64 "\n", stream_id);
  }
  if (sample_type & PERF_SAMPLE_CPU) {
    printf("  cpu %u, res %u\n", cpu, res);
  }
  if (sample_type & PERF_SAMPLE_READ) {
    printf("  read_values: value %" PRId64 ", time_enabled %" PRId64 ", time_running %" PRId64 ", id %" PRId64 "\n", read_values.value, read_values.time_enabled, read_values.time_running, read_values.id);
  }
}

RecordBuildId::RecordBuildId(char* buf, size_t size) : Record(buf, size) {
  char* p = buf + sizeof(perf_event_header);
  pid = *reinterpret_cast<pid_t*>(p);
  p += sizeof(pid_t);
  std::copy_n(p, build_id.size(), build_id.begin());
  p += ALIGN(build_id.size(), 8);
  filename = p;
  p += ALIGN(strlen(filename) + 1, 64);
  if (p != buf + size) {
    fprintf(stdout, "RecordBuildId has %zd bytes left\n", buf + size - p);
  }
}

void RecordBuildId::PrintData() const {
  printf("  pid %d\n", pid);
  printf("  build_id 0x");
  for (size_t i = 0; i < build_id.size(); ++i) {
    printf("%02x", build_id[i]);
  }
  printf("\n");
  printf("  filename %s\n", filename);
}

std::unique_ptr<Record> RecordBuildId::MakeRecord(pid_t pid,
                                   const BuildId& build_id,
                                   const char* filename, bool in_kernel) {
  size_t buf_size = sizeof(perf_event_header) + sizeof(pid_t) + ALIGN(BUILD_ID_SIZE, 8) +
                    ALIGN(strlen(filename) + 1, 64);
  char* buf = new char[buf_size];
  memset(buf, 0, buf_size);
  perf_event_header* header = reinterpret_cast<perf_event_header*>(buf);
  header->type = PERF_RECORD_BUILD_ID;
  header->misc = (in_kernel) ? PERF_RECORD_MISC_KERNEL : PERF_RECORD_MISC_USER;
  header->size = buf_size;
  char* p = buf + sizeof(perf_event_header);
  *reinterpret_cast<pid_t*>(p) = pid;
  p += sizeof(pid_t);
  std::copy_n(build_id.begin(), build_id.size(), p);
  p += ALIGN(BUILD_ID_SIZE, 8);
  strcpy(p, filename);
  return BuildRecordOnBuffer(buf, buf_size, nullptr);
}
