/*
**
** Copyright 2016, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "oatreader.h"

#include <android-base/file.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <llvm/ADT/StringRef.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>

#pragma clang diagnostic pop

// Borrowed directly from simpleperf/read_elf.cpp. Hopefully can be reused
// directly once the simpleperf perf.data reader is integrated into perfprofd.

static bool IsValidElfFile(int fd) {
  static const char elf_magic[] = {0x7f, 'E', 'L', 'F'};
  char buf[4];
  return android::base::ReadFully(fd, buf, 4) && memcmp(buf, elf_magic, 4) == 0;
}

static bool IsValidElfPath(const std::string& filename) {
  struct stat st;
  if (stat(filename.c_str(), &st) != 0) {
    return false;
  }
  if (!S_ISREG(st.st_mode)) {
    return false;
  }
  std::string mode = std::string("rb");
  FILE* fp = fopen(filename.c_str(), mode.c_str());
  if (fp == nullptr) {
    return false;
  }
  bool result = IsValidElfFile(fileno(fp));
  fclose(fp);
  return result;
}

template <class ELFT>
bool ElfHasOatDynSyms(const llvm::object::ELFObjectFile<ELFT>* elf,
                      uint64_t &base_text)
{
  if (elf->dynamic_symbol_begin()->getRawDataRefImpl() ==
      llvm::object::DataRefImpl())
    return false;

  bool oatdata_found = false;
  bool oatexec_found = false;
  for (auto sym_it = elf->dynamic_symbol_begin();
       sym_it != elf->dynamic_symbol_end();
       ++sym_it) {
    auto sym_ref = static_cast<const llvm::object::ELFSymbolRef*>(&*sym_it);

    // Collect section name
    llvm::ErrorOr<llvm::object::section_iterator> section_it_or_err = sym_ref->getSection();
    if (!section_it_or_err) {
      continue;
    }
    llvm::StringRef section_name;
    if (section_it_or_err.get()->getName(section_name) || section_name.empty()) {
      continue;
    }

    // Collect symbol name
    llvm::ErrorOr<llvm::StringRef> symbol_name_or_err = sym_ref->getName();
    if (!symbol_name_or_err || symbol_name_or_err.get().empty()) {
      continue;
    }
    std::string symbol_name = symbol_name_or_err.get();

    // Look for oatdata/oatexec
    if (section_name == ".text" && symbol_name == "oatexec") {
      // record value
      base_text = sym_ref->getValue();
      oatexec_found = true;
    } else if (section_name == ".rodata" && symbol_name == "oatdata") {
      oatdata_found = true;
    }
  }
  return (oatdata_found && oatexec_found);
}

template <class ELFT>
static bool
CollectElfRodataSection(const llvm::object::ELFObjectFile<ELFT>* elf,
                        llvm::StringRef &data) {
  // Collect .rodata section contents
  for (llvm::object::section_iterator it = elf->section_begin(); it != elf->section_end(); ++it) {
    llvm::StringRef name;
    if (it->getName(name) || name != std::string(".rodata")) {
      continue;
    }
    std::error_code err = it->getContents(data);
    if (err) {
      return false;
    }
    return true;
  }
  return false;
}

template <class ELFT>
bool ExamineElfFile(const llvm::object::ELFObjectFile<ELFT>* elf,
                   bool is64bit,
                   OatVisitor &visitor)

{
  uint64_t base_text = 0;
  if (! ElfHasOatDynSyms(elf, base_text)) {
    return false;
  }

  llvm::StringRef data;
  if ( ! CollectElfRodataSection(elf, data)) {
    return false;
  }

  // Invoke visitor
  const unsigned char *rodata = data.bytes_begin();
  visitor.visit(is64bit, base_text, rodata, data.size());
  return true;
}

bool examineOatFile(const std::string &path,
                    OatVisitor &visitor)
{
  if (!IsValidElfPath(path))
    return false;

  auto binary_or_err = llvm::object::createBinary(llvm::StringRef(path));
  if (!binary_or_err) {
    return false;
  }
  auto owning_binary = std::move(binary_or_err.get());
  llvm::object::Binary* binary = owning_binary.getBinary();
  auto obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary);
  if (obj == nullptr) {
    return false;
  }

  if (auto elf = llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(obj)) {
    bool is64bit = false;
    return ExamineElfFile(elf, is64bit, visitor);
  }
  if (auto elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(obj)) {
    bool is64bit = true;
    return ExamineElfFile(elf, is64bit, visitor);
  }
  return false;
}
