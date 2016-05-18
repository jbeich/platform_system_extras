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

#ifndef SYSTEM_EXTRAS_PERFPROFD_OATFORMAT_H_
#define SYSTEM_EXTRAS_PERFPROFD_OATFORMAT_H_

//
// Structure templates and constants for reading OAT files, modeled
// after similar code in ART.
//

//
// Oat file header
//
typedef struct {
  uint8_t oatmagic[4];
  uint8_t oatversion[4];
  uint32_t adler32_checksum;
  uint32_t instruction_set;
  uint32_t instruction_set_features_bitmap;
  uint32_t dex_file_count;
  uint32_t executable_offset;
  uint32_t interpreter_to_interpreter_bridge_offset;
  uint32_t interpreter_to_compiled_code_bridge_offset;
  uint32_t jni_dlsym_lookup_offset;
  uint32_t quick_generic_jni_trampoline_offset;
  uint32_t quick_imt_conflict_trampoline_offset;
  uint32_t quick_resolution_trampoline_offset;
  uint32_t quick_to_interpreter_bridge_offset;
  int32_t image_patch_delta;
  uint32_t image_file_location_oat_checksum;
  uint32_t image_file_location_oat_data_begin;
  uint32_t key_value_store_size;
  uint8_t key_value_store[0];
} OatFileHeader;

//
// Expected version and magic strings
//
static constexpr unsigned char OatVersion[] = { '0', '7', '9', '\0' };
static constexpr unsigned char OatMagic[] = { 'o', 'a', 't', '\n' };

typedef enum {
  OatDispAllCompiled=0,
  OatDispSomeCompiled=1,
  OatDispNoneCompiled=2,
  OatDispMax=3,
} OatClassDisposition;

typedef struct {
  uint32_t vmapTableOffset;
  uint32_t frameSizeInBytes;
  uint32_t coreSpillMask;
  uint32_t fpSpillMask;
  uint32_t codeSizeInBytes;
} OatPreMethodHeader;

#endif // SYSTEM_EXTRAS_PERFPROFD_OATFORMAT_H_
