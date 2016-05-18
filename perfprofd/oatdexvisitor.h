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

  // Top-level callback invoked when visiting an OAT file.
  // Here adler32_checksum and executable_offset are drawn from the
  // OAT file header, and base_text is taken from the oatexec entry
  // in the dynsym (latter will reflect any additional alignment from
  // the containing ELF file).
  virtual void visitOAT(bool is64bit,
                        uint32_t adler32_checksum,
                        uint64_t executable_offset,
                        uint64_t base_text);

  // Callback invoked when visiting a DEX file.
  virtual void visitDEX(unsigned char sha1sig[20]);

  // Invoked for each DEX class
  virtual void visitClass(const std::string &className, uint32_t nMethods);

  // Invoked for each dex method. It goes without saying that the
  // native code offset + size will only be filled in for OAT DEX files.
  // when reading only a DEX file, these will be set to null.
  virtual void visitMethod(const std::string &methodName,
                           unsigned dexMethodIdx,
                           uint32_t numDexInstrs,
                           uint64_t *nativeCodeOffset,
                           uint32_t *nativeCodeSize);

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

  // We are now visiting class K within the current DEX file.
  // Return value is TRUE for ok, false for error.
  virtual bool setupClass(uint32_t classIdx) = 0;

  // We are now visiting method K within the current DEX class
  // Return value is TRUE for ok, false for error.
  virtual bool setupMethod(uint32_t methodIdx) = 0;

  // Fill in native code offset + size for current method
  // Return value is TRUE for ok, false for error.
  virtual bool getMethodNativeCodeInfo(uint64_t &nativeCodeOffset,
                                       uint32_t &nativeCodeSize) = 0;
};

#endif // SYSTEM_EXTRAS_PERFPROFD_OATDEXVISITOR_H_
