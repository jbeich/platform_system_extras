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
#include <unordered_map>

#include <base/logging.h>
#include <base/stringprintf.h>

#include "environment.h"
#include "event_attr.h"
#include "utils.h"

static std::string RecordTypeToString(int record_type) {
  static std::unordered_map<int, std::string> record_type_names = {
      {PERF_RECORD_MMAP, "mmap"},
      {PERF_RECORD_LOST, "lost"},
      {PERF_RECORD_COMM, "comm"},
      {PERF_RECORD_EXIT, "exit"},
      {PERF_RECORD_THROTTLE, "throttle"},
      {PERF_RECORD_UNTHROTTLE, "unthrottle"},
      {PERF_RECORD_FORK, "fork"},
      {PERF_RECORD_READ, "read"},
      {PERF_RECORD_SAMPLE, "sample"},
      {PERF_RECORD_BUILD_ID, "build_id"},
  };

  auto it = record_type_names.find(record_type);
  if (it != record_type_names.end()) {
    return it->second;
  }
  return android::base::StringPrintf("unknown(%d)", record_type);
}

Record::Record() {
  memset(&header, 0, sizeof(header));
  memset(&sample_id, 0, sizeof(sample_id));
}

Record::Record(const perf_event_header* pheader) {
  header = *pheader;
  memset(&sample_id, 0, sizeof(sample_id));
}

void Record::Dump(size_t indent) const {
  PrintIndented(indent, "record %s: type %u, misc %u, size %u\n",
                RecordTypeToString(header.type).c_str(), header.type, header.misc, header.size);
  DumpData(indent + 1);
  DumpSampleId(indent + 1);
}

void Record::ParseSampleId(const EventAttr& attr, const char* p, const char* end) {
  sample_id.sample_id_all = attr.GetSampleAll();
  sample_id.sample_type = attr.SampleType();
  if (sample_id.sample_id_all) {
    if (sample_id.sample_type & PERF_SAMPLE_TID) {
      sample_id.pid = *reinterpret_cast<const uint32_t*>(p);
      p += 4;
      sample_id.tid = *reinterpret_cast<const uint32_t*>(p);
      p += 4;
    }
    if (sample_id.sample_type & PERF_SAMPLE_TIME) {
      sample_id.time = *reinterpret_cast<const uint64_t*>(p);
      p += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_ID) {
      sample_id.id = *reinterpret_cast<const uint64_t*>(p);
      p += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_STREAM_ID) {
      sample_id.stream_id = *reinterpret_cast<const uint64_t*>(p);
      p += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_CPU) {
      sample_id.cpu = *reinterpret_cast<const uint32_t*>(p);
      p += 4;
      sample_id.res = *reinterpret_cast<const uint32_t*>(p);
      p += 4;
    }
    CHECK_LE(p, end);
  }
  if (p < end) {
    LOG(DEBUG) << "Record has " << end - p << " bytes left\n";
  }
}

void Record::DumpSampleId(size_t indent) const {
  if (sample_id.sample_id_all) {
    if (sample_id.sample_type & PERF_SAMPLE_TID) {
      PrintIndented(indent, "sample_id: pid %u, tid %u\n", sample_id.pid, sample_id.tid);
    }
    if (sample_id.sample_type & PERF_SAMPLE_TIME) {
      PrintIndented(indent, "sample_id: time %" PRId64 "\n", sample_id.time);
    }
    if (sample_id.sample_type & PERF_SAMPLE_ID) {
      PrintIndented(indent, "sample_id: stream_id %" PRId64 "\n", sample_id.id);
    }
    if (sample_id.sample_type & PERF_SAMPLE_STREAM_ID) {
      PrintIndented(indent, "sample_id: stream_id %" PRId64 "\n", sample_id.stream_id);
    }
    if (sample_id.sample_type & PERF_SAMPLE_CPU) {
      PrintIndented(indent, "sample_id: cpu %u, res %u\n", sample_id.cpu, sample_id.res);
    }
  }
}

// Return sample_id size in binary format.
size_t Record::SetSampleId(const EventAttr& attr) {
  sample_id.sample_id_all = attr.GetSampleAll();
  sample_id.sample_type = attr.SampleType();
  // Other data are not necessary. TODO: Set missing SampleId data.
  size_t sample_id_size = 0;
  if (sample_id.sample_id_all) {
    if (sample_id.sample_type & PERF_SAMPLE_TID) {
      sample_id_size += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_TIME) {
      sample_id_size += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_ID) {
      sample_id_size += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_STREAM_ID) {
      sample_id_size += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_CPU) {
      sample_id_size += 8;
    }
  }
  return sample_id_size;
}

void Record::GetSampleId(char*& p) const {
  if (sample_id.sample_id_all) {
    if (sample_id.sample_type & PERF_SAMPLE_TID) {
      memcpy(p, &sample_id.pid, 4);
      p += 4;
      memcpy(p, &sample_id.tid, 4);
      p += 4;
    }
    if (sample_id.sample_type & PERF_SAMPLE_TIME) {
      memcpy(p, &sample_id.time, 8);
      p += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_ID) {
      memcpy(p, &sample_id.id, 8);
      p += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_STREAM_ID) {
      memcpy(p, &sample_id.stream_id, 8);
      p += 8;
    }
    if (sample_id.sample_type & PERF_SAMPLE_CPU) {
      memcpy(p, &sample_id.cpu, 4);
      p += 4;
      memcpy(p, &sample_id.res, 4);
      p += 4;
    }
  }
}

MmapRecord::MmapRecord(const EventAttr& attr, const perf_event_header* pheader) : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  pid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  tid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  addr = *reinterpret_cast<const uint64_t*>(p);
  p += 8;
  len = *reinterpret_cast<const uint64_t*>(p);
  p += 8;
  pgoff = *reinterpret_cast<const uint64_t*>(p);
  p += 8;
  filename = p;
  p += ALIGN(strlen(p) + 1, 8);
  CHECK_LE(p, end);
  ParseSampleId(attr, p, end);
}

void MmapRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u, tid %u, addr %p, len 0x%" PRIx64 "\n", pid, tid,
                reinterpret_cast<void*>(addr), len);
  PrintIndented(indent, "pgoff 0x%" PRIx64 ", filename %s\n", pgoff, filename.c_str());
}

std::vector<char> MmapRecord::BinaryFormat() const {
  std::vector<char> buf(header.size);
  char* p = buf.data();
  memcpy(p, &header, sizeof(header));
  p += sizeof(header);
  memcpy(p, &pid, 4);
  p += 4;
  memcpy(p, &tid, 4);
  p += 4;
  memcpy(p, &addr, 8);
  p += 8;
  memcpy(p, &len, 8);
  p += 8;
  memcpy(p, &pgoff, 8);
  p += 8;
  strcpy(p, filename.c_str());
  p += ALIGN(filename.size() + 1, 8);
  GetSampleId(p);
  return buf;
}

CommRecord::CommRecord(const EventAttr& attr, const perf_event_header* pheader) : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  pid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  tid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  comm = p;
  p += ALIGN(strlen(p) + 1, 8);
  CHECK_LE(p, end);
  ParseSampleId(attr, p, end);
}

void CommRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u, tid %u, comm %s\n", pid, tid, comm.c_str());
}

std::vector<char> CommRecord::BinaryFormat() const {
  std::vector<char> buf(header.size);
  char* p = buf.data();
  memcpy(p, &header, sizeof(header));
  p += sizeof(header);
  memcpy(p, &pid, 4);
  p += 4;
  memcpy(p, &tid, 4);
  p += 4;
  strcpy(p, comm.c_str());
  p += ALIGN(comm.size() + 1, 8);
  GetSampleId(p);
  return buf;
}

ExitRecord::ExitRecord(const EventAttr& attr, const perf_event_header* pheader) : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  pid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  ppid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  tid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  ptid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  time = *reinterpret_cast<const uint64_t*>(p);
  p += 8;
  CHECK_LE(p, end);
  ParseSampleId(attr, p, end);
}

void ExitRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u, ppid %u, tid %u, ptid %u\n", pid, ppid, tid, ptid);
}

SampleRecord::SampleRecord(const EventAttr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  sample_type = attr.SampleType();

  if (sample_type & PERF_SAMPLE_IP) {
    ip = *reinterpret_cast<const uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_TID) {
    pid = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    tid = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
  }
  if (sample_type & PERF_SAMPLE_TIME) {
    time = *reinterpret_cast<const uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_ADDR) {
    addr = *reinterpret_cast<const uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_ID) {
    id = *reinterpret_cast<const uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_STREAM_ID) {
    id = *reinterpret_cast<const uint64_t*>(p);
    p += 8;
  }
  if (sample_type & PERF_SAMPLE_CPU) {
    cpu = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    res = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
  }
  if (sample_type & PERF_SAMPLE_PERIOD) {
    period = *reinterpret_cast<const uint64_t*>(p);
    p += 8;
  }
  CHECK_LE(p, end);
  if (p < end) {
    LOG(DEBUG) << "Record has " << end - p << " bytes left\n";
  }
}

void SampleRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "sample_type: 0x%" PRIx64 "\n", sample_type);
  if (sample_type & PERF_SAMPLE_IP) {
    PrintIndented(indent, "ip %p\n", reinterpret_cast<void*>(ip));
  }
  if (sample_type & PERF_SAMPLE_TID) {
    PrintIndented(indent, "pid %u, tid %u\n", pid, tid);
  }
  if (sample_type & PERF_SAMPLE_TIME) {
    PrintIndented(indent, "time %" PRId64 "\n", time);
  }
  if (sample_type & PERF_SAMPLE_ADDR) {
    PrintIndented(indent, "addr %p\n", reinterpret_cast<void*>(addr));
  }
  if (sample_type & PERF_SAMPLE_ID) {
    PrintIndented(indent, "id %" PRId64 "\n", id);
  }
  if (sample_type & PERF_SAMPLE_STREAM_ID) {
    PrintIndented(indent, "stream_id %" PRId64 "\n", stream_id);
  }
  if (sample_type & PERF_SAMPLE_CPU) {
    PrintIndented(indent, "cpu %u, res %u\n", cpu, res);
  }
  if (sample_type & PERF_SAMPLE_PERIOD) {
    PrintIndented(indent, "period %" PRId64 "\n", period);
  }
}

BuildIdRecord::BuildIdRecord(const EventAttr&, const perf_event_header* pheader)
    : BuildIdRecord(pheader) {
}

BuildIdRecord::BuildIdRecord(const perf_event_header* pheader) : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  pid = *reinterpret_cast<const uint32_t*>(p);
  p += 4;
  std::copy_n(p, build_id.size(), build_id.begin());
  p += ALIGN(build_id.size(), 8);
  filename = p;
  p += ALIGN(strlen(p) + 1, 64);
  CHECK_LE(p, end);
  if (p < end) {
    LOG(DEBUG) << "Record has " << end - p << " bytes left\n";
  }
}

void BuildIdRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %d\n", pid);
  PrintIndented(indent, "build_id 0x");
  for (size_t i = 0; i < build_id.size(); ++i) {
    printf("%02x", build_id[i]);
  }
  printf("\n");
  PrintIndented(indent, "filename %s\n", filename.c_str());
}

std::vector<char> BuildIdRecord::BinaryFormat() const {
  std::vector<char> buf(header.size);
  char* p = buf.data();
  memcpy(p, &header, sizeof(header));
  p += sizeof(header);
  memcpy(p, &pid, 4);
  p += 4;
  memcpy(p, build_id.data(), build_id.size());
  p += ALIGN(build_id.size(), 8);
  strcpy(p, filename.c_str());
  p += ALIGN(filename.size() + 1, 64);
  GetSampleId(p);
  return buf;
}

std::unique_ptr<const Record> ReadRecordFromBuffer(const EventAttr& attr,
                                                   const perf_event_header* pheader) {
  switch (pheader->type) {
    case PERF_RECORD_MMAP:
      return std::unique_ptr<const Record>(new MmapRecord(attr, pheader));
    case PERF_RECORD_COMM:
      return std::unique_ptr<const Record>(new CommRecord(attr, pheader));
    case PERF_RECORD_EXIT:
      return std::unique_ptr<const Record>(new ExitRecord(attr, pheader));
    case PERF_RECORD_SAMPLE:
      return std::unique_ptr<const Record>(new SampleRecord(attr, pheader));
    case PERF_RECORD_BUILD_ID:
      return std::unique_ptr<const Record>(new BuildIdRecord(attr, pheader));
    default:
      return std::unique_ptr<const Record>(new Record(pheader));
  }
}

MmapRecord CreateKernelMmapRecord(const KernelMmap& kernel_mmap, const EventAttr& attr) {
  MmapRecord record;
  record.header.type = PERF_RECORD_MMAP;
  record.header.misc = PERF_RECORD_MISC_KERNEL;
  record.pid = UINT_MAX;
  record.tid = 0;
  record.addr = kernel_mmap.start_addr;
  record.len = kernel_mmap.len;
  record.pgoff = kernel_mmap.pgoff;
  record.filename = kernel_mmap.name;
  size_t sample_id_size = record.SetSampleId(attr);
  record.header.size =
      sizeof(record.header) + 32 + ALIGN(record.filename.size() + 1, 8) + sample_id_size;
  return record;
}

MmapRecord CreateModuleMmapRecord(const ModuleMmap& module_mmap, const EventAttr& attr) {
  MmapRecord record;
  record.header.type = PERF_RECORD_MMAP;
  record.header.misc = PERF_RECORD_MISC_KERNEL;
  record.pid = UINT_MAX;
  record.tid = 0;
  record.addr = module_mmap.start_addr;
  record.len = module_mmap.len;
  record.pgoff = 0;

  std::string filename = module_mmap.filepath;
  if (filename.empty()) {
    filename = "[" + module_mmap.name + "]";
  }
  record.filename = filename;
  size_t sample_id_size = record.SetSampleId(attr);
  record.header.size =
      sizeof(record.header) + 32 + ALIGN(record.filename.size() + 1, 8) + sample_id_size;
  return record;
}

MmapRecord CreateThreadMmapRecord(const ThreadComm& thread, const ThreadMmap& thread_mmap,
                                  const EventAttr& attr) {
  MmapRecord record;
  record.header.type = PERF_RECORD_MMAP;
  record.header.misc = PERF_RECORD_MISC_USER;
  record.pid = thread.tgid;
  record.tid = thread.tid;
  record.addr = thread_mmap.start_addr;
  record.len = thread_mmap.len;
  record.pgoff = thread_mmap.pgoff;
  record.filename = thread_mmap.name;
  size_t sample_id_size = record.SetSampleId(attr);
  record.header.size =
      sizeof(record.header) + 32 + ALIGN(record.filename.size() + 1, 8) + sample_id_size;
  return record;
}

CommRecord CreateThreadCommRecord(const ThreadComm& thread, const EventAttr& attr) {
  CommRecord record;
  record.header.type = PERF_RECORD_COMM;
  record.header.misc = 0;
  record.pid = thread.tgid;
  record.tid = thread.tid;
  record.comm = thread.comm;
  size_t sample_id_size = record.SetSampleId(attr);
  record.header.size = sizeof(record.header) + 8 + ALIGN(record.comm.size() + 1, 8) + sample_id_size;
  return record;
}

BuildIdRecord CreateBuildIdRecordForFeatureSection(int pid, const BuildId& build_id,
                                                   const std::string& filename, bool in_kernel) {
  BuildIdRecord record;
  record.header.type = PERF_RECORD_BUILD_ID;
  record.header.misc = (in_kernel ? PERF_RECORD_MISC_KERNEL : PERF_RECORD_MISC_USER);
  record.pid = pid;
  record.build_id = build_id;
  record.filename = filename;
  // As we create BuildIdRecord for feature section, we don't need the SampleId part.
  record.header.size =
      sizeof(record.header) + 4 + ALIGN(BUILD_ID_SIZE, 8) + ALIGN(filename.size() + 1, 64);
  return record;
}
