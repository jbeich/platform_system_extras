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

#include <stdint.h>
#include <memory>

#include "build_id.h"
#include "environment.h"
#include "perf_event.h"

class EventAttr;

enum user_record_type {
  PERF_RECORD_ATTR = 64,
  PERF_RECORD_EVENT_TYPE,
  PERF_RECORD_TRACING_DATA,
  PERF_RECORD_BUILD_ID,
  PERF_RECORD_FINISHED_ROUND,
};

struct SampleId {
  uint32_t pid, tid;  // If sample_id_all && PERF_SAMPLE_TID
  uint64_t time;   // If sample_id_all && PERF_SAMPLE_TIME
  uint64_t id;     // If sample_id_all && PERF_SAMPLE_ID
  uint64_t stream_id;  // If sample_id_all && PERF_SAMPLE_STREAM_ID
  uint32_t cpu, res;   // If sample_id_all && PERF_SAMPLE_CPU
  uint64_t identifier;  // If sample_id_all && PERF_SAMPLE_IDENTIFIER

  static size_t GetSizeOnBuffer(const EventAttr& attr);
};

class Record {
 public:
  Record(char* buf, size_t size);

  virtual ~Record();

  void Print() const;

  const char* GetBuf() const {
    return buf;
  }

  size_t GetBufSize() const {
    return size;
  }

  uint32_t Type() const {
    return header->type;
  }

  bool InKernel() {
    return (header->misc & PERF_RECORD_MISC_KERNEL) != 0;
  }

 protected:
  void ParseSampleId(char* sample_id_buf, size_t sample_id_size, const EventAttr& attr);

  virtual void PrintData() const { }
  void PrintSampleId() const;

 protected:
  perf_event_header* header;
  char* buf;
  size_t size;

  bool sample_id_all;
  uint64_t sample_type;
  SampleId sample_id;
};

class RecordMmap : public Record {
 public:
  RecordMmap(char* buf, size_t size, const EventAttr& attr);

  static std::unique_ptr<Record> MakeRecord(const EventAttr& attr,
                                            uint32_t pid, uint32_t tid, uint64_t addr,
                                            uint64_t len, uint64_t pgoff,
                                            const char* filename, bool in_kernel, bool is_data);
  pid_t Pid() const {
    return pid;
  }

  pid_t Tid() const {
    return tid;
  }

  uint64_t Addr() const {
    return addr;
  }

  uint64_t Len() const {
    return len;
  }

  const char* Filename() const {
    return filename;
  }

 protected:
  void PrintData() const override;

  uint32_t pid;
  uint32_t tid;
  uint64_t addr;
  uint64_t len;
  uint64_t pgoff;
  char* filename;
};

class RecordComm : public Record {
 public:
  RecordComm(char* buf, size_t size, const EventAttr& attr);

  static std::unique_ptr<Record> MakeRecord(const EventAttr& attr, uint32_t pid, uint32_t tid,
                                            const char* comm);

 protected:
  void PrintData() const override;

  uint32_t pid, tid;
  char* comm;
};

class RecordExit : public Record {
 public:
  RecordExit(char* buf, size_t size, const EventAttr& attr);

 protected:
  void PrintData() const override;

  uint32_t pid, ppid;
  uint32_t tid, ptid;
  uint64_t time;
};

struct ReadFormat {
  uint64_t value;
  uint64_t time_enabled;
  uint64_t time_running;
  uint64_t id;
};

class RecordSample : public Record {
 public:
  RecordSample(char* buf, size_t size, const EventAttr& attr);

  pid_t Pid() const {
    return pid;
  }

  uint64_t Ip() const {
    return ip;
  }

 protected:
  void PrintData() const override;

  uint64_t sample_type;

  uint64_t ip;        // If PERF_SAMPLE_IP
  uint32_t pid, tid;  // If PERF_SAMPLE_TID
  uint64_t time;      // If PERF_SAMPLE_TIME
  uint64_t addr;      // If PERF_SAMPLE_ADDR
  uint64_t id;        // If PERF_SAMPLE_ID
  uint64_t stream_id; // If PERF_SAMPLE_STREAM_ID
  uint32_t cpu, res;  // If PERF_SAMPLE_CPU
  uint64_t period;    // If PERF_SAMPLE_PERIOD
  struct ReadFormat read_values;  // If PERF_SAMPLE_READ
  uint64_t callchain_nr;   // If PERF_SAMPLE_CALLCHAIN
  uint64_t* callchain_ips; // If PERF_SAMPLE_CALLCHAIN
};

#define BUILD_ID_SIZE 20

class RecordBuildId : public Record {
 public:
  RecordBuildId(char* buf, size_t size);

  static std::unique_ptr<Record> MakeRecord(pid_t pid, const BuildId& build_id,
                                            const char* filename, bool in_kernel);

  const char* Filename() const {
    return filename;
  }

 protected:
  void PrintData() const override;

  pid_t pid;
  BuildId build_id;
  char* filename;
};

class EventAttr;

std::unique_ptr<Record> BuildRecordOnBuffer(char* buf, size_t size, const EventAttr* attr);

std::unique_ptr<Record> CreateKernelMmapRecord(const KernelMmap& kernel_mmap,
                                               const EventAttr& attr);

std::unique_ptr<Record> CreateModuleMmapRecord(const ModuleMmap& module_mmap,
                                               const EventAttr& attr);

std::unique_ptr<Record> CreateThreadCommRecord(const ThreadComm& thread_comm,
                                               const EventAttr& attr);

std::unique_ptr<Record> CreateThreadMmapRecord(pid_t pid, pid_t tid,
                                               const ThreadMmap& thread_mmap,
                                               const EventAttr& attr);

std::unique_ptr<Record> CreateBuildIdRecord(pid_t pid, const BuildId& build_id,
                                            const char* filename, bool in_kernel);

std::unique_ptr<Record> BuildRecordBuildId(const char* buf, size_t size, const char** new_buf_p);

#endif  // SIMPLE_PERF_RECORD_H_
