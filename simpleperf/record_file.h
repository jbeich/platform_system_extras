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
