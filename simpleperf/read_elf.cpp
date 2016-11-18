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
#include "read_apk.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <limits>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <elf_reader/elf_reader.h>

using namespace android::libelf_reader;

#include "utils.h"

#define ELF_NOTE_GNU "GNU"
#define NT_GNU_BUILD_ID 3

std::ostream& operator<<(std::ostream& os, const ElfStatus& status) {
  switch (status) {
    case ElfStatus::NO_ERROR:
      os << "No error";
      break;
    case ElfStatus::FILE_NOT_FOUND:
      os << "File not found";
      break;
    case ElfStatus::READ_FAILED:
      os << "Read failed";
      break;
    case ElfStatus::FILE_MALFORMED:
      os << "Malformed file";
      break;
    case ElfStatus::NO_SYMBOL_TABLE:
      os << "No symbol table";
      break;
    case ElfStatus::NO_BUILD_ID:
      os << "No build id";
      break;
    case ElfStatus::BUILD_ID_MISMATCH:
      os << "Build id mismatch";
      break;
    case ElfStatus::SECTION_NOT_FOUND:
      os << "Section not found";
      break;
  }
  return os;
}

ElfStatus IsValidElfFile(int fd) {
  static const char elf_magic[] = {0x7f, 'E', 'L', 'F'};
  char buf[4];
  if (!android::base::ReadFully(fd, buf, 4)) {
    return ElfStatus::READ_FAILED;
  }
  if (memcmp(buf, elf_magic, 4) != 0) {
    return ElfStatus::FILE_MALFORMED;
  }
  return ElfStatus::NO_ERROR;
}

ElfStatus IsValidElfPath(const std::string& filename) {
  if (!IsRegularFile(filename)) {
    return ElfStatus::FILE_NOT_FOUND;
  }
  std::string mode = std::string("rb") + CLOSE_ON_EXEC_MODE;
  FILE* fp = fopen(filename.c_str(), mode.c_str());
  if (fp == nullptr) {
    return ElfStatus::READ_FAILED;
  }
  ElfStatus result = IsValidElfFile(fileno(fp));
  fclose(fp);
  return result;
}

bool GetBuildIdFromNoteSection(const char* section, size_t section_size, BuildId* build_id) {
  const char* p = section;
  const char* end = p + section_size;
  while (p < end) {
    if (p + 12 >= end) {
      return false;
    }
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
    MoveFromBinaryFormat(namesz, p);
    MoveFromBinaryFormat(descsz, p);
    MoveFromBinaryFormat(type, p);
    namesz = Align(namesz, 4);
    descsz = Align(descsz, 4);
    if ((type == NT_GNU_BUILD_ID) && (p < end) && (strcmp(p, ELF_NOTE_GNU) == 0)) {
      const char* desc_start = p + namesz;
      const char* desc_end = desc_start + descsz;
      if (desc_start > p && desc_start < desc_end && desc_end <= end) {
        *build_id = BuildId(p + namesz, descsz);
        return true;
      } else {
        return false;
      }
    }
    p += namesz + descsz;
  }
  return false;
}

ElfStatus GetBuildIdFromNoteFile(const std::string& filename, BuildId* build_id) {
  std::string content;
  if (!android::base::ReadFileToString(filename, &content)) {
    return ElfStatus::READ_FAILED;
  }
  if (!GetBuildIdFromNoteSection(content.c_str(), content.size(), build_id)) {
    return ElfStatus::NO_BUILD_ID;
  }
  return ElfStatus::NO_ERROR;
}

template <typename ElfTypes>
ElfStatus GetBuildIdFromELFFile(ElfReaderImpl<ElfTypes>* elf, BuildId* build_id) {
  auto sec_headers = elf->ReadSectionHeaders();
  if (sec_headers == nullptr) {
    return ElfStatus::NO_BUILD_ID;
  }
  for (auto& sec : *sec_headers) {
    if (sec.sh_type == SHT_NOTE) {
      SectionData data = elf->ReadSectionData(sec);
      if (data.data != nullptr) {
        if (GetBuildIdFromNoteSection(data.data, data.size, build_id)) {
          return ElfStatus::NO_ERROR;
        }
      }
    }
  }
  return ElfStatus::NO_BUILD_ID;
}

ElfStatus GetBuildIdFromObjectFile(ElfReader* elf, BuildId* build_id) {
  if (elf->GetImpl32() != nullptr) {
    return GetBuildIdFromELFFile(elf->GetImpl32(), build_id);
  } else if (elf->GetImpl64() != nullptr) {
    return GetBuildIdFromELFFile(elf->GetImpl64(), build_id);
  }
  return ElfStatus::FILE_MALFORMED;
}

static ElfStatus OpenObjectFile(const std::string& filename, uint64_t file_offset,
                                uint64_t /*file_size*/, std::unique_ptr<ElfReader>* wrapper) {
  std::string error_msg;
  *wrapper = ElfReader::OpenFile(filename.c_str(), file_offset, &error_msg);
  if (*wrapper == nullptr) {
    LOG(DEBUG) << error_msg;
    return ElfStatus::READ_FAILED;
  }
  return ElfStatus::NO_ERROR;
}

static ElfStatus OpenObjectFileFromString(const std::string& s, std::unique_ptr<ElfReader>* wrapper) {
  std::string error_msg;
  *wrapper = ElfReader::OpenMem(&s[0], s.size(), "", &error_msg);
  if (*wrapper == nullptr) {
    LOG(DEBUG) << error_msg;
    return ElfStatus::FILE_MALFORMED;
  }
  return ElfStatus::NO_ERROR;
}

ElfStatus GetBuildIdFromElfFile(const std::string& filename, BuildId* build_id) {
  return GetBuildIdFromEmbeddedElfFile(filename, 0, 0, build_id);
}

ElfStatus GetBuildIdFromEmbeddedElfFile(const std::string& filename, uint64_t file_offset,
                                        uint32_t file_size, BuildId* build_id) {
  std::unique_ptr<ElfReader> wrapper;
  ElfStatus result = OpenObjectFile(filename, file_offset, file_size, &wrapper);
  if (result != ElfStatus::NO_ERROR) {
    return result;
  }
  return GetBuildIdFromObjectFile(wrapper.get(), build_id);
}

bool IsArmMappingSymbol(const char* name) {
  // Mapping symbols in arm, which are described in "ELF for ARM Architecture" and
  // "ELF for ARM 64-bit Architecture". The regular expression to match mapping symbol
  // is ^\$(a|d|t|x)(\..*)?$
  return name[0] == '$' && strchr("adtx", name[1]) != nullptr && (name[2] == '\0' || name[2] == '.');
}

template <typename ElfTypes>
void ReadSymbolTable(ElfReaderImpl<ElfTypes>* elf,
                     const typename ElfTypes::Shdr* sym_sec,
                     const typename ElfTypes::Shdr* str_sec,
                     int text_sec_index,
                     const std::function<void(const ElfFileSymbol&)>& callback,
                     bool is_arm) {
  using Sym = typename ElfTypes::Sym;
  SectionData sym_data = elf->ReadSectionData(*sym_sec);
  SectionData str_data = elf->ReadSectionData(*str_sec);
  if (sym_data.data == nullptr || str_data.data == nullptr) {
    return;
  }
  for (const char* p = sym_data.data; p < sym_data.data + sym_data.size;
      p += sizeof(Sym)) {
    ElfFileSymbol symbol;
    const Sym* sym = reinterpret_cast<const Sym*>(p);
    if (sym->st_shndx == text_sec_index) {
      symbol.is_in_text_section = true;
    }
    if (sym->st_name == STN_UNDEF) {
      continue;
    }
    symbol.name = &str_data.data[sym->st_name];
    symbol.vaddr = sym->st_value;
    if ((symbol.vaddr & 1) != 0 && is_arm) {
      // Arm sets bit 0 to mark it as thumb code, remove the flag.
      symbol.vaddr &= ~1;
    }
    symbol.len = sym->st_size;
    if (sym->getType() == STT_FUNC) {
      symbol.is_func = true;
    } else if (sym->getType() == STT_NOTYPE) {
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

template <typename Elf_Shdr>
void AddSymbolForPltSection(Elf_Shdr* plt_sec,
                            const std::function<void(const ElfFileSymbol&)>& callback) {
  // We may sample instructions in .plt section if the program
  // calls functions from shared libraries. Different architectures use
  // different formats to store .plt section, so it needs a lot of work to match
  // instructions in .plt section to symbols. As samples in .plt section rarely
  // happen, and .plt section can hardly be a performance bottleneck, we can
  // just use a symbol @plt to represent instructions in .plt section.
  ElfFileSymbol symbol;
  symbol.vaddr = plt_sec->sh_addr;
  symbol.len = plt_sec->sh_size;
  symbol.is_func = true;
  symbol.is_label = true;
  symbol.is_in_text_section = true;
  symbol.name = "@plt";
  callback(symbol);
}

template <typename ElfTypes>
ElfStatus ParseSymbolsFromELFFile(ElfReaderImpl<ElfTypes>* elf,
                                  const std::function<void(const ElfFileSymbol&)>& callback) {
  using Elf_Shdr = typename ElfTypes::Shdr;
  const Elf_Shdr* symtab_sec = nullptr;
  const Elf_Shdr* strtab_sec = nullptr;
  const Elf_Shdr* dynsym_sec = nullptr;
  const Elf_Shdr* dynstr_sec = nullptr;
  const Elf_Shdr* plt_sec = nullptr;
  const Elf_Shdr* gnu_debugdata_sec = nullptr;
  int text_sec_index = -1;

  auto header = elf->ReadHeader();
  if (header == nullptr) {
    return ElfStatus::READ_FAILED;
  }
  auto machine = header->e_machine;
  bool is_arm = (machine == EM_ARM || machine == EM_AARCH64);

  auto sec_headers = elf->ReadSectionHeaders();
  if (sec_headers == nullptr) {
    return ElfStatus::READ_FAILED;
  }
  for (size_t i = 0; i < sec_headers->size(); ++i) {
    auto& sec = (*sec_headers)[i];
    const char* name = elf->GetSectionName(sec);
    if (strcmp(name, ".text") == 0) {
      text_sec_index = static_cast<int>(i);
    } else if (strcmp(name, ".symtab") == 0) {
      symtab_sec = &sec;
    } else if (strcmp(name, ".strtab") == 0) {
      strtab_sec = &sec;
    } else if (strcmp(name, ".dynsym") == 0) {
      dynsym_sec = &sec;
    } else if (strcmp(name, ".dynstr") == 0) {
      dynstr_sec = &sec;
    } else if (strcmp(name, ".plt") == 0) {
      plt_sec = &sec;
    } else if (strcmp(name, ".gnu_debugdata") == 0) {
      gnu_debugdata_sec = &sec;
    }
  }
  if (plt_sec != nullptr) {
    AddSymbolForPltSection(plt_sec, callback);
  }
  if (symtab_sec != nullptr && strtab_sec != nullptr) {
    ReadSymbolTable(elf, symtab_sec, strtab_sec, text_sec_index, callback, is_arm);
    return ElfStatus::NO_ERROR;
  }
  if (dynsym_sec != nullptr && dynstr_sec != nullptr) {
    ReadSymbolTable(elf, dynsym_sec, dynstr_sec, text_sec_index, callback, is_arm);
  }
  if (gnu_debugdata_sec == nullptr) {
    return ElfStatus::NO_SYMBOL_TABLE;
  }
  std::string debugdata;
  if (elf->ReadSectionData(*gnu_debugdata_sec, &debugdata)) {
    std::string decompressed_data;
    if (XzDecompress(debugdata, &decompressed_data)) {
      std::unique_ptr<ElfReader> wrapper;
      ElfStatus result = OpenObjectFileFromString(decompressed_data, &wrapper);
      if (result == ElfStatus::NO_ERROR) {
        if (wrapper->GetImpl32()) {
          return ParseSymbolsFromELFFile(wrapper->GetImpl32(), callback);
        } else if (wrapper->GetImpl64()) {
          return ParseSymbolsFromELFFile(wrapper->GetImpl64(), callback);
        } else {
          return ElfStatus::FILE_MALFORMED;
        }
      }
    }
  }
  return ElfStatus::NO_ERROR;
}

ElfStatus MatchBuildId(ElfReader* elf, const BuildId& expected_build_id) {
  if (expected_build_id.IsEmpty()) {
    return ElfStatus::NO_ERROR;
  }
  BuildId real_build_id;
  ElfStatus result = GetBuildIdFromObjectFile(elf, &real_build_id);
  if (result != ElfStatus::NO_ERROR) {
    return result;
  }
  if (expected_build_id != real_build_id) {
    return ElfStatus::BUILD_ID_MISMATCH;
  }
  return ElfStatus::NO_ERROR;
}

ElfStatus ParseSymbolsFromElfFile(const std::string& filename,
                                  const BuildId& expected_build_id,
                                  const std::function<void(const ElfFileSymbol&)>& callback) {
  return ParseSymbolsFromEmbeddedElfFile(filename, 0, 0, expected_build_id, callback);
}

ElfStatus ParseSymbolsFromEmbeddedElfFile(const std::string& filename, uint64_t file_offset,
                                          uint32_t file_size, const BuildId& expected_build_id,
                                          const std::function<void(const ElfFileSymbol&)>& callback) {
  std::unique_ptr<ElfReader> wrapper;
  ElfStatus result = OpenObjectFile(filename, file_offset, file_size, &wrapper);
  if (result != ElfStatus::NO_ERROR) {
    return result;
  }
  result = MatchBuildId(wrapper.get(), expected_build_id);
  if (result != ElfStatus::NO_ERROR) {
    return result;
  }
  if (wrapper->GetImpl32()) {
    return ParseSymbolsFromELFFile(wrapper->GetImpl32(), callback);
  } else if (wrapper->GetImpl64()) {
    return ParseSymbolsFromELFFile(wrapper->GetImpl64(), callback);
  }
  return ElfStatus::FILE_MALFORMED;
}

ElfStatus ReadMinExecutableVirtualAddressFromElfFile(const std::string& filename,
                                                     const BuildId& expected_build_id,
                                                     uint64_t* min_vaddr) {
  std::unique_ptr<ElfReader> wrapper;
  ElfStatus result = OpenObjectFile(filename, 0, 0, &wrapper);
  if (result != ElfStatus::NO_ERROR) {
    return result;
  }
  result = MatchBuildId(wrapper.get(), expected_build_id);
  if (result != ElfStatus::NO_ERROR) {
    return result;
  }
  if (wrapper->GetMinExecutableVaddr(min_vaddr)) {
    return ElfStatus::NO_ERROR;
  }
  return ElfStatus::FILE_MALFORMED;
}

ElfStatus ReadSectionFromElfFile(const std::string& filename, const std::string& section_name,
                                 std::string* content) {
  std::unique_ptr<ElfReader> wrapper;
  ElfStatus result = OpenObjectFile(filename, 0, 0, &wrapper);
  if (result != ElfStatus::NO_ERROR) {
    return result;
  }
  if (!wrapper->ReadSectionData(section_name.c_str(), content)) {
    return ElfStatus::READ_FAILED;
  }
  return ElfStatus::NO_ERROR;
}
