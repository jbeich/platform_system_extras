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


bool IsValidElfFile(int fd) {
  static const char elf_magic[] = {0x7f, 'E', 'L', 'F'};
  char buf[4];
  return android::base::ReadFully(fd, buf, 4) && memcmp(buf, elf_magic, 4) == 0;
}

bool IsValidElfPath(const std::string& filename) {
  if (!IsRegularFile(filename)) {
    return false;
  }
  std::string mode = std::string("rb") + CLOSE_ON_EXEC_MODE;
  FILE* fp = fopen(filename.c_str(), mode.c_str());
  if (fp == nullptr) {
    return false;
  }
  bool result = IsValidElfFile(fileno(fp));
  fclose(fp);
  return result;
}

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
bool GetBuildIdFromELFFile(const llvm::object::ELFObjectFile<ELFT>* elf, BuildId* build_id) {
  for (auto it = elf->section_begin(); it != elf->section_end(); ++it) {
    const llvm::object::ELFSectionRef& section_ref = *it;
    if (section_ref.getType() == llvm::ELF::SHT_NOTE) {
      llvm::StringRef data;
      if (it->getContents(data)) {
        LOG(DEBUG) << "read note section error";
        continue;
      }
      if (GetBuildIdFromNoteSection(reinterpret_cast<const char*>(data.data()),
                                    data.size(), build_id)) {
        return true;
      }
    }
  }
  return false;
}

static bool GetBuildIdFromObjectFile(llvm::object::ObjectFile* obj, BuildId* build_id) {
  bool result = false;
  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(obj)) {
    result = GetBuildIdFromELFFile(elf, build_id);
  } else if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(obj)) {
    result = GetBuildIdFromELFFile(elf, build_id);
  } else {
    LOG(ERROR) << "unknown elf format in file " << obj->getFileName().data();
    return false;
  }
  if (!result) {
    LOG(DEBUG) << "no build id present in file " << obj->getFileName().data();
  }
  return result;
}

struct BinaryRet {
  llvm::object::OwningBinary<llvm::object::Binary> binary;
  llvm::object::ObjectFile* obj;

  BinaryRet() : obj(nullptr) {
  }
};

static BinaryRet OpenObjectFile(const std::string& filename, uint64_t file_offset = 0,
                                uint64_t file_size = 0) {
  BinaryRet ret;
  FileHelper fhelper = FileHelper::OpenReadOnly(filename);
  if (!fhelper) {
    PLOG(DEBUG) << "failed to open " << filename;
    return ret;
  }
  if (file_size == 0) {
    file_size = GetFileSize(filename);
    if (file_size == 0) {
      PLOG(ERROR) << "failed to get size of file " << filename;
      return ret;
    }
  }
  auto buffer_or_err = llvm::MemoryBuffer::getOpenFileSlice(fhelper.fd(), filename, file_size, file_offset);
  if (!buffer_or_err) {
    LOG(ERROR) << "failed to read " << filename << " [" << file_offset << "-" << (file_offset + file_size)
        << "]: " << buffer_or_err.getError().message();
    return ret;
  }
  auto binary_or_err = llvm::object::createBinary(buffer_or_err.get()->getMemBufferRef());
  if (!binary_or_err) {
    LOG(ERROR) << filename << " [" << file_offset << "-" << (file_offset + file_size)
        << "] is not a binary file: " << binary_or_err.getError().message();
    return ret;
  }
  ret.binary = llvm::object::OwningBinary<llvm::object::Binary>(std::move(binary_or_err.get()),
                                                                std::move(buffer_or_err.get()));
  ret.obj = llvm::dyn_cast<llvm::object::ObjectFile>(ret.binary.getBinary());
  if (ret.obj == nullptr) {
    LOG(ERROR) << filename << " [" << file_offset << "-" << (file_offset + file_size)
        << "] is not an object file";
  }
  return ret;
}

static BinaryRet OpenObjectFileFromString(const std::string& s, const std::string& content_name) {
  BinaryRet ret;
  auto buffer = llvm::MemoryBuffer::getMemBuffer(s);
  auto binary_or_err = llvm::object::createBinary(buffer->getMemBufferRef());
  if (!binary_or_err) {
    LOG(ERROR) << content_name << " is not a binary file: " << binary_or_err.getError().message();
    return ret;
  }
  ret.binary = llvm::object::OwningBinary<llvm::object::Binary>(std::move(binary_or_err.get()),
                                                                std::move(buffer));
  ret.obj = llvm::dyn_cast<llvm::object::ObjectFile>(ret.binary.getBinary());
  if (ret.obj == nullptr) {
    LOG(ERROR) << content_name << " is not an object file";
  }
  return ret;
}

bool GetBuildIdFromElfFile(const std::string& filename, BuildId* build_id) {
  if (!IsValidElfPath(filename)) {
    return false;
  }
  bool result = GetBuildIdFromEmbeddedElfFile(filename, 0, 0, build_id);
  LOG(VERBOSE) << "GetBuildIdFromElfFile(" << filename << ") => " << build_id->ToString();
  return result;
}

bool GetBuildIdFromEmbeddedElfFile(const std::string& filename, uint64_t file_offset,
                                   uint32_t file_size, BuildId* build_id) {
  BinaryRet ret = OpenObjectFile(filename, file_offset, file_size);
  if (ret.obj == nullptr) {
    return false;
  }
  return GetBuildIdFromObjectFile(ret.obj, build_id);
}

template <class ELFT>
bool ReadSectionFromELFFile(const llvm::object::ELFObjectFile<ELFT>* elf, const std::string& section_name,
                            std::string* content, bool report_error = true) {
  for (llvm::object::section_iterator it = elf->section_begin(); it != elf->section_end(); ++it) {
    llvm::StringRef name;
    if (it->getName(name) || name != section_name) {
      continue;
    }
    llvm::StringRef data;
    if (it->getContents(data)) {
      if (report_error) {
        LOG(ERROR) << "failed to read section " << section_name;
      }
      return false;
    }
    *content = data;
    return true;
  }
  if (report_error) {
    LOG(ERROR) << "can't find section " << section_name;
  }
  return false;
}

bool IsArmMappingSymbol(const char* name) {
  // Mapping symbols in arm, which are described in "ELF for ARM Architecture" and
  // "ELF for ARM 64-bit Architecture". The regular expression to match mapping symbol
  // is ^\$(a|d|t|x)(\..*)?$
  return name[0] == '$' && strchr("adtx", name[1]) != nullptr && (name[2] == '\0' || name[2] == '.');
}

void ReadSymbolTable(llvm::object::symbol_iterator sym_begin,
                     llvm::object::symbol_iterator sym_end,
                     std::function<void(const ElfFileSymbol&)> callback,
                     bool is_arm) {
  for (; sym_begin != sym_end; ++sym_begin) {
    ElfFileSymbol symbol;
    auto symbol_ref = static_cast<const llvm::object::ELFSymbolRef*>(&*sym_begin);
    llvm::ErrorOr<llvm::object::section_iterator> section_it_or_err = symbol_ref->getSection();
    if (!section_it_or_err) {
      continue;
    }

    llvm::StringRef section_name;
    if (section_it_or_err.get()->getName(section_name) || section_name.empty()) {
      continue;
    }
    if (section_name == ".text") {
      symbol.is_in_text_section = true;
    }
    llvm::ErrorOr<llvm::StringRef> symbol_name_or_err = symbol_ref->getName();
    if (!symbol_name_or_err || symbol_name_or_err.get().empty()) {
      continue;
    }

    symbol.name = symbol_name_or_err.get();
    symbol.vaddr = symbol_ref->getValue();
    if ((symbol.vaddr & 1) != 0 && is_arm) {
      // Arm sets bit 0 to mark it as thumb code, remove the flag.
      symbol.vaddr &= ~1;
    }
    symbol.len = symbol_ref->getSize();
    llvm::object::SymbolRef::Type symbol_type = symbol_ref->getType();
    if (symbol_type == llvm::object::SymbolRef::ST_Function) {
      symbol.is_func = true;
    } else if (symbol_type == llvm::object::SymbolRef::ST_Unknown) {
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

template <class ELFT>
void ParseSymbolsFromELFFile(const llvm::object::ELFObjectFile<ELFT>* elf,
                             std::function<void(const ElfFileSymbol&)> callback) {
  auto machine = elf->getELFFile()->getHeader()->e_machine;
  bool is_arm = (machine == llvm::ELF::EM_ARM || machine == llvm::ELF::EM_AARCH64);
  if (elf->symbol_begin() != elf->symbol_end()) {
    ReadSymbolTable(elf->symbol_begin(), elf->symbol_end(), callback, is_arm);
  } else if (elf->dynamic_symbol_begin()->getRawDataRefImpl() != llvm::object::DataRefImpl()) {
    ReadSymbolTable(elf->dynamic_symbol_begin(), elf->dynamic_symbol_end(), callback, is_arm);
  }
  std::string debugdata;
  if (ReadSectionFromELFFile(elf, ".gnu_debugdata", &debugdata, false)) {
    LOG(VERBOSE) << "Read .gnu_debugdata from " << elf->getFileName().str();
    std::string decompressed_data;
    if (XzDecompress(debugdata, &decompressed_data)) {
      std::string content_name = std::string(".gnu_debugdata in ") + elf->getFileName().str();
      BinaryRet ret = OpenObjectFileFromString(decompressed_data, content_name);
      if (ret.obj != nullptr) {
        if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(ret.obj)) {
          ParseSymbolsFromELFFile(elf, callback);
        } else if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(ret.obj)) {
          ParseSymbolsFromELFFile(elf, callback);
        }
      }
    }
  }
}

bool MatchBuildId(llvm::object::ObjectFile* obj, const BuildId& expected_build_id,
                  const std::string& debug_filename) {
  if (expected_build_id.IsEmpty()) {
    return true;
  }
  BuildId real_build_id;
  if (!GetBuildIdFromObjectFile(obj, &real_build_id)) {
    return false;
  }
  if (expected_build_id != real_build_id) {
    LOG(DEBUG) << "build id for " << debug_filename << " mismatch: "
               << "expected " << expected_build_id.ToString()
               << ", real " << real_build_id.ToString();
    return false;
  }
  return true;
}

bool ParseSymbolsFromElfFile(const std::string& filename, const BuildId& expected_build_id,
                             std::function<void(const ElfFileSymbol&)> callback) {
  if (!IsValidElfPath(filename)) {
    return false;
  }
  return ParseSymbolsFromEmbeddedElfFile(filename, 0, 0, expected_build_id, callback);
}

bool ParseSymbolsFromEmbeddedElfFile(const std::string& filename, uint64_t file_offset,
                                     uint32_t file_size, const BuildId& expected_build_id,
                                     std::function<void(const ElfFileSymbol&)> callback) {
  LOG(VERBOSE) << "Parse symbols from file " << filename;
  BinaryRet ret = OpenObjectFile(filename, file_offset, file_size);
  if (ret.obj == nullptr || !MatchBuildId(ret.obj, expected_build_id, filename)) {
    return false;
  }
  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(ret.obj)) {
    ParseSymbolsFromELFFile(elf, callback);
  } else if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(ret.obj)) {
    ParseSymbolsFromELFFile(elf, callback);
  } else {
    LOG(ERROR) << "unknown elf format in file " << filename;
    return false;
  }
  return true;
}

template <class ELFT>
bool ReadMinExecutableVirtualAddress(const llvm::object::ELFFile<ELFT>* elf, uint64_t* p_vaddr) {
  bool has_vaddr = false;
  uint64_t min_addr = std::numeric_limits<uint64_t>::max();
  for (auto it = elf->program_header_begin(); it != elf->program_header_end(); ++it) {
    if ((it->p_type == llvm::ELF::PT_LOAD) && (it->p_flags & llvm::ELF::PF_X)) {
      if (it->p_vaddr < min_addr) {
        min_addr = it->p_vaddr;
        has_vaddr = true;
      }
    }
  }
  if (has_vaddr) {
    *p_vaddr = min_addr;
  }
  return has_vaddr;
}

bool ReadMinExecutableVirtualAddressFromElfFile(const std::string& filename,
                                                const BuildId& expected_build_id,
                                                uint64_t* min_vaddr) {
  if (!IsValidElfPath(filename)) {
    return false;
  }
  BinaryRet ret = OpenObjectFile(filename);
  if (ret.obj == nullptr || !MatchBuildId(ret.obj, expected_build_id, filename)) {
    return false;
  }

  bool result = false;
  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(ret.obj)) {
    result = ReadMinExecutableVirtualAddress(elf->getELFFile(), min_vaddr);
  } else if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(ret.obj)) {
    result = ReadMinExecutableVirtualAddress(elf->getELFFile(), min_vaddr);
  } else {
    LOG(ERROR) << "unknown elf format in file" << filename;
    return false;
  }

  if (!result) {
    LOG(ERROR) << "no program header in file " << filename;
  }
  return result;
}

bool ReadSectionFromElfFile(const std::string& filename, const std::string& section_name,
                            std::string* content) {
  if (!IsValidElfPath(filename)) {
    return false;
  }
  BinaryRet ret = OpenObjectFile(filename);
  if (ret.obj == nullptr) {
    return false;
  }
  bool result = false;
  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(ret.obj)) {
    result = ReadSectionFromELFFile(elf, section_name, content);
  } else if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(ret.obj)) {
    result = ReadSectionFromELFFile(elf, section_name, content);
  } else {
    LOG(ERROR) << "unknown elf format in file" << filename;
    return false;
  }
  return result;
}
