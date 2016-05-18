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

#ifndef SYSTEM_EXTRAS_PERFPROFD_OATREADER_H_
#define SYSTEM_EXTRAS_PERFPROFD_OATREADER_H_

#include <string>

class OatVisitor;

//
// Given a (potential) OAT file, open the file and check to make sure
// that it is indeed an OAT, and if so, invoke the "visit" method
// on the visitor object passed to examineOatFile, passing the flavor
// of the OAT file (32 vs 64 bit), the base text address, and a pointer
// to the OAT data section for the file.
//
bool examineOatFile(const std::string &path,
                    OatVisitor &visitor);


//
// Abstract class for clients to examine OAT file data.
//
class OatVisitor {
 public:
  OatVisitor() { }
  virtual ~OatVisitor() { }
  virtual void visit(bool is64bit,
                     uint64_t &base_text,
                     unsigned char *oatdata,
                     size_t oatdata_len) = 0;
};

#endif
