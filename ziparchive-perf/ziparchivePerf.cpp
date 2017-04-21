/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "benchmark/benchmark.h"
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>
#include <tuple>

#include <ziparchive/zip_archive.h>
#include <ziparchive/zip_writer.h>
#include <ziparchive/zip_archive_stream_entry.h>
#include <android-base/test_utils.h>

static TemporaryFile* createZip() {
  TemporaryFile* temp_file_ = new TemporaryFile();
  int fd_ = temp_file_->fd;
  FILE* file_ = fdopen(fd_, "w");

  ZipWriter writer(file_);
  std::string lastName = "file";
  for (int i = 0; i < 1000; i++) {
    // Make file names longer and longer.
    lastName = lastName + std::to_string(i++);
    writer.StartEntry(lastName.c_str(), ZipWriter::kCompress);
    writer.WriteBytes("helo", 4);
    writer.FinishEntry();
  }
  writer.Finish();
  fclose(file_);

  return temp_file_;
}

static void FindEntry_no_match(benchmark::State& state) {
  // Create a temporary zip archive.
  std::unique_ptr<TemporaryFile> temp_file_(createZip());
  ZipArchiveHandle handle;
  ZipEntry data;
  ZipString name;
  // In order to walk through all file names in the archive, look for a name
  // that does not exist in the archive.
  const char* file_name = "thisFileNameDoesNotExist";
  name.name = (uint8_t *) file_name;
  name.name_length = strlen(file_name);

  // Start the benchmark.
  while (state.KeepRunning()) {
    OpenArchive(temp_file_->path, &handle);
    FindEntry(handle, name, &data);
    CloseArchive(handle);
  }
}
BENCHMARK(FindEntry_no_match);

static void Iterate_all_files(benchmark::State& state) {
  std::unique_ptr<TemporaryFile> temp_file_(createZip());
  ZipArchiveHandle handle;
  void* iteration_cookie;
  ZipEntry data;
  ZipString name;

  while (state.KeepRunning()) {
    OpenArchive(temp_file_->path, &handle);
    StartIteration(handle, &iteration_cookie, nullptr, nullptr);
    while (0 == Next(iteration_cookie, &data, &name)) {}
    EndIteration(iteration_cookie);
    CloseArchive(handle);
  }
}
BENCHMARK(Iterate_all_files);

BENCHMARK_MAIN()
