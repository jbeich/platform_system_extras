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

#include "genoatmap.h"
#include "oatreader.h"
#include "oatformat.h"
#include "dexread.h"

class GenMapOatVisitor : public OatVisitor, OatDexVisitor {
 public:
  GenMapOatVisitor(oatmap::MapOatFile &mf)
      : mapfile_(mf)
      , oatdata_(nullptr)
      , oatdata_len_(0)
      , success_(true) { }
  virtual ~GenMapOatVisitor() { }

  virtual void visitOAT(bool is64bit,
                        uint64_t &base_text,
                        unsigned char *oatdata,
                        size_t oatdata_len) {
    oatdata_ = oatdata;
    oatdata_len_ = oatdata_len;
    success_ = harvestOatmap();
  }

  // To be implemented
  void visitDEX(unsigned char sha1sig[20]) { }
  void visitClass(const std::string &className, uint32_t nMethods) { }
  void visitMethod(const std::string &methodName,
                   unsigned methodIdx,
                   uint64_t methodCodeOffset) {}

  bool success() const { return success_; }
  bool markFailure() { success_ = false; return false; }
  unsigned char *dbegin() const { return oatdata_; }
  unsigned char *dend() const { return oatdata_ + oatdata_len_; }
  size_t dsize() const { return oatdata_len_; }

  OatFileHeader &oatFileHeader() {
    return *(reinterpret_cast<OatFileHeader *>(oatdata_));
  }

 private:
  oatmap::MapOatFile &mapfile_;
  std::vector<unsigned char *> dexptrvec_;
  unsigned char *oatdata_;
  uint32_t *class_offsets_;
  size_t oatdata_len_;
  bool success_;

 private:
  bool harvestOatmap();
  bool isValidOatHeader();
  template<typename T> bool
    readValueFromOatData(unsigned char* &dat, T &outval);
  bool collectDexFileInfo(unsigned char * &dat);
  bool collectDexFiles();
};

bool GenMapOatVisitor::isValidOatHeader()
{
  if (memcmp(OatMagic, oatFileHeader().oatmagic, sizeof(OatMagic))) {
    return markFailure();
  }
  if (memcmp(OatVersion, oatFileHeader().oatversion, sizeof(OatVersion))) {
    return markFailure();
  }
  return true;
}

//
// Read a "T" from oat data pointed to be "dat", then advance
// "dat" the correct number of bytes.
//
template<typename T>
bool GenMapOatVisitor::readValueFromOatData(unsigned char* &dat, T &outval)
{
  size_t remain = static_cast<size_t>(dend() - dat);
  if (remain < sizeof(T))
    return false;
  memcpy(&outval, dat, sizeof(T));
  dat += sizeof(T);
  return true;
}

bool GenMapOatVisitor::collectDexFileInfo(unsigned char * &dat)
{
  uint32_t dex_file_location_size;
  if (!readValueFromOatData(dat, dex_file_location_size))
    return markFailure(); // couldn't read
  // sanity check the size
  if (! dex_file_location_size || dat + dex_file_location_size > dend())
    return markFailure(); // truncated file
  dat += dex_file_location_size;
  return markFailure(); // couldn't read
  uint32_t dex_file_checksum;
  if (!readValueFromOatData(dat, dex_file_checksum))
    return markFailure(); // couldn't read
  uint32_t dex_file_offset;
  if (!readValueFromOatData(dat, dex_file_offset))
    return markFailure(); // couldn't read
  if (!dex_file_offset || dex_file_offset > dsize())
    return markFailure(); // bad/corrupted dex offset
  unsigned char *dex_file_pointer = dbegin() + dex_file_offset;
  dexptrvec_.push_back(dex_file_pointer);
  uint32_t class_offsets_offset;
  if (!readValueFromOatData(dat, class_offsets_offset))
    return markFailure();
  if (!class_offsets_offset || class_offsets_offset > dsize())
    return markFailure();
  unsigned char *class_offsets_bpointer = dbegin() + class_offsets_offset;
  if (! IsWordAlignedPtr(class_offsets_bpointer))
    return markFailure();
  class_offsets_ = reinterpret_cast<uint32_t *>(class_offsets_bpointer);

  if (! examineOatDexFile(dex_file_pointer, dend(),  *this))
    return false;

  uint32_t lookup_table_offset;
  if (!readValueFromOatData(dat, lookup_table_offset))
    return markFailure();
  return true;
}

bool GenMapOatVisitor::collectDexFiles()
{
  //
  // Read file header, advance past key-value store
  //
  unsigned char *dat = dbegin();
  dat += sizeof(OatFileHeader);
  if (dat > dend())
    return markFailure(); // truncated file
  dat += oatFileHeader().key_value_store_size;
  if (dat > dend())
    return markFailure(); // truncated file

  //
  // We're interested in the embedded DEX files. Locate
  // the starting offsets within the oatdata for them.
  // Note that we don't actually care about the dex file names,
  // but we have to get past that part to get to the offsets.
  //
  uint32_t nDexFiles = oatFileHeader().dex_file_count;
  for (unsigned dc = 0; dc < nDexFiles; ++dc) {
    if (! collectDexFileInfo(dat))
      return false;
  }
  return true;
}

bool GenMapOatVisitor::harvestOatmap()
{
  if (!isValidOatHeader())
    return false;
  if (! collectDexFiles()) {
    return false;
  }
  mapfile_.set_adler32_checksum(0);
#if 0
  if (!walkDexFiles())
    return false;
#endif
  return true;
}

bool genmap_for_oat(const char *oatfile,
                    oatmap::MapOatFile &mapfile) {
  GenMapOatVisitor visitor(mapfile);
  return examineOatFile(oatfile, visitor) && visitor.success();
}
