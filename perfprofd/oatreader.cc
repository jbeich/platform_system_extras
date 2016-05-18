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

// Uncomment while debugging
// #define DEBUGGING 1

#include "oatreader.h"
#include "oatdexvisitor.h"
#include "perfprofdutils.h"

#include <sstream>

#include <android-base/file.h>

#include "arch/instruction_set_features.h"
#include "imtable.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "oat.h"
#include "oat_file-inl.h"

static bool examineMethod(const art::DexFile &dex_file,
                          const art::OatHeader& oat_header,
                          const art::OatFile::OatMethod& oat_method,
                          uint32_t midx,
                          const art::DexFile::CodeItem* code_item,
                          OatDexVisitor &visitor)
{
  // Quick header for method, so as to get native code size
  const art::OatQuickMethodHeader* method_header =
      oat_method.GetOatQuickMethodHeader();

  // Generate method name. No signature (although we might want to
  // revisit this depending on feedback from GWP folks)
  bool include_method_signature = false;
  std::string method_name = art::PrettyMethod(midx, dex_file,
                                              include_method_signature);

  // No code (Native method or abstract method)? Register
  // with visitor, then early return
  if (method_header == nullptr || method_header->GetCodeSize() == 0) {
    visitor.visitMethod(method_name, midx, 0,
                         nullptr, nullptr);
    return true;
  }

  //
  // Collect pertinent info on method (size, offset, etc)
  //
  uint32_t entry_point = oat_method.GetCodeOffset() -
      oat_header.GetExecutableOffset();
  entry_point &= ~0x1; // clean thumb2 bit (no-op for other architectures)
  uint64_t nativeCodeOffset = static_cast<uint64_t>(entry_point);
  uint32_t nativeCodeSize = method_header->GetCodeSize();
  uint32_t numDexInstrs = 0;
  if (code_item)
    numDexInstrs = code_item->insns_size_in_code_units_;

  // Pass info to visitor
  visitor.visitMethod(method_name, midx, numDexInstrs,
                       &nativeCodeOffset, &nativeCodeSize);
  return true;
}

static bool examineOatClass(const art::OatFile::OatClass &oat_class,
                            const art::DexFile &dex_file,
                            const art::OatHeader& oat_header,
                            uint32_t cidx,
                            OatDexVisitor &visitor)
{
  // Disposition of compile decision
  art::OatClassType cdisp = oat_class.GetType();

  if (cdisp != art::kOatClassAllCompiled &&
      cdisp != art::kOatClassSomeCompiled) {
    // not interesting
    return true;
  }

  // Collect class name
  const art::DexFile::ClassDef& class_def = dex_file.GetClassDef(cidx);
  const char* classDesc = dex_file.GetClassDescriptor(class_def);
  std::string className = art::PrettyDescriptor(classDesc);

  DEBUGLOG(("examining class %s", className.c_str()));

  const uint8_t* class_data = dex_file.GetClassData(class_def);
  if (class_data == nullptr) {
    visitor.visitClass(className.c_str(), 0);
    return true;
  }

  art::ClassDataItemIterator it(dex_file, class_data);
  uint32_t nMethods = it.NumDirectMethods() + it.NumVirtualMethods();
  visitor.visitClass(className.c_str(), nMethods);

  // Skip fields
  while (it.HasNextStaticField()) { it.Next(); }
  while (it.HasNextInstanceField()) { it.Next(); }

  // Visit methods.
  uint32_t midx = 0;
  for (; it.HasNextDirectMethod() || it.HasNextVirtualMethod(); it.Next()) {
    const art::OatFile::OatMethod& oat_method =
        oat_class.GetOatMethod(midx);
    if (!examineMethod(dex_file,
                       oat_header,
                       oat_method,
                       midx,
                       it.GetMethodCodeItem(),
                       visitor))
      return false;
    midx++;
  }
  return true;
}

static bool examineOatDexFile(const art::OatFile::OatDexFile* oat_dex_file,
                              const art::OatHeader& oat_header,
                              OatDexVisitor &visitor)
{
  std::string unused_errmsg;
  std::unique_ptr<const art::DexFile> dex_file =
      oat_dex_file->OpenDexFile(&unused_errmsg);
  if (!dex_file)
    return false;

  // tell visitor about dexfile
  const art::DexFile::Header& dex_header = dex_file->GetHeader();
  const unsigned char *c_sha1sig =
      reinterpret_cast<const unsigned char *>(&dex_header.signature_[0]);
  unsigned char *sha1sig =
      const_cast<unsigned char *>(c_sha1sig);
  visitor.visitDEX(sha1sig);

  // walk classes
  for (size_t cidx = 0; cidx < dex_file->NumClassDefs(); ++cidx) {
    const art::OatFile::OatClass oat_class =
        oat_dex_file->GetOatClass(cidx);
    if (!examineOatClass(oat_class, *dex_file, oat_header,
                         cidx, visitor))
      return false;
  }

  return true;
}

// Main entry point
bool examineOatFile(const std::string &path,
                    OatDexVisitor &visitor)
{
  art::Locks::Init();
  art::MemMap::Init();


  std::string unused_error_msg;
  std::unique_ptr<art::OatFile>
      oat_file(art::OatFile::Open(path,
                                  path,
                                  nullptr,
                                  nullptr,
                                  false,
                                  /*low_4gb*/false,
                                  nullptr,
                                  &unused_error_msg));
  if (!oat_file)
    return false;

  // TODO: needs to be filled in from "oatexec" symbol in OAT file
  // See https://android-review.googlesource.com/244911
  uint64_t base_text = 0; // oat_file.OatExec()

  const art::OatHeader& oat_header = oat_file->GetOatHeader();
  bool is_64Bit = Is64BitInstructionSet(oat_header.GetInstructionSet());

  // Tell visitor about this OAT
  visitor.visitOAT(is_64Bit,
                   oat_header.GetChecksum(),
                   oat_header.GetExecutableOffset(),
                   base_text);

  // Walk Dex files
  std::vector<const art::OatFile::OatDexFile*> oat_dex_files =
      oat_file->GetOatDexFiles();
  for (size_t dc = 0; dc < oat_dex_files.size(); dc++) {
    const art::OatFile::OatDexFile* oat_dex_file = oat_dex_files[dc];
    if (!oat_dex_file || !examineOatDexFile(oat_dex_file,
                                            oat_header,
                                            visitor)) {
      DEBUGLOG(("examineOatDexFile failed at iteration %d", (int)dc));
      return false;
    }
  }

  return true;
}
