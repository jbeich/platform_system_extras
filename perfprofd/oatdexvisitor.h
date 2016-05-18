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

#ifndef SYSTEM_EXTRAS_PERFPROFD_OATDEXVISITOR_H_
#define SYSTEM_EXTRAS_PERFPROFD_OATDEXVISITOR_H_

#include <inttypes.h>
#include <string>

//
// Interface class for clients to examine oat-embedded DEX file data; clients
// can override selected methods depending on which bits they want to visit.
//
class OatDexVisitor {
 public:
  OatDexVisitor();
  virtual ~OatDexVisitor();
  virtual bool doVisitDex();
  virtual void visitOAT(bool is64bit,
                        uint64_t &base_text);
  virtual void visitDEX(unsigned char sha1sig[20]);
  virtual void visitClass(const std::string &className, uint32_t nMethods);
  virtual void visitMethod(const std::string &methodName,
                           unsigned methodIdx,
                           uint64_t methodCodeOffset);
};

#endif // SYSTEM_EXTRAS_PERFPROFD_OATDEXVISITOR_H_
