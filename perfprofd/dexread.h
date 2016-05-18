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

#ifndef SYSTEM_EXTRAS_PERFPROFD_DEXREAD_H_
#define SYSTEM_EXTRAS_PERFPROFD_DEXREAD_H_



#include <string>

class OatDexVisitor;

//
// Given a DEX file embedded inside an OAT file, walk the DEX and
// invoke various callbacks via a caller-supplied visitor object.
// Here 'dex_data' points at the memory region containing
// the DEX file, and 'limit' is a pointer to the end of the memory
// region containin one or more DEX files (used for error-checking).
//

bool examineOatDexFile(unsigned char *dex_data,
                       unsigned char *limit,
                       OatDexVisitor &visitor);


//
// Abstract class for clients to examine oat-embedded DEX file data.
//
class OatDexVisitor {
 public:
  OatDexVisitor() { }
  virtual ~OatDexVisitor() { }
  virtual void visitDEX(unsigned char sha1sig[20]) = 0;
  virtual void visitClass(const std::string &className, uint32_t nMethods);
  virtual void visitMethod(const std::string &methodName,
                           unsigned methodIdx,
                           uint64_t methodCodeOffset);
};

inline bool IsWordAlignedPtr(unsigned char *ptr) {
  uint64_t val64 = reinterpret_cast<uint64_t>(ptr);
  return (val64 & 0x3 ? false : true);
}

#endif // SYSTEM_EXTRAS_PERFPROFD_DEXREAD_H_
