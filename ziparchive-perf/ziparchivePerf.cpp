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

#include "benchmark/benchmark_api.h"
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

using namespace std;

static void SetZipString(ZipString* zip_str, const std::string& str) {
  zip_str->name = reinterpret_cast<const uint8_t*>(str.c_str());
  zip_str->name_length = str.size();
}

static TemporaryFile *createZip() {
  TemporaryFile *temp_file_ = new TemporaryFile();
  int fd_ = temp_file_->fd;
  FILE *file_ = fdopen(fd_, "w");

  ZipWriter writer(file_);
  std::string lastName = "file";
  for (int i = 0; i < 1000; i++) {
    // Make file names longer and longer.
    lastName = lastName + to_string(i++);
    writer.StartEntry(lastName.c_str(), ZipWriter::kCompress);
    writer.WriteBytes("helo", 4);
    writer.FinishEntry();
  }
  writer.Finish();
  fclose(file_);

  return temp_file_;
}

static void findIt(benchmark::State& state) {
  // Create a temporary zip archive.
  TemporaryFile *temp_file_ = createZip();
  ZipArchiveHandle handle;
  ZipEntry data;
  ZipString name;
  // In order to walk through all file names in the archive, look for a name
  // that does not exist in the archive.
  SetZipString(&name, "thisFileNameDoesNotExist");

  // Start the benchmark.
  while (state.KeepRunning()) {
    OpenArchive(temp_file_->path, &handle);
    FindEntry(handle, name, &data);
    CloseArchive(handle);
  }

  // Cleanup.
  delete temp_file_;
}
BENCHMARK(findIt);

static void iterateAll(benchmark::State& state) {
  TemporaryFile *temp_file_ = createZip();
  ZipArchiveHandle handle;
  void* iteration_cookie;
  ZipEntry data;
  ZipString name;

  while (state.KeepRunning()) {
    OpenArchive(temp_file_->path, &handle);
    StartIteration(handle, &iteration_cookie, nullptr, nullptr);
    while (0 == Next(iteration_cookie, &data, &name));
    CloseArchive(handle);
  }
  delete temp_file_;
}
BENCHMARK(iterateAll);

BENCHMARK_MAIN()
