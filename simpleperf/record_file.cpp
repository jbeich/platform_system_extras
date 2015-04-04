/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "record_file.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>

#include "event.h"
#include "event_attr.h"
#include "event_fd.h"
#include "record.h"
#include "record_file_format.h"

std::unique_ptr<RecordFile> RecordFile::CreateFile(const std::string& filename) {
  unlink(filename.c_str());

  int fd = open(filename.c_str(), O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);
  if (fd == -1) {
    fprintf(stderr, "open() record file %s failed: %s\n", filename.c_str(), strerror(errno));
    return std::unique_ptr<RecordFile>(nullptr);
  }
  FILE* fp = fdopen(fd, "wb+");
  if (fp == nullptr) {
    fprintf(stderr, "fdopen() record file %s failed: %s\n", filename.c_str(), strerror(errno));
    return std::unique_ptr<RecordFile>(nullptr);
  }
  return std::unique_ptr<RecordFile>(new RecordFile(filename, fp));
}

RecordFile::RecordFile(const std::string& filename, FILE* fp)
  	: filename(filename), record_fp(fp), data_offset(0), data_size(0),
     	max_feature_count(0), current_feature_index(0) {
}

RecordFile::~RecordFile() {
  if (record_fp != nullptr) {
    Close();
  }
}

bool RecordFile::WriteHeader(const EventAttr& event_attr) {
  fseek(record_fp, sizeof(file_header), SEEK_SET);

  // TODO: Add id section when necessary.
  long ids_offset = ftell(record_fp);

  std::vector<file_attr> file_attrs;

  uint64_t id_offset = ids_offset;
  file_attr attr;
  attr.attr = *(event_attr.Attr());
  attr.ids.offset = id_offset;
  attr.ids.size = 0;
  file_attrs.push_back(attr);

  this->event_attr = std::unique_ptr<EventAttr>(new EventAttr(event_attr));

  long attrs_offset = ftell(record_fp);
  size_t attrs_size = file_attrs.size() * sizeof(file_attr);
  if (!WriteOutput(file_attrs.data(), attrs_size)) {
    return false;
  }

  data_offset = ftell(record_fp);

  file_header header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, PERF_MAGIC, sizeof(header.magic));
  header.header_size = sizeof(header);
  header.attr_size = sizeof(file_attr);
  header.attrs.offset = attrs_offset;
  header.attrs.size = attrs_size;
  header.data.offset = data_offset;
  header.data.size = data_size;
  for (auto feature : features) {
    int i = feature / 8;
    int j = feature % 8;
    header.adds_features[i] |= (1 << j);
  }

  if (fseek(record_fp, 0, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }
  if (!WriteOutput(&header, sizeof(header))) {
    return false;
  }
  fseek(record_fp, data_offset, SEEK_SET);

  return true;
}

bool RecordFile::WriteData(const void* buf, size_t len) {
  if (!WriteOutput(buf, len)) {
    return false;
  }
  data_size += len;
  return true;
}

bool RecordFile::WriteFeatureHeader(size_t max_feature_count) {
  this->max_feature_count = max_feature_count;
  current_feature_index = 0;
  uint64_t feature_header_size = max_feature_count * sizeof(file_section);

  std::vector<unsigned char> zero_data(feature_header_size);
  if (fseek(record_fp, data_offset + data_size, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }
  if (fwrite(zero_data.data(), feature_header_size, 1, record_fp) != 1) {
    perror("fwrite");
    return false;
  }
  return true;
}

bool RecordFile::WriteBuildIdFeature(const std::vector<std::unique_ptr<Record>>& build_id_records) {
  if (current_feature_index + 1 > max_feature_count) {
    return false;
  }
  // Always write features at the end of the file.
  if (fseek(record_fp, 0, SEEK_END) != 0) {
    perror("fseek");
    return false;
  }
  long section_start = ftell(record_fp);
  if (section_start == -1) {
    perror("ftell");
    return false;
  }
  for (auto& record : build_id_records) {
    if (fwrite(record->GetBuf(), record->GetBufSize(), 1, record_fp) != 1) {
      perror("fwrite");
      return false;
    }
  }
  long section_end = ftell(record_fp);
  if (section_end == -1) {
    perror("ftell");
    return false;
  }

  file_section section;
  section.offset = section_start;
  section.size = section_end - section_start;
  uint64_t feature_offset = data_offset + data_size;
  if (fseek(record_fp, feature_offset + current_feature_index * sizeof(file_section), SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }
  if (fwrite(&section, sizeof(section), 1, record_fp) != 1) {
    perror("fwrite");
    return false;
  }
  if (fseek(record_fp, section_end, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }
  current_feature_index += 1;
  features.push_back(FEAT_BUILD_ID);
  return true;
}

bool RecordFile::WriteOutput(const void* buf, size_t len) {
  if (fwrite(buf, len, 1, record_fp) != 1) {
    fprintf(stderr, "RecordFile::WriteData for file %s failed: %s\n", filename.c_str(),
            strerror(errno));
    return false;
  }
  return true;
}

bool RecordFile::ReadHitFiles(std::vector<std::string>& hit_kernel_modules,
                              std::vector<std::string>& hit_user_files) {
  if (fseek(record_fp, data_offset, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }

  std::vector<std::unique_ptr<Record>> kernel_mmap;
  std::vector<std::unique_ptr<Record>> user_mmap;
  std::set<std::string> hit_kernel_set;
  std::set<std::string> hit_user_set;

  size_t last_data_size = data_size;
  while (last_data_size != 0) {
    auto record = ReadRecord();
    if (record == nullptr) {
      return false;
    }
    last_data_size -= record->GetBufSize();

    if (record->Type() == PERF_RECORD_MMAP) {
      if (record->InKernel()) {
        kernel_mmap.push_back(std::move(record));
      } else {
        user_mmap.push_back(std::move(record));
      }
    } else if (record->Type() == PERF_RECORD_SAMPLE) {
      RecordSample* sample = static_cast<RecordSample*>(record.get());

      if (sample->InKernel()) {
        // Loop from back to front, because new RecordMmap is inserted at the end of kernel_mmap,
        // and we always want to match the newest one.
        for (auto it = kernel_mmap.rbegin(); it != kernel_mmap.rend(); ++it) {
          RecordMmap* mmap = static_cast<RecordMmap*>(it->get());
          if (sample->Ip() >= mmap->Addr() && sample->Ip() <= mmap->Addr() + mmap->Len()) {
            hit_kernel_set.insert(mmap->Filename());
          }
        }
      } else {
        for (auto it = user_mmap.rbegin(); it != user_mmap.rend(); ++it) {
          RecordMmap* mmap = static_cast<RecordMmap*>(it->get());
          if (sample->Pid() == mmap->Pid() && sample->Ip() >= mmap->Addr() &&
              sample->Ip() <= mmap->Addr() + mmap->Len()) {
            hit_user_set.insert(mmap->Filename());
          }
        }
      }
    }
  }

  hit_kernel_modules.clear();
  hit_kernel_modules.insert(hit_kernel_modules.begin(), hit_kernel_set.begin(), hit_kernel_set.end());
  hit_user_files.clear();
  hit_user_files.insert(hit_user_files.begin(), hit_user_set.begin(), hit_user_set.end());
  return true;
}

std::unique_ptr<Record> RecordFile::ReadRecord() {
  perf_event_header record_header;
  if (fread(&record_header, sizeof(record_header), 1, record_fp) != 1) {
    perror("fread");
    return std::unique_ptr<Record>(nullptr);
  }
  size_t record_size = record_header.size;
  char* buf = new char[record_size];
  memcpy(buf, &record_header, sizeof(record_header));
  if (fread(buf + sizeof(record_header), record_size - sizeof(record_header), 1, record_fp) != 1) {
    perror("fread");
    delete [] buf;
    return std::unique_ptr<Record>(nullptr);
  }
  return BuildRecordOnBuffer(buf, record_size, event_attr.get());
}

bool RecordFile::Close() {
  if (record_fp == nullptr) {
    return true;
  }

  if (fclose(record_fp) != 0) {
    fprintf(stderr, "RecordFile::Close for file %s failed: %s\n", filename.c_str(),
            strerror(errno));
    return false;
  }
  record_fp = nullptr;
  return true;
}
