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

#include <sstream>

// Uncomment while debugging
// #define DEBUGGING 1

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

class OatDataReader : public OatReaderHooks {
 public:
  OatDataReader(unsigned char *oatdata,
                size_t oatdata_len,
                bool is64bit,
                uint64_t base_text,
                OatDexVisitor &visitor)
      : oatdata_(oatdata)
      , visitor_(visitor)
      , base_text_(base_text)
      , class_offsets_(0)
      , methods_ptr_(nullptr)
      , bitmap_ptr_(nullptr)
      , bitmap_size_(0)
      , class_disp_(OatDispMax)
      , code_offset_(0)
      , code_size_(0)
      , oatdata_len_(oatdata_len)
      , is64bit_(is64bit) { }

  bool examineOatData();

  bool setupClass(uint32_t classIdx);
  bool setupMethod(uint32_t methodIdx);
  bool getMethodNativeCodeInfo(uint64_t &nativeCodeOffset,
                               uint32_t &nativeCodeSize);

 private:
  unsigned char *oatdata_;
  OatDexVisitor &visitor_;
  uint64_t base_text_;

  // class offsets for current DEX
  uint32_t *class_offsets_;

  // methods + bitmap pointer for current class
  uint32_t *methods_ptr_;
  uint32_t *bitmap_ptr_;
  uint32_t bitmap_size_;

  // disposition for current class
  OatClassDisposition class_disp_;

  // code offset + size for current method
  uint64_t code_offset_;
  uint32_t code_size_;

  size_t oatdata_len_;
  bool is64bit_;


 private:
  unsigned char *dbegin() const { return oatdata_; }
  unsigned char *dend() const { return oatdata_ + oatdata_len_; }
  size_t dsize() const { return oatdata_len_; }
  OatFileHeader &oatFileHeader() {
    return *(reinterpret_cast<OatFileHeader *>(oatdata_));
  }
  uint32_t oatClassOffset(uint32_t class_def_index) const {
    return class_offsets_[class_def_index];
  }
  bool isValidOatHeader();
  template<typename T> bool
    readValueFromOatData(unsigned char* &dat, T &outval);
  bool walkDexFile(unsigned char * &dat);
  bool walkDexFiles();
};

#if DEBUGGING
static std::string mkBitString(uint32_t word) {
  std::stringstream ss;
  ss << "[ ";
  for (unsigned ii = 0; ii < 31; ++ii) {
    if (word & (1<<ii)) {
      ss << ii << " ";
    }
  }
  ss << "]";
  return ss.str();
}
#endif

bool OatDataReader::setupClass(uint32_t classIdx)
{
  class_disp_ = OatDispMax;
  methods_ptr_ = nullptr;
  bitmap_ptr_ = nullptr;
  bitmap_size_ = 0;

  //
  // Unpack OAT data for this class
  //
  uint32_t oatclass_off = class_offsets_[classIdx];
  unsigned char *oatclass_ptr = dbegin() + oatclass_off;
  if (oatclass_ptr > dend())
    return false;

  // class status
  unsigned char *status_bptr = oatclass_ptr;
  if (! IsAlignedPtr(status_bptr, 2))
    return false;

  // compilation disposition
  unsigned char *disp_bptr = oatclass_ptr + sizeof(uint16_t);
  if (disp_bptr > dend())
    return false;
  uint16_t *disp_ptr = reinterpret_cast<uint16_t*>(disp_bptr);
  class_disp_ = static_cast<OatClassDisposition>(*disp_ptr);

  DEBUGLOG(("class disp: %s",
            class_disp_ == OatDispAllCompiled ? "allcompiled" :
            class_disp_ == OatDispSomeCompiled ? "somecompiled" :
            class_disp_ == OatDispNoneCompiled ? "nonecompiled" : "<illegal>"));

  unsigned char *after_bptr = disp_bptr + sizeof(uint16_t);
  if (after_bptr > dend())
    return false;

  // how much of this class is compiled?
  switch(class_disp_) {
    case OatDispAllCompiled:
      methods_ptr_ = reinterpret_cast<uint32_t *>(after_bptr);
      break;
    case OatDispSomeCompiled: {
      bitmap_size_ = *(reinterpret_cast<const uint32_t*>(after_bptr));
      unsigned char *bitmap_bptr = after_bptr + sizeof(uint32_t);
      bitmap_ptr_ = reinterpret_cast<uint32_t *>(bitmap_bptr);
      methods_ptr_ = reinterpret_cast<uint32_t *>(bitmap_bptr + bitmap_size_);
      DEBUGLOG(("setupClass: bitmap_size_ is %d", bitmap_size_));
      DEBUGLOG(("setupClass: bitmap[0] is 0x%x %s", bitmap_ptr_[0],
                mkBitString(bitmap_ptr_[0]).c_str()));
      break;
    }
    case OatDispNoneCompiled:
      break;
    default:
      return false;
  }
  if (reinterpret_cast<unsigned char*>(methods_ptr_) > dend())
    return false;
  return true;
}

//
// Thumb code addresses have the LSB set; this routine scrubs out that bit.
//
static unsigned char *cleanThumbBit(unsigned char *entry_ptr)
{
  uintptr_t pval = reinterpret_cast<uintptr_t>(entry_ptr);
  pval &= ~0x1;
  return reinterpret_cast<unsigned char*>(pval);
}

bool isBitSet(unsigned *bitvec,
              unsigned bv_words,
              unsigned slot) {
  return ((slot < (bv_words << 5)) &&
          (bitvec[slot >> 5] & (1 << (slot & 0x1f))) != 0);
}

unsigned numBitsSet(unsigned *bitvec,
                    unsigned bv_words,
                    unsigned end_index) {
  unsigned endword = end_index >> 5;
  assert(end_index < (bv_words << 5));
  uint32_t remainder = end_index & 0x1f;
  uint32_t nbits = 0;
  for (uint32_t w = 0u; w < endword; ++w) {
    nbits += __builtin_popcount(bitvec[w]);
  }
  if (remainder != 0u) {
    uint32_t mask = ~(0xffffffffu << remainder);
    nbits +=  __builtin_popcount(bitvec[endword] & mask);
 }
  return nbits;
}

bool OatDataReader::setupMethod(uint32_t methodIdx)
{
  code_offset_ = 0;
  code_size_ = 0;

  DEBUGLOG(("setupMethod: methodIdx %d", methodIdx));

  uint32_t midx = 0;
  switch(class_disp_) {
    case OatDispAllCompiled:
      midx = methodIdx;
      break;
    case OatDispSomeCompiled:
      if (! isBitSet(bitmap_ptr_, bitmap_size_>>2, methodIdx)) {
        DEBUGLOG(("bitmap not set, early return"));
        return true;
      }
      midx = numBitsSet(bitmap_ptr_, bitmap_size_>>1, methodIdx);
      break;
    case OatDispNoneCompiled:
      return true;
    default:
      return false;
  }


  // Update code offset
  code_offset_ = methods_ptr_[midx];

  DEBUGLOG(("setupMethod: methodIdx %d => midx %d (off %llu)",
            methodIdx, midx, code_offset_));

  //
  // Form pointer to code, then walk back to locate the quick method hdr,
  // from which we can extract the code size.
  //
  unsigned char *code_ptr = cleanThumbBit(dbegin() + code_offset_);
  OatPreMethodHeader *mhdr =
                     (reinterpret_cast<OatPreMethodHeader*>(code_ptr) - 1);
  code_size_ = mhdr->codeSizeInBytes;

  DEBUGLOG(("mhdr: framesize=%d codesize=%d", mhdr->frameSizeInBytes,
            mhdr->codeSizeInBytes));

  return true;
}

bool OatDataReader:: getMethodNativeCodeInfo(uint64_t &nativeCodeOffset,
                                             uint32_t &nativeCodeSize)
{
  if (code_offset_) {
    nativeCodeOffset = code_offset_ & ~(0x1llu);
    nativeCodeSize = code_size_;
    return true;
  }
  return false;
}

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


  DEBUGLOG(("invoking examineDexMemory hooks"));
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

  // Invoke visitor
  visitor_.visitOAT(is64bit_,
                    oatFileHeader().adler32_checksum,
                    oatFileHeader().executable_offset,
                    base_text_);

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

  // Stop here if we're not going to visit the dex files
  if (! visitor.doVisitDex())
    return true;

  // Examine the oatdata
  unsigned char *rodata = const_cast<unsigned char *>(data.bytes_begin());
  OatDataReader reader(rodata, data.size(), is64bit, base_text, visitor);
  visitor.setOatReaderHooks(&reader);
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
