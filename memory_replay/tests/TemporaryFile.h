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

#ifndef _MEMORY_REPLAY_TEST_TEMP_FILE_H
#define _MEMORY_REPLAY_TEST_TEMP_FILE_H

#include <stdlib.h>

#include <string>

class TemporaryFile {
 public:
  TemporaryFile() {
    char* tmpdir = getenv("TMPDIR");
    if (tmpdir != nullptr) {
      filename_ = tmpdir;
    } else {
#if defined(__BIONIC__)
      filename_ = "/data/local/tmp";
#else
      filename_ = "/tmp";
#endif
    }
    filename_ += "/MEMORY_REPLAY_XXXXXXXX";
    fd_ = mkstemp(&filename_[0]);
  }

  virtual ~TemporaryFile() {
    if (fd_ != -1) {
      close(fd_);
      unlink(filename_.c_str());
    }
  }

  int fd() { return fd_; }

 private:
  int fd_ = -1;
  std::string filename_;
};

#endif // _MEMORY_REPLAY_TEST_TEMP_FILE_H
