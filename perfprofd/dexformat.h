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

#ifndef SYSTEM_EXTRAS_PERFPROFD_DEXFORMAT_H_
#define SYSTEM_EXTRAS_PERFPROFD_DEXFORMAT_H_

//
// Structure templates and constants for reading DEX files, modeled
// after similar code in ART.
//

//
// DEX file header
//
static constexpr uint32_t EndianConstant = 0x12345678;
static constexpr uint32_t ReverseEndianConst = 0x78563412;
static constexpr unsigned char DexMagic[4] = {0x64, 0x65, 0x78, 0x0a};
static constexpr unsigned char DexVersion35[4] = {0x30, 0x33, 0x35, 0x00};
static constexpr unsigned char DexVersion37[4] = {0x30, 0x33, 0x37, 0x00};

// https://source.android.com/devices/tech/dalvik/dex-format.html#header-item

typedef struct {
  unsigned char magic[4];
  unsigned char version[4];
  uint32_t      checksum;
  unsigned char sha1sig[20];
  uint32_t      filesize;
  uint32_t    headersize;
  uint32_t     endiantag;
  uint32_t      linksize;
  uint32_t       linkoff;
  uint32_t        mapoff;
  uint32_t stringidssize;
  uint32_t  stringidsoff;
  uint32_t   typeidssize;
  uint32_t    typeidsoff;
  uint32_t  protoidssize;
  uint32_t   protoidsoff;
  uint32_t  pieldidssize;
  uint32_t   fieldidsoff;
  uint32_t methodidssize;
  uint32_t  methodidsoff;
  uint32_t classdefssize;
  uint32_t  classdefsoff;
  uint32_t      datasize;
  uint32_t       dataoff;
} DexFileHeader;

typedef struct {
  uint16_t classidx;
  uint16_t typeidx;
  uint32_t nameidx;
} DexMethodIdItem;

// https://source.android.com/devices/tech/dalvik/dex-format.html#class-def-item
typedef struct {
  uint32_t classidx;
  uint32_t accessflags;
  uint32_t superclassidx;
  uint32_t interfacesoff;
  uint32_t sourcefilesidx;
  uint32_t annotationsoff;
  uint32_t classdataoff;
  uint32_t staticvaluesoff;
} DexClassHeader;

//
// Note that within the DEX file, these fields are ULEB128 encoded; the
// struct below is to hold the decoded values. More at
// https://source.android.com/devices/tech/dalvik/dex-format.html#class-data-item
//
typedef struct {
  uint32_t numStaticFields;
  uint32_t numInstanceFields;
  uint32_t numDirectMethods;
  uint32_t numVirtualMethods;
} DexClassContents;

#endif // SYSTEM_EXTRAS_PERFPROFD_DEXFORMAT_H_
