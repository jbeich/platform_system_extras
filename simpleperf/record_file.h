#ifndef SIMPLE_PERF_RECORD_FILE_H_
#define SIMPLE_PERF_RECORD_FILE_H_

#include <stdio.h>
#include <memory>
#include <string>
#include <vector>

class EventFd;

class RecordFile {
 public:
  static std::unique_ptr<RecordFile> CreateFile(const std::string& filename);

  ~RecordFile();

  RecordFile(const RecordFile&) = delete;
  RecordFile& operator=(const RecordFile&) = delete;

  bool WriteHeader(std::vector<std::unique_ptr<EventFd>>&);

  bool WriteData(const void* buf, size_t len);

  bool Close();

 private:
  RecordFile(const std::string& filename, FILE* fp);

  bool WriteOutput(const void* buf, size_t len);

 private:
  const std::string filename;
  FILE* record_fp;

  uint64_t data_offset;
  uint64_t data_size;
};

#endif  // SIMPLE_PERF_RECORD_FILE_H_
