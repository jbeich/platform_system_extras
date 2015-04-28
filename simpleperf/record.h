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

#ifndef SIMPLE_PERF_RECORD_H_
#define SIMPLE_PERF_RECORD_H_

#include <string>
#include <vector>

#include "perf_event.h"

class EventAttr;
struct KernelMmap;
struct ModuleMmap;
struct ThreadComm;
struct ThreadMmap;

enum user_record_type {
  PERF_RECORD_ATTR = 64,
  PERF_RECORD_EVENT_TYPE,
  PERF_RECORD_TRACING_DATA,
  PERF_RECORD_BUILD_ID,
  PERF_RECORD_FINISHED_ROUND,
};

struct SampleId {
  bool sample_id_all;
  uint64_t sample_type;

  uint32_t pid, tid;   // If sample_id_all && PERF_SAMPLE_TID
  uint64_t time;       // If sample_id_all && PERF_SAMPLE_TIME
  uint64_t id;         // If sample_id_all && PERF_SAMPLE_ID
  uint64_t stream_id;  // If sample_id_all && PERF_SAMPLE_STREAM_ID
  uint32_t cpu, res;   // If sample_id_all && PERF_SAMPLE_CPU
};

// Usually one record contains three parts:
//   perf_event_header
//   data depends on the specific record type
//   sample_id
// We hold the common parts (perf_event_header and sample_id in the base class Record.
struct Record {
  perf_event_header header;
  SampleId sample_id;

  Record();
  Record(const perf_event_header* pheader);

  virtual ~Record() {
  }

  void Dump(size_t indent = 0) const;

  // Create the content of sample_id. It depends on the attr we use.
  size_t CreateSampleId(const EventAttr& attr);

  // Write the binary format of sample_id to the buffer pointed by p.
  void SampleIdToBinaryFormat(char*& p) const;

 protected:
  virtual void DumpData(size_t) const {
  }

  void ParseSampleId(const EventAttr& attr, const char* p, const char* end);
  void DumpSampleId(size_t indent) const;
};

struct MmapRecord : public Record {
  union {
    struct {
      uint32_t pid, tid;
      uint64_t addr;
      uint64_t len;
      uint64_t pgoff;
    };
    char mdata[32];  // mdata[] is used only to read from or write to binary format.
  };
  std::string filename;

  MmapRecord() {  // For storage in std::vector.
  }

  MmapRecord(const EventAttr& attr, const perf_event_header* pheader);
  void DumpData(size_t indent) const override;
  std::vector<char> BinaryFormat() const;
};

struct CommRecord : public Record {
  union {
    struct {
      uint32_t pid, tid;
    };
    char mdata[8];
  };
  std::string comm;

  CommRecord() {
  }

  CommRecord(const EventAttr& attr, const perf_event_header* pheader);
  void DumpData(size_t indent) const override;
  std::vector<char> BinaryFormat() const;
};

struct ExitRecord : public Record {
  union {
    struct {
      uint32_t pid, ppid;
      uint32_t tid, ptid;
      uint64_t time;
    };
    char mdata[24];
  };

  ExitRecord(const EventAttr& attr, const perf_event_header* pheader);
  void DumpData(size_t indent) const override;
};

struct SampleRecord : public Record {
  uint64_t sample_type;  // sample_type determines which fields below are valid.

  uint64_t ip;         // If PERF_SAMPLE_IP
  uint32_t pid, tid;   // If PERF_SAMPLE_TID
  uint64_t time;       // If PERF_SAMPLE_TIME
  uint64_t addr;       // If PERF_SAMPLE_ADDR
  uint64_t id;         // If PERF_SAMPLE_ID
  uint64_t stream_id;  // If PERF_SAMPLE_STREAM_ID
  uint32_t cpu, res;   // If PERF_SAMPLE_CPU
  uint64_t period;     // If PERF_SAMPLE_PERIOD

  SampleRecord(const EventAttr& attr, const perf_event_header* pheader);
  void DumpData(size_t indent) const override;
};

std::unique_ptr<const Record> ReadRecordFromBuffer(const EventAttr& attr,
                                                   const perf_event_header* pheader);

#endif  // SIMPLE_PERF_RECORD_H_
