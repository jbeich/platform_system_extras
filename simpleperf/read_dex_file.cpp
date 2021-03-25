/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "read_dex_file.h"

#include <fcntl.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <art_api/dex_file_support.h>

namespace simpleperf {

static bool ReadSymbols(
    const std::vector<uint64_t>& dex_file_offsets,
    const std::function<std::unique_ptr<art_api::dex::DexFile>(uint64_t offset)>& open_file_cb,
    const std::function<void(DexFileSymbol*)>& symbol_cb) {
  for (uint64_t offset : dex_file_offsets) {
    std::unique_ptr<art_api::dex::DexFile> dex_file = open_file_cb(offset);
    if (dex_file == nullptr) {
      return false;
    }

    auto callback = [&](DexFileSymbol* symbol) {
      symbol->addr += offset;
      symbol_cb(symbol);
    };
    dex_file->GetAllMethodInfos(callback);
  }

  return true;
}

bool ReadSymbolsFromDexFileInMemory(void* addr, uint64_t size,
                                    const std::vector<uint64_t>& dex_file_offsets,
                                    const std::function<void(DexFileSymbol*)>& symbol_callback) {
  return ReadSymbols(
      dex_file_offsets,
      [&](uint64_t offset) -> std::unique_ptr<art_api::dex::DexFile> {
        size_t max_file_size;
        if (__builtin_sub_overflow(size, offset, &max_file_size)) {
          return nullptr;
        }
        uint8_t* file_addr = static_cast<uint8_t*>(addr) + offset;
        std::string error_msg;
        std::unique_ptr<art_api::dex::DexFile> dex_file =
            art_api::dex::DexFile::OpenFromMemory(file_addr, &max_file_size, "", &error_msg);
        if (dex_file == nullptr) {
          LOG(WARNING) << "Failed to read dex file symbols: " << error_msg;
          return nullptr;
        }
        return dex_file;
      },
      symbol_callback);
}

bool ReadSymbolsFromDexFile(const std::string& file_path,
                            const std::vector<uint64_t>& dex_file_offsets,
                            const std::function<void(DexFileSymbol*)>& symbol_callback) {
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(file_path.c_str(), O_RDONLY | O_CLOEXEC)));
  if (fd == -1) {
    return false;
  }
  return ReadSymbols(
      dex_file_offsets,
      [&](uint64_t offset) -> std::unique_ptr<art_api::dex::DexFile> {
        std::string error_msg;
        std::unique_ptr<art_api::dex::DexFile> dex_file =
            art_api::dex::DexFile::OpenFromFd(fd, offset, file_path, &error_msg);
        if (dex_file == nullptr) {
          LOG(WARNING) << "Failed to read dex file symbols from '" << file_path
                       << "': " << error_msg;
          return nullptr;
        }
        return dex_file;
      },
      symbol_callback);
}

}  // namespace simpleperf
