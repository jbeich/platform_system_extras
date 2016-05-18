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

static bool harvestOatmapFromOatData(const unsigned char *oatdata,
                                     size_t oatdata_len,
                                     oatmap::MapOatFile &mapfile)
{
  return true;
}

class GenMapOatVisitor : public OatVisitor {
 public:
  GenMapOatVisitor(oatmap::MapOatFile &mf) : mapfile_(mf), success_(true) { }
  virtual ~GenMapOatVisitor() { }
  virtual void visit(bool is64bit,
                     uint64_t &base_text,
                     const unsigned char *oatdata,
                     size_t oatdata_len) {
    success_ = harvestOatmapFromOatData(oatdata, oatdata_len, mapfile_);
  }
  bool success() const { return success_; }
 private:
  oatmap::MapOatFile &mapfile_;
  bool success_;
};

bool genmap_for_oat(const char *oatfile,
                    oatmap::MapOatFile &mapfile) {
  GenMapOatVisitor visitor(mapfile);
  return examineOatFile(oatfile, visitor) && visitor.success();
}
