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

class GenMapOatVisitor : public OatDexVisitor {
 public:
  GenMapOatVisitor(oatmap::MapOatFile &mf)
      : mapfile_(mf)
      , success_(true) { }
  virtual ~GenMapOatVisitor() { }

  virtual void visitOAT(bool is64bit,
                        uint64_t &base_text) {
    mapfile_.set_adler32_checksum(1234567);
  }

  // To be implemented
  void visitDEX(unsigned char sha1sig[20]) { }
  void visitClass(const std::string &className, uint32_t nMethods) { }
  void visitMethod(const std::string &methodName,
                   unsigned methodIdx,
                   uint32_t numInstrs,
                   uint64_t *nativeCodeOffset) {}

  bool success() const { return success_; }
  bool markFailure() { success_ = false; return false; }

 private:
  oatmap::MapOatFile &mapfile_;
  bool success_;

 private:
};

bool genmap_for_oat(const char *oatfile,
                    oatmap::MapOatFile &mapfile) {
  GenMapOatVisitor visitor(mapfile);
  return examineOatFile(oatfile, visitor) && visitor.success();
}
