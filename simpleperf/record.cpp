#include "record.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "event_attr.h"

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
    default: return nullptr;
  }
}

static void PrintRecordHeader(const perf_event_header* header) {
  printf("record %s: type %u, misc %u, size %u\n", GetHeaderName(header), header->type,
                                                   header->misc, header->size);
}

std::unique_ptr<Record> BuildRecordOnBuffer(char* buf, size_t size, EventAttr& attr) {
  perf_event_header* header = reinterpret_cast<perf_event_header*>(buf);
  if (header->type == PERF_RECORD_MMAP) {
    return std::unique_ptr<Record>(new RecordMmap(buf, size));
  } else if (header->type == PERF_RECORD_COMM) {
    return std::unique_ptr<Record>(new RecordComm(buf, size));
  } else if (header->type == PERF_RECORD_EXIT) {
    return std::unique_ptr<Record>(new RecordExit(buf, size));
  } else if (header->type == PERF_RECORD_SAMPLE) {
    return std::unique_ptr<Record>(new RecordSample(buf, size, attr));
  }

  return std::unique_ptr<Record>(new Record(buf, size));
}

Record::Record(char* buf, size_t size)
  : header(reinterpret_cast<perf_event_header*>(buf)), buf(buf), size(size) {
}

Record::~Record() {
  delete[] buf;
}

void Record::Print() {
  PrintRecordHeader(header);
}

RecordMmap::RecordMmap(char* buf, size_t size)
  : Record(buf, size) {
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
  p += strlen(filename) + 1;
  if (p != buf + size) {
    fprintf(stderr, "RecordMmap has %zd bytes left\n", buf + size - p);
  }
}

void RecordMmap::Print() {
  Record::Print();
  printf("  pid %u, tid %u, addr %p, len %" PRIu64 "\n", pid, tid, reinterpret_cast<void*>(addr), len);
  printf("  pgoff %" PRIu64 ", filename %s\n", pgoff, filename);
}

RecordComm::RecordComm(char* buf, size_t size)
  : Record(buf, size) {
  char* p = buf + sizeof(perf_event_header);
  pid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  tid = *reinterpret_cast<uint32_t*>(p);
  p += 4;
  comm = p;
  p += strlen(comm) + 1;
  if (p != buf + size) {
    fprintf(stderr, "RecordComm has %zd bytes left\n", buf + size - p);
  }
}

void RecordComm::Print() {
  Record::Print();
  printf("  pid %u, tid %u, comm %s\n", pid, tid, comm);
}

RecordExit::RecordExit(char* buf, size_t size)
  : Record(buf, size) {
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
  if (p != buf + size) {
    fprintf(stderr, "RecordExit has %zd bytes left\n", buf + size - p);
  }
}

void RecordExit::Print() {
  Record::Print();
  printf("  pid %u, ppid %u, tid %u, ptid %u\n", pid, ppid, tid, ptid);
}

RecordSample::RecordSample(char* buf, size_t size, EventAttr& attr)
  : Record(buf, size) {
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
    fprintf(stderr, "SampleRecord has %zd bytes left\n", buf + size - p);
  }
}

void RecordSample::Print() {
  Record::Print();
  printf("  sample_type: 0x%" PRIx64 "\n", sample_type);

  if (sample_type & PERF_SAMPLE_IP) {
    printf("  ip: %p\n", reinterpret_cast<void*>(ip));
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
