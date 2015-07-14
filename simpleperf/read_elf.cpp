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

#include "utils.h"

#define ELF_NOTE_GNU "GNU"
#define NT_GNU_BUILD_ID 3

static bool GetBuildIdFromNoteSection(const char* section, size_t section_size, BuildId* build_id) {
  const char* p = section;
  const char* end = p + section_size;
  while (p < end) {
    CHECK_LE(p + 12, end);
    size_t namesz = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    size_t descsz = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    uint32_t type = *reinterpret_cast<const uint32_t*>(p);
    p += 4;
    namesz = ALIGN(namesz, 4);
    descsz = ALIGN(descsz, 4);
    CHECK_LE(p + namesz + descsz, end);
    if ((type == NT_GNU_BUILD_ID) && (strcmp(p, ELF_NOTE_GNU) == 0)) {
      *build_id = BuildId(p + namesz, descsz);
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
bool GetBuildIdFromELFFile(const llvm::object::ELFFile<ELFT>* elf, BuildId* build_id) {
  for (auto section_iterator = elf->begin_sections(); section_iterator != elf->end_sections();
       ++section_iterator) {
    if (section_iterator->sh_type == llvm::ELF::SHT_NOTE) {
      auto contents = elf->getSectionContents(&*section_iterator);
      if (contents.getError()) {
        LOG(DEBUG) << "read note section error";
        continue;
      }
      if (GetBuildIdFromNoteSection(reinterpret_cast<const char*>(contents->data()),
                                    contents->size(), build_id)) {
        return true;
      }
    }
  }
  return false;
}

static bool GetBuildIdFromObjectFile(llvm::object::ObjectFile* obj, BuildId* build_id) {
  bool result = false;
  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(obj)) {
    result = GetBuildIdFromELFFile(elf->getELFFile(), build_id);
  } else if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(obj)) {
    result = GetBuildIdFromELFFile(elf->getELFFile(), build_id);
  } else {
    LOG(ERROR) << "unknown elf format in file " << obj->getFileName().data();
    return false;
  }
  if (!result) {
    LOG(DEBUG) << "no build id present in file " << obj->getFileName().data();
  }
  return result;
}

bool GetBuildIdFromElfFile(const std::string& filename, BuildId* build_id) {
  auto owning_binary = llvm::object::createBinary(llvm::StringRef(filename));
  if (owning_binary.getError()) {
    PLOG(DEBUG) << "can't open file " << filename;
    return false;
  }
  llvm::object::Binary* binary = owning_binary.get().getBinary();
  llvm::object::ObjectFile* obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary);
  if (obj == nullptr) {
    LOG(DEBUG) << filename << " is not an object file.";
    return false;
  }
  return GetBuildIdFromObjectFile(obj, build_id);
}

bool IsArmMappingSymbol(const char* name) {
  // Mapping symbols in arm, which are described in "ELF for ARM Architecture" and
  // "ELF for ARM 64-bit Architecture". The regular expression to match mapping symbol
  // is ^\$(a|d|t|x)(\..*)?$
  return name[0] == '$' && strchr("adtx", name[1]) != nullptr && (name[2] == '\0' || name[2] == '.');
}

template <class ELFT>
void ParseSymbolsFromELFFile(const llvm::object::ELFFile<ELFT>* elf,
                             std::function<void(const ElfFileSymbol&)> callback) {
  bool is_arm = (elf->getHeader()->e_machine == llvm::ELF::EM_ARM ||
                 elf->getHeader()->e_machine == llvm::ELF::EM_AARCH64);
  auto begin = elf->begin_symbols();
  auto end = elf->end_symbols();
  if (begin == end) {
    begin = elf->begin_dynamic_symbols();
    end = elf->end_dynamic_symbols();
  }
  for (; begin != end; ++begin) {
    auto& elf_symbol = *begin;

    ElfFileSymbol symbol;
    memset(&symbol, '\0', sizeof(symbol));

    auto shdr = elf->getSection(&elf_symbol);
    if (shdr == nullptr) {
      continue;
    }
    auto section_name = elf->getSectionName(shdr);
    if (section_name.getError() || section_name.get().empty()) {
      continue;
    }
    if (section_name.get() == ".text") {
      symbol.is_in_text_section = true;
    }

    auto symbol_name = elf->getSymbolName(begin);
    if (symbol_name.getError()) {
      continue;
    }
    symbol.name = symbol_name.get();
    if (symbol.name.empty()) {
      continue;
    }
    symbol.vaddr = elf_symbol.st_value;
    symbol.start_in_file = elf_symbol.st_value - shdr->sh_addr + shdr->sh_offset;
    if ((symbol.start_in_file & 1) != 0 && is_arm) {
      // Arm sets bit 0 to mark it as thumb code, remove the flag.
      symbol.start_in_file &= ~1;
    }
    symbol.len = elf_symbol.st_size;
    int type = elf_symbol.getType();
    if (type == llvm::ELF::STT_FUNC) {
      symbol.is_func = true;
    } else if (type == llvm::ELF::STT_NOTYPE) {
      if (symbol.is_in_text_section) {
        symbol.is_label = true;
        if (is_arm) {
          // Remove mapping symbols in arm.
          const char* p = (symbol.name.compare(0, linker_prefix.size(), linker_prefix) == 0)
                              ? symbol.name.c_str() + linker_prefix.size()
                              : symbol.name.c_str();
          if (IsArmMappingSymbol(p)) {
            symbol.is_label = false;
          }
        }
      }
    }

    callback(symbol);
  }
}

bool ParseSymbolsFromElfFile(const std::string& filename, const BuildId& expected_build_id,
                             std::function<void(const ElfFileSymbol&)> callback) {
  auto owning_binary = llvm::object::createBinary(llvm::StringRef(filename));
  if (owning_binary.getError()) {
    PLOG(DEBUG) << "can't open file " << filename;
    return false;
  }
  llvm::object::Binary* binary = owning_binary.get().getBinary();
  llvm::object::ObjectFile* obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary);
  if (obj == nullptr) {
    LOG(DEBUG) << filename << " is not an object file.";
    return false;
  }
  BuildId real_build_id;
  GetBuildIdFromObjectFile(obj, &real_build_id);
  bool result = (expected_build_id == real_build_id);
  LOG(DEBUG) << "check build id for \"" << filename << "\" (" << (result ? "match" : "mismatch")
             << "): expected " << expected_build_id.ToString() << ", real "
             << real_build_id.ToString();
  if (!result) {
    return result;
  }

  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(obj)) {
    ParseSymbolsFromELFFile(elf->getELFFile(), callback);
  } else if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(obj)) {
    ParseSymbolsFromELFFile(elf->getELFFile(), callback);
  } else {
    LOG(ERROR) << "unknown elf format in file" << filename;
    return false;
  }
  return true;
}

template <class ELFT>
bool ReadSectionsFromELFFile(const llvm::object::ELFFile<ELFT>* elf,
                             std::vector<ElfFileSection>* sections) {
  for (auto it = elf->begin_sections(); it != elf->end_sections(); ++it) {
    llvm::ErrorOr<llvm::StringRef> name = elf->getSectionName(&*it);
    if (name) {
      for (auto& section : *sections) {
        if (name.get() == section.name) {
          llvm::ErrorOr<llvm::ArrayRef<uint8_t>> data = elf->getSectionContents(&*it);
          if (data) {
            section.data.clear();
            section.data.insert(section.data.begin(), data->data(), data->data() + data->size());
            section.offset = it->sh_offset;
          } else {
            LOG(ERROR) << "can't read section " << section.name;
            return false;
          }
          break;
        }
      }
    }
  }
  return true;
}

bool ReadSectionsFromElfFileByName(const std::string& filename,
                                   std::vector<ElfFileSection>* sections) {
  auto owning_binary = llvm::object::createBinary(llvm::StringRef(filename));
  if (owning_binary.getError()) {
    PLOG(DEBUG) << "can't open file " << filename;
    return false;
  }
  llvm::object::Binary* binary = owning_binary.get().getBinary();
  llvm::object::ObjectFile* obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary);
  if (obj == nullptr) {
    LOG(DEBUG) << filename << " is not an object file.";
    return false;
  }
  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(obj)) {
    return ReadSectionsFromELFFile(elf->getELFFile(), sections);
  }
  if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(obj)) {
    return ReadSectionsFromELFFile(elf->getELFFile(), sections);
  }
  LOG(ERROR) << "unknown elf format in file " << filename;
  return false;
}
