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

#ifndef SIMPLE_PERF_RECORD_FILE_H_
#define SIMPLE_PERF_RECORD_FILE_H_

#include <stdio.h>
#include <memory>
#include <string>
#include <vector>

class EventAttr;
class EventFd;
class Record;

class RecordFile {
 public:
  static std::unique_ptr<RecordFile> CreateFile(const std::string& filename);

  ~RecordFile();

  RecordFile(const RecordFile&) = delete;
  RecordFile& operator=(const RecordFile&) = delete;

  // To keep the format of file content, Write* functions below should be called in a strict order.
  // 1. WriteHeader
  // 2. zero or more times WriteData
  // 3. features are optional, but if write, different types of features should be write in a strict
  //    order.
  //    3.1 WriteFeatureHeader
  //    3.2 WriteBuildIdFeature

  bool WriteHeader(const EventAttr& event_attr);

  bool WriteData(const void* buf, size_t len);

  bool WriteFeatureHeader(size_t max_feature_count);
  bool WriteBuildIdFeature(const std::vector<std::unique_ptr<Record>>& build_id_records);

  // ReadHitFiles() is used to know which files should dump build_id.
  bool ReadHitFiles(std::vector<std::string>& hit_kernel_modules,
                    std::vector<std::string>& hit_user_files);

  bool Close();

 private:
  RecordFile(const std::string& filename, FILE* fp);

  bool WriteOutput(const void* buf, size_t len);

  std::unique_ptr<Record> ReadRecord();

 private:
  const std::string filename;
  FILE* record_fp;

  std::unique_ptr<EventAttr> event_attr;

  uint64_t data_offset;
  uint64_t data_size;

  size_t max_feature_count;
  size_t current_feature_index;
  std::vector<int> features;
};

#endif  // SIMPLE_PERF_RECORD_FILE_H_
