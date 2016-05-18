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

class OatReaderHooks;

//
// Interface class for clients to examine oat-embedded DEX file data; clients
// can override selected methods depending on which bits they want to visit.
//
class OatDexVisitor {
 public:
  OatDexVisitor() : hooks_(nullptr) { }
  virtual ~OatDexVisitor();

  // Called by the OAT reader. If this method returns false, the reader
  // will not examine any DEX files within the OAT.
  virtual bool doVisitDex();

  // These callbacks will be invoked by the OAT/DEX reader as it proceeds along
  // through the OAT file and/or DEX file(s).
  virtual void visitOAT(bool is64bit,
                        uint64_t &base_text);
  virtual void visitDEX(unsigned char sha1sig[20]);
  virtual void visitClass(const std::string &className, uint32_t nMethods);

  // native code offset will only be filled in for OAT DEX files.
  // when reading only a DEX file, will be set to null.
  virtual void visitMethod(const std::string &methodName,
                           unsigned methodIdx,
                           uint32_t numInstrs,
                           uint64_t *nativeCodeOffset);

  // Get/set hooks object. Used by the OAT reader.
  OatReaderHooks *getOatReaderHooks() { return hooks_; }
  void setOatReaderHooks(OatReaderHooks *hooks) { hooks_ = hooks; }

 private:
  OatReaderHooks *hooks_;
};

//
// A lot of the guts in an OAT file can only be accessed using
// information drawn from the DEX files embedded into the OAT. For
// example, there is no explicit count of the number of classes stored
// in an OAT -- you have to find this out by walking the OAT's DEX
// files. The interface class below contains hooks for the DEX file
// reader to call back into the OAT reader at strategic points (for
// example, to announce that class X is about to be visited).
//
class OatReaderHooks {
 public:
  OatReaderHooks() {}
  virtual ~OatReaderHooks();

  // We are now visiting class K within the current DEX file
  virtual void setupClass(uint32_t classIdx) = 0;

  // We are now visiting method K within the current DEX class
  virtual void setupMethod(uint32_t methodIdx) = 0;

  // Return native code offset for current method
  virtual uint64_t getMethodNativeCodeOffset() = 0;
};

#endif // SYSTEM_EXTRAS_PERFPROFD_OATDEXVISITOR_H_
