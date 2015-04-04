#ifndef SIMPLE_PERF_RECORD_H_
#define SIMPLE_PERF_RECORD_H_

#include <linux/perf_event.h>
#include <stdint.h>
#include <memory>

class Record {
 public:
  Record(char* buf, size_t size);
  virtual ~Record();
  virtual void Print();

 protected:
  perf_event_header* header;
  char* buf;
  size_t size;
};

class RecordMmap : public Record {
 public:
  RecordMmap(char* buf, size_t size);

  void Print() override;

 protected:
  uint32_t pid;
  uint32_t tid;
  uint64_t addr;
  uint64_t len;
  uint64_t pgoff;
  char* filename;
};

class RecordComm : public Record {
 public:
  RecordComm(char* buf, size_t size);

  void Print() override;

 protected:
  uint32_t pid, tid;
  char* comm;
};

class RecordExit : public Record {
 public:
  RecordExit(char* buf, size_t size);

  void Print() override;

 protected:
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

class EventAttr;

class RecordSample : public Record {
 public:
  RecordSample(char* buf, size_t size, EventAttr& attr);

  void Print() override;

 protected:
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

class EventAttr;

std::unique_ptr<Record> BuildRecordOnBuffer(char* buf, size_t size, EventAttr& attr);

#endif  // SIMPLE_PERF_RECORD_H_
