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

void Record::ParseSampleId(const perf_event_attr& attr, const char* p, const char* end) {
  sample_id.sample_id_all = attr.sample_id_all;
  sample_id.sample_type = attr.sample_type;
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
size_t Record::CreateSampleId(const perf_event_attr& attr) {
  sample_id.sample_id_all = attr.sample_id_all;
  sample_id.sample_type = attr.sample_type;
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

void Record::SampleIdToBinaryFormat(char*& p) const {
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

MmapRecord::MmapRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  memcpy(mdata, p, sizeof(mdata));
  p += sizeof(mdata);
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
  memcpy(p, mdata, sizeof(mdata));
  p += sizeof(mdata);
  strcpy(p, filename.c_str());
  p += ALIGN(filename.size() + 1, 8);
  SampleIdToBinaryFormat(p);
  return buf;
}

CommRecord::CommRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  memcpy(mdata, p, sizeof(mdata));
  p += sizeof(mdata);
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
  memcpy(p, mdata, sizeof(mdata));
  p += sizeof(mdata);
  strcpy(p, comm.c_str());
  p += ALIGN(comm.size() + 1, 8);
  SampleIdToBinaryFormat(p);
  return buf;
}

ExitRecord::ExitRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  memcpy(mdata, p, sizeof(mdata));
  p += sizeof(mdata);
  CHECK_LE(p, end);
  ParseSampleId(attr, p, end);
}

void ExitRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u, ppid %u, tid %u, ptid %u\n", pid, ppid, tid, ptid);
}

SampleRecord::SampleRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  sample_type = attr.sample_type;

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

std::unique_ptr<const Record> ReadRecordFromBuffer(const perf_event_attr& attr,
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
    default:
      return std::unique_ptr<const Record>(new Record(pheader));
  }
}
