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


#include "dexread.h"
#include "dexformat.h"
#include "perfprofdutils.h"

#define DEBUGLOG W_ALOGD

#include <sstream>

class DexReader {
 public:
  DexReader(unsigned char *dex_pointer,
            unsigned char *limit,
            OatDexVisitor &visitor)
      : dexdata_(dex_pointer)
      , limit_(limit)
      , visitor_(visitor) {}

  bool walkDEX();

 private:
  unsigned char *dexdata_;
  unsigned char *limit_;
  OatDexVisitor &visitor_;
  DexFileHeader header_;
  DexMethodIdItem *methodids_;
  DexClassHeader *classheaders_;
  uint32_t *typeids_;
  uint32_t *stringids_;

 private:

  bool IsWordAlignedPtr(unsigned char *ptr) {
    uint64_t val64 = reinterpret_cast<uint64_t>(ptr);
    return (val64 & 0x3 ? false : true);
  }

  template<typename T>
  bool FormWordAlignedPointer(uint32_t offset, T * &result) {
    if (offset & 0x3)
      return false;
    if (dexdata_ + offset > limit_)
      return false;
    result = reinterpret_cast<T *>(dexdata_ + offset);
    return true;
  }

  bool getULEB128(unsigned char * &data, uint32_t &val) {
    uint32_t shift = 0, res = 0;
    unsigned char * ptr = data;
    while(true) {
      if (ptr > limit_)
        return false;
      uint32_t byte = *(ptr++);
      res += ((byte & 0x7f) << shift);
      if ((byte & 0x80) == 0)
        break;
      shift += 7;
    }
    val = res;
    data = ptr;
    return true;
  }

  bool validateHeader() {
    if (limit_ <= dexdata_)
      return false;
    if (limit_ - dexdata_ < sizeof(DexFileHeader))
      return false;
    if (! IsWordAlignedPtr(dexdata_))
      return false;
    memcpy(&header_, dexdata_, sizeof(DexFileHeader));
    if (memcmp(header_.magic, DexMagic, sizeof(DexMagic)))
      return false;
    if (memcmp(header_.version, DexVersion35, sizeof(DexVersion35)) != 0 &&
        memcmp(header_.version, DexVersion37, sizeof(DexVersion37)) != 0)
      return false;
    // not sure how this could ever happen
    if (header_.endiantag != EndianConstant)
      return false;
    return true;
  }

  bool unpackIds() {
    if (! FormWordAlignedPointer(header_.stringidsoff, stringids_))
      return false;
    W_ALOGD("%d strings", header_.stringidssize);
    if (! FormWordAlignedPointer(header_.typeidsoff, typeids_))
      return false;
    W_ALOGD("%d typeids", header_.typeidssize);
    if (! FormWordAlignedPointer(header_.methodidsoff, methodids_))
      return false;
    W_ALOGD("%d methodids", header_.methodidssize);
    return true;
  }

  bool getString(uint32_t strIdx,
                 const char * &result) {
    if (strIdx >= header_.stringidssize)
      return false;
    uint32_t stroff = stringids_[strIdx];
    unsigned char *strdata = dexdata_ + stroff;
    if (strdata >= limit_)
      return false;
    // bypass size, we don't need it
    uint32_t unused;
    if (! getULEB128(strdata, unused))
      return false;
    result = reinterpret_cast<const char *>(strdata);
    return true;
  }

  bool getMethodIdItem(uint32_t midx, DexMethodIdItem * &result) {
    if (midx >= header_.methodidssize)
      return false;
    result = &methodids_[midx];
    return true;
  }

  bool getClassHeader(unsigned clIdx, DexClassHeader * &result) {
    if (clIdx >= header_.classdefssize)
      return false;
    result = &classheaders_[clIdx];
    return true;
  }

  bool getStringIdFromTypeId(uint32_t typeIdx, uint32_t &result) {
    if (typeIdx >= header_.typeidssize)
      return false;
    result = typeids_[typeIdx];
    return true;
  }

  //
  // For the rules on how type descriptors are encoded, see
  // https://source.android.com/devices/tech/dalvik/dex-format.html#typedescriptor
  //
  std::string decodeTypeDescriptor(const char *desc)
  {
    std::stringstream ss;
    const char *d = desc;

    // count array dimensions
    unsigned nDims = 0;
    for (; *d == '['; ++d) {
      nDims += 1;
    }

    if (*d == 'L') {
      // reference
      for (; *d && *d != ';'; ++d) {
        ss << (*d == '/' ? '.' : *d);
      }
    } else {
      // primitive
      switch (*d) {
        case 'B': ss << "byte";
        case 'C': ss << "char";
        case 'D': ss << "double";
        case 'F': ss << "float";
        case 'I': ss << "int";
        case 'J': ss << "long";
        case 'S': ss << "short";
        case 'Z': ss << "boolean";
        case 'V': ss << "void";
        default:
          // something went wrong, punt...
          ss << desc;
      }
    }
    for (unsigned dim = 0; dim < nDims; ++dim) {
      ss << "[]";
    }
    return ss.str();
  }


  bool getClassName(unsigned cl, std::string &result) {
    DexClassHeader *clhdr;
    if (! getClassHeader(cl, clhdr))
      return false;
    return getClassName(clhdr, result);
  }

  bool getClassName(DexClassHeader *clhdr, std::string &result) {
    unsigned sidx;
    if (! getStringIdFromTypeId(clhdr->classidx, sidx))
      return false;
    const char *str;
    if (! getString(sidx, str))
      return false;
    result = decodeTypeDescriptor(str);
    return true;
  }

  bool examineMethod(uint32_t methodIdx, uint64_t codeOffset) {
    DexMethodIdItem *mitem;
    if (! getMethodIdItem(methodIdx, mitem)) {
      W_ALOGD("bad lookup for method %d", methodIdx);
      return false;
    }
    const char *str;
    if (! getString(mitem->nameidx, str)) {
      W_ALOGD("bad string lookup for str %d", mitem->nameidx);
      return false;
    }
    visitor_.visitMethod(std::string(str), methodIdx, codeOffset);
    return true;
  }

  // See here for more info:
  // https://source.android.com/devices/tech/dalvik/dex-format.html#class-data-item
  bool examineClass(unsigned clIdx) {
    DexClassHeader *chdr;
    if ( ! getClassHeader(clIdx, chdr))
      return false;

    std::string clname;
    if (! getClassName(chdr, clname))
      return false;

    // in theory this can happen
    if (chdr->classdataoff == 0) {
      visitor_.visitClass(clname, 0);
      return true;
    }

    unsigned char *cldata = dexdata_ + chdr->classdataoff;
    uint32_t numStaticFields;
    uint32_t numInstanceFields;
    uint32_t numDirectMethods;
    uint32_t numVirtualMethods;
    if (! getULEB128(cldata, numStaticFields) ||
        ! getULEB128(cldata, numInstanceFields) ||
        ! getULEB128(cldata, numDirectMethods) ||
        ! getULEB128(cldata, numVirtualMethods)) {
      W_ALOGD("cldata method/field info decode failed");
      return false;
    }

    uint32_t numMethods = numDirectMethods + numVirtualMethods;
    W_ALOGD("class %s has %d methods", clname.c_str(), numMethods);
    visitor_.visitClass(clname, numMethods);

    // Bypass field info, which we're not really interested in
    unsigned nFields = numStaticFields + numInstanceFields;
    for (unsigned fi = 0; fi < nFields; ++fi) {
      uint32_t fieldStuff;
      if (! getULEB128(cldata, fieldStuff))
        return false;
    }

    //
    // Examine the methods. Note that method ID value read is a
    // difference from the index of the previous element in the list.
    //
    uint64_t midx = 0;
    W_ALOGD("starting method data read");
    for (uint32_t mc = 0; mc < numMethods; ++mc) {
      uint32_t mDelta;
      if (! getULEB128(cldata, mDelta))
        return false;
      if (mc == 0 || mc == numDirectMethods)
        midx = mDelta;
      else
        midx = midx + mDelta;

      uint32_t unused;
      if (! getULEB128(cldata, unused)) // read accessFlags
        return false;

      uint32_t codeOffset;
      if (! getULEB128(cldata, codeOffset))
        return false;

      if (! examineMethod(midx, static_cast<uint64_t>(codeOffset)))
        return false;
    }

    return true;
  }

  bool examineClasses() {
    unsigned nClasses = header_.classdefssize;
    if (! FormWordAlignedPointer(header_.classdefsoff, classheaders_))
      return false;
    for (unsigned cl = 0; cl < nClasses; ++cl) {
      W_ALOGD("walking class %u", cl);
      if (!examineClass(cl))
        return false;
    }
    return true;
  }

};

bool DexReader::walkDEX()
{
  W_ALOGD("validating header");
  if (!validateHeader())
    return false;
  W_ALOGD("visit for sha");
  visitor_.visitDEX(header_.sha1sig);
  if (!unpackIds())
    return false;
  W_ALOGD("about to walk classes");
  if (!examineClasses())
    return false;
  return true;
}

bool examineOatDexFile(unsigned char *dex_data,
                       unsigned char *limit,
                       OatDexVisitor &visitor)
{
  DexReader reader(dex_data, limit, visitor);
  return reader.walkDEX();
}
