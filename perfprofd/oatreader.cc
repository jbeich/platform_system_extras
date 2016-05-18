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
#include "oatformat.h"
#include "oatdexvisitor.h"
#include "dexread.h"
#include "perfprofdutils.h"

// Uncomment while debugging
#define DEBUGGING 1

#if DEBUGGING
#define DEBUGLOG(x) W_ALOGD x
#else
#define DEBUGLOG(x)
#endif

#include <android-base/file.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <llvm/ADT/StringRef.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>

#pragma clang diagnostic pop

class OatDataReader {
 public:
  OatDataReader(unsigned char *oatdata,
                size_t oatdata_len,
                OatDexVisitor &visitor)
      : oatdata_(oatdata)
      , visitor_(visitor)
      , oatdata_len_(oatdata_len) { }

  bool examineOatData();

 private:
  unsigned char *oatdata_;
  OatDexVisitor &visitor_;
  uint32_t *class_offsets_;
  size_t oatdata_len_;

 private:
  unsigned char *dbegin() const { return oatdata_; }
  unsigned char *dend() const { return oatdata_ + oatdata_len_; }
  size_t dsize() const { return oatdata_len_; }
  OatFileHeader &oatFileHeader() {
    return *(reinterpret_cast<OatFileHeader *>(oatdata_));
  }
  bool isValidOatHeader();
  template<typename T> bool
    readValueFromOatData(unsigned char* &dat, T &outval);
  bool walkDexFile(unsigned char * &dat);
  bool walkDexFiles();
};

//
// Read a "T" from oat data pointed to be "dat", then advance
// "dat" the correct number of bytes.
//
template<typename T>
bool OatDataReader::readValueFromOatData(unsigned char* &dat, T &outval)
{
  size_t remain = static_cast<size_t>(dend() - dat);
  if (remain < sizeof(T))
    return false;
  memcpy(&outval, dat, sizeof(T));
  dat += sizeof(T);
  return true;
}

bool OatDataReader::walkDexFile(unsigned char * &dat)
{
  uint32_t dex_file_location_size;
  if (!readValueFromOatData(dat, dex_file_location_size)) {
    DEBUGLOG(("dex_file_location_size read failed"));
    return false; // couldn't read
  }
  // sanity check the size
  if (! dex_file_location_size || (dat + dex_file_location_size) > dend()) {
    DEBUGLOG(("bad dex_file_location_size %u", dex_file_location_size));
    return false; // truncated file
  }
  dat += dex_file_location_size; // bypass dex file location
  uint32_t dex_file_checksum;
  if (!readValueFromOatData(dat, dex_file_checksum)) {
    DEBUGLOG(("dex_file_checksum read failed"));
    return false; // truncated file
  }
  uint32_t dex_file_offset;
  if (!readValueFromOatData(dat, dex_file_offset)) {
    DEBUGLOG(("dex_file_offset read failed"));
    return false; // couldn't read
  }
  if (!dex_file_offset || dex_file_offset > dsize()) {
    DEBUGLOG(("bad dex_file_offset %u", dex_file_offset));
    return false; // bad/corrupted dex offset
  }
  unsigned char *dex_file_pointer = dbegin() + dex_file_offset;
  uint32_t class_offsets_offset;
  if (!readValueFromOatData(dat, class_offsets_offset)) {
    DEBUGLOG(("class_offsets_offset read failed"));
    return false;
  }
  if (!class_offsets_offset || class_offsets_offset > dsize()) {
    DEBUGLOG(("bad class_offsets_offset %u", class_offsets_offset));
    return false;
  }
  unsigned char *class_offsets_bpointer = dbegin() + class_offsets_offset;
  if (! IsWordAlignedPtr(class_offsets_bpointer)) {
    DEBUGLOG(("class offsets pointer not word aligned: %p", class_offsets_bpointer));
    return false;
  }
  class_offsets_ = reinterpret_cast<uint32_t *>(class_offsets_bpointer);

  DEBUGLOG(("invoking examineDexMemory"));
  if (! examineDexMemory(dex_file_pointer, dend(),  visitor_))
    return false;

  uint32_t lookup_table_offset;
  if (!readValueFromOatData(dat, lookup_table_offset))
    return false;
  return true;
}

bool OatDataReader::walkDexFiles()
{
  //
  // Read file header, advance past key-value store
  //
  unsigned char *dat = dbegin();
  dat += sizeof(OatFileHeader);
  if (dat > dend())
    return false; // truncated file
  dat += oatFileHeader().key_value_store_size;
  if (dat > dend())
    return false; // truncated file
  uint32_t nDexFiles = oatFileHeader().dex_file_count;
  for (unsigned dc = 0; dc < nDexFiles; ++dc) {
    if (! walkDexFile(dat)) {
      DEBUGLOG(("walkDexFile failed at iteration %d", dc));
      return false;
    }
  }
  return true;
}

bool OatDataReader::isValidOatHeader()
{
  if (memcmp(OatMagic, oatFileHeader().oatmagic, sizeof(OatMagic))) {
    return false;
  }
  if (memcmp(OatVersion, oatFileHeader().oatversion, sizeof(OatVersion))) {
    return false;
  }
  return true;
}

bool OatDataReader::examineOatData()
{
  if (!isValidOatHeader()) {
    DEBUGLOG(("bad OAT header"));
    return false;
  }
  if (! walkDexFiles()) {
    return false;
  }
  return true;
}

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
                   OatDexVisitor &visitor)

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
  visitor.visitOAT(is64bit, base_text);

  // Stop here if we're not going to visit the dex files
  if (! visitor.doVisitDex())
    return true;

  // Examine the oatdata
  unsigned char *rodata = const_cast<unsigned char *>(data.bytes_begin());
  OatDataReader reader(rodata, data.size(), visitor);
  return reader.examineOatData();
}

bool examineOatFile(const std::string &path,
                    OatDexVisitor &visitor)
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
