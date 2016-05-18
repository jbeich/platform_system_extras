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
#include "oatdexvisitor.h"
#include "oatformat.h"
#include "dexread.h"

#include <sstream>
//#include <iostream>

class GenMapOatVisitor : public OatDexVisitor {
 public:
  GenMapOatVisitor(oatmap::MapOatFile &mf)
      : mapfile_(mf)
      , current_dexfile_(nullptr)
      , current_dexclass_(nullptr)
      , executable_offset_(0u)
      , class_counter_(0u)
      , success_(true) { }
  virtual ~GenMapOatVisitor() { }

  virtual void visitOAT(bool is64bit,
                        uint32_t adler32_checksum,
                        uint64_t executable_offset,
                        uint64_t base_text) {
#if 0
    std::cerr << "executable_offset is 0x" << std::hex << executable_offset
              << std::dec << " (dec " << executable_offset << ")\n";
    std::cerr << "base_text is 0x" << std::hex << base_text
              << std::dec << " (dec " << base_text << ")\n";
#endif
    mapfile_.set_adler32_checksum(adler32_checksum);
    executable_offset_ = executable_offset;
  }

  void visitDEX(unsigned char sha1sig[20]) {
    auto dexfile = mapfile_.add_dexfiles();
    std::stringstream sha1;
    for (unsigned ii = 0; ii < 20; ++ii) {
      sha1 << std::hex << static_cast<int>(sha1sig[ii]);
    }
    dexfile->set_sha1signature(sha1.str());
    // std::cerr << "adding DEX with sha1 " << sha1.str() << "\n";
    current_dexfile_ = dexfile;
    class_counter_ = 0;
  }

  void visitClass(const std::string &className, uint32_t nMethods) {
    assert(current_dexfile_);
    current_dexclass_ = nullptr;
    class_counter_ += 1;
  }

  void visitMethod(const std::string &methodName,
                   unsigned dexMethodIdx,
                   uint32_t numDexInstrs,
                   uint64_t *nativeCodeOffset,
                   uint32_t *nativeCodeSize) {
    if (nativeCodeOffset && nativeCodeSize && *nativeCodeSize > 0) {
      if (! current_dexclass_) {
        auto dexclass = current_dexfile_->add_classes();
        dexclass->set_classindex(class_counter_ - 1);
        current_dexclass_ = dexclass;
      }
      assert(current_dexclass_);
      assert(nativeCodeSize);
      auto dexmethod = current_dexclass_->add_methods();
      dexmethod->set_dindex(dexMethodIdx);
      dexmethod->set_dsize(numDexInstrs);
      dexmethod->set_mstart(*nativeCodeOffset - executable_offset_);
      dexmethod->set_msize(*nativeCodeSize);
    }
  }

  bool success() const { return success_; }
  bool markFailure() { success_ = false; return false; }

 private:
  oatmap::MapOatFile &mapfile_;
  oatmap::MapDexFile *current_dexfile_;
  oatmap::MapDexClass *current_dexclass_;
  uint64_t executable_offset_;
  uint32_t class_counter_;
  bool success_;

 private:
};

bool genmap_for_oat(const char *oatfile,
                    oatmap::MapOatFile &mapfile) {
  GenMapOatVisitor visitor(mapfile);
  return examineOatFile(oatfile, visitor) && visitor.success();
}
