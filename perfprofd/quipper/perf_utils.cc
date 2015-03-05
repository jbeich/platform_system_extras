// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define LOG_TAG "perf_reader"

#include "perf_utils.h"

#include <sys/stat.h>

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>  // NOLINT(readability/streams)
#include <iomanip>
#include <sstream>

#include "base/logging.h"
#include "base/macros.h"

namespace {

// Specify buffer size to be used to read files.
// This is allocated on the stack, so make sure it's less than 16k.
const int kFileReadSize = 1024;

// Number of hex digits in a byte.
const int kNumHexDigitsInByte = 2;

}  // namespace

namespace quipper {

int64_t GetFileSizeFromHandle(FILE* fp) {
  int64_t position = ftell(fp);
  fseek(fp, 0, SEEK_END);
  int64_t file_size = ftell(fp);
  // Restore the original file handle position.
  fseek(fp, position, SEEK_SET);
  return file_size;
}

event_t* CallocMemoryForEvent(size_t size) {
  event_t* event = reinterpret_cast<event_t*>(calloc(1, size));
  CHECK(event);
  return event;
}

event_t* ReallocMemoryForEvent(event_t* event, size_t new_size) {
  event_t* new_event = reinterpret_cast<event_t*>(realloc(event, new_size));
  CHECK(new_event);  // NB: event is "leaked" if this CHECK fails.
  return new_event;
}

build_id_event* CallocMemoryForBuildID(size_t size) {
  build_id_event* event = reinterpret_cast<build_id_event*>(calloc(1, size));
  CHECK(event);
  return event;
}

bool FileToBuffer(const string& filename, std::vector<char>* contents) {
  FILE* fp = fopen(filename.c_str(), "rb");
  if (!fp)
    return false;
  int64_t file_size = quipper::GetFileSizeFromHandle(fp);
  contents->resize(file_size);
  // Do not read anything if the file exists but is empty.
  if (file_size > 0)
    CHECK_GT(fread(contents->data(), file_size, 1, fp), 0U);
  fclose(fp);
  return true;
}

bool FileExists(const string& filename) {
  struct stat st;
  return stat(filename.c_str(), &st) == 0;
}

string HexToString(const u8* array, size_t length) {
  // Convert the bytes to hex digits one at a time.
  // There will be kNumHexDigitsInByte hex digits, and 1 char for NUL.
  char buffer[kNumHexDigitsInByte + 1];
  string result = "";
  for (size_t i = 0; i < length; ++i) {
    snprintf(buffer, sizeof(buffer), "%02x", array[i]);
    result += buffer;
  }
  return result;
}

bool StringToHex(const string& str, u8* array, size_t length) {
  const int kHexRadix = 16;
  char* err;
  // Loop through kNumHexDigitsInByte characters at a time (to get one byte)
  // Stop when there are no more characters, or the array has been filled.
  for (size_t i = 0;
       (i + 1) * kNumHexDigitsInByte <= str.size() && i < length;
       ++i) {
    string one_byte = str.substr(i * kNumHexDigitsInByte, kNumHexDigitsInByte);
    array[i] = strtol(one_byte.c_str(), &err, kHexRadix);
    if (*err)
      return false;
  }
  return true;
}

uint64_t AlignSize(uint64_t size, uint32_t align_size) {
  return ((size + align_size - 1) / align_size) * align_size;
}

// In perf data, strings are packed into the smallest number of 8-byte blocks
// possible, including the null terminator.
// e.g.
//    "0123"                ->  5 bytes -> packed into  8 bytes
//    "0123456"             ->  8 bytes -> packed into  8 bytes
//    "01234567"            ->  9 bytes -> packed into 16 bytes
//    "0123456789abcd"      -> 15 bytes -> packed into 16 bytes
//    "0123456789abcde"     -> 16 bytes -> packed into 16 bytes
//    "0123456789abcdef"    -> 17 bytes -> packed into 24 bytes
//
// Returns the size of the 8-byte-aligned memory for storing |string|.
size_t GetUint64AlignedStringLength(const string& str) {
  return AlignSize(str.size() + 1, sizeof(uint64_t));
}

uint64_t GetSampleFieldsForEventType(uint32_t event_type,
                                     uint64_t sample_type) {
  uint64_t mask = kuint64max;
  switch (event_type) {
  case PERF_RECORD_MMAP:
  case PERF_RECORD_LOST:
  case PERF_RECORD_COMM:
  case PERF_RECORD_EXIT:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
  case PERF_RECORD_FORK:
  case PERF_RECORD_READ:
  case PERF_RECORD_MMAP2:
    // See perf_event.h "struct" sample_id and sample_id_all.
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ID |
           PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_CPU | PERF_SAMPLE_IDENTIFIER;
    break;
  case PERF_RECORD_SAMPLE:
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event_type;
  }
  return sample_type & mask;
}

uint64_t GetPerfSampleDataOffset(const event_t& event) {
  uint64_t offset = kuint64max;
  switch (event.header.type) {
  case PERF_RECORD_SAMPLE:
    offset = offsetof(event_t, sample.array);
    break;
  case PERF_RECORD_MMAP:
    offset = sizeof(event.mmap) - sizeof(event.mmap.filename) +
             GetUint64AlignedStringLength(event.mmap.filename);
    break;
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
    offset = sizeof(event.fork);
    break;
  case PERF_RECORD_COMM:
    offset = sizeof(event.comm) - sizeof(event.comm.comm) +
             GetUint64AlignedStringLength(event.comm.comm);
    break;
  case PERF_RECORD_LOST:
    offset = sizeof(event.lost);
    break;
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
    offset = sizeof(event.throttle);
    break;
  case PERF_RECORD_READ:
    offset = sizeof(event.read);
    break;
  case PERF_RECORD_MMAP2:
    offset = sizeof(event.mmap2) - sizeof(event.mmap2.filename) +
             GetUint64AlignedStringLength(event.mmap2.filename);
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event.header.type;
    break;
  }
  // Make sure the offset was valid
  CHECK_NE(offset, kuint64max);
  CHECK_EQ(offset % sizeof(uint64_t), 0U);
  return offset;
}

bool ReadFileToData(const string& filename, std::vector<char>* data) {
  std::ifstream in(filename.c_str(), std::ios::binary);
  if (!in.good()) {
    LOG(ERROR) << "Failed to open file " << filename;
    return false;
  }
  in.seekg(0, in.end);
  size_t length = in.tellg();
  in.seekg(0, in.beg);
  data->resize(length);

  in.read(&(*data)[0], length);

  if (!in.good()) {
    LOG(ERROR) << "Error reading from file " << filename;
    return false;
  }
  return true;
}

bool WriteDataToFile(const std::vector<char>& data, const string& filename) {
  std::ofstream out(filename.c_str(), std::ios::binary);
  out.seekp(0, std::ios::beg);
  out.write(&data[0], data.size());
  return out.good();
}

bool RunCommandAndGetStdout(const string& command, std::vector<char>* output) {
  FILE* fp = popen(command.c_str(), "r");
  if (!fp)
    return false;

  output->clear();
  char buf[kFileReadSize];
  while (!feof(fp)) {
    size_t size_read = fread(buf, 1, sizeof(buf), fp);
    size_t prev_size = output->size();
    output->resize(prev_size + size_read);
    memcpy(&(*output)[prev_size], buf, size_read);
  }
  if (pclose(fp))
    return false;

  return true;
}

void TrimWhitespace(string* str) {
  const char kWhitespaceCharacters[] = " \t\n\r";
  size_t end = str->find_last_not_of(kWhitespaceCharacters);
  if (end != std::string::npos) {
    size_t start = str->find_first_not_of(kWhitespaceCharacters);
    *str = str->substr(start, end + 1 - start);
  } else {
    // The string contains only whitespace.
    *str = "";
  }
}

}  // namespace quipper
