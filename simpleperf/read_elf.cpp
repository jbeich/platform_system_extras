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

#include "read_elf.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <base/file.h>
#include <base/logging.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <llvm/ADT/StringRef.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>

#pragma clang diagnostic pop

#include <elf.h>

#include "utils.h"

using namespace llvm;
using namespace llvm::object;

static bool GetBuildIdFromNoteSection(const char* section, size_t section_size, BuildId* build_id) {
  const char* p = section;
  const char* end = p + section_size;
  while (p < end) {
    size_t namesz = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    size_t descsz = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    uint32_t type = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    namesz = ALIGN(namesz, 4);
    descsz = ALIGN(descsz, 4);
    if ((type == NT_GNU_BUILD_ID) && (strcmp(p, ELF_NOTE_GNU) == 0)) {
      std::fill(build_id->begin(), build_id->end(), 0);
      memcpy(build_id->data(), p + namesz, std::min(build_id->size(), descsz));
      return true;
    }
    p += namesz + descsz;
  }
  return false;
}

bool GetBuildIdFromNoteFile(const std::string& filename, BuildId* build_id) {
  std::string content;
  if (!android::base::ReadFileToString(filename, &content)) {
    LOG(DEBUG) << "can't read note file " << filename;
    return false;
  }
  if (GetBuildIdFromNoteSection(content.c_str(), content.size(), build_id) == false) {
    LOG(DEBUG) << "can't read build_id from note file " << filename;
    return false;
  }
  return true;
}

template <class ELFT>
bool GetBuildIdFromELFFile(const ELFFile<ELFT>* elf, BuildId* build_id) {
  for (auto it = elf->begin_sections(); it != elf->end_sections(); ++it) {
    auto section = &*it;
    if (section->sh_type == SHT_NOTE) {
      auto err_or_data = elf->getSectionContents(section);
      if (err_or_data.getError()) {
        LOG(DEBUG) << "read note section error";
        continue;
      }
      ArrayRef<uint8_t> data = *err_or_data;
      if (GetBuildIdFromNoteSection(reinterpret_cast<const char*>(data.data()), data.size(),
                                    build_id)) {
        return true;
      }
    }
  }
  return false;
}

bool GetBuildIdFromElfFile(const std::string& filename, BuildId* build_id) {
  auto err_or_binary = createBinary(StringRef(filename));
  if (err_or_binary.getError()) {
    PLOG(DEBUG) << "can't open file " << filename;
    return false;
  }
  bool result = false;
  Binary& binary = *err_or_binary.get().getBinary();
  if (ObjectFile* obj = dyn_cast<ObjectFile>(&binary)) {
    if (auto* elf = dyn_cast<ELF32LEObjectFile>(obj)) {
      result = GetBuildIdFromELFFile(elf->getELFFile(), build_id);
    } else if (auto* elf = dyn_cast<ELF32BEObjectFile>(obj)) {
      result = GetBuildIdFromELFFile(elf->getELFFile(), build_id);
    } else if (auto* elf = dyn_cast<ELF64LEObjectFile>(obj)) {
      result = GetBuildIdFromELFFile(elf->getELFFile(), build_id);
    } else if (auto* elf = dyn_cast<ELF64BEObjectFile>(obj)) {
      result = GetBuildIdFromELFFile(elf->getELFFile(), build_id);
    } else {
      PLOG(DEBUG) << "unknown elf format in file " << filename;
    }
  }
  if (!result) {
    PLOG(DEBUG) << "can't read build_id from file " << filename;
  }
  return result;
}
