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

#ifndef SYSTEM_EXTRAS_PERFPROFD_OATMAPPER_H_
#define SYSTEM_EXTRAS_PERFPROFD_OATMAPPER_H_

#include "perf_profile.pb.h"

typedef enum {
  OAT_ADDR_RAW=0,      // 0 - unencoded (raw text address)
  OAT_ADDR_METHOD=1,   // 1 - map text addr to dex method ID
  OAT_ADDR_DEXOP=2     // 2 - map text addr to dex bytecode ID (where possible)
} OatAddressEncoding;

using wireless_android_play_playlog::LoadModule;
using wireless_android_play_playlog::OatFileInfo;
using wireless_android_play_playlog::AndroidPerfProfile;

class OatMapperImpl;

class OatMapper {
 public:
  OatMapper(const char *cachedir);
  ~OatMapper();

  //
  // Top level OAT file post-processing hook. Takes care of detecting
  // any load modules that are OAT files (augmenting the proto entries
  // for them with checksum information if needed) and remapping text
  // addresses to an artificial DEX address space.
  //
  void postprocess_encoded_profile(AndroidPerfProfile &prof);

  //
  // Exposed for unit testing.
  //
  // Here "oatpath" is a path to an OAT file of interest (expected to
  // exist and be readable) and "oatinfo" is the actual data to be
  // returned.
  //
  bool collect_oatfile_checksums(const char *oatpath,
                                 OatFileInfo &oatinfo);

  //
  // Exposed for unit testing.
  //
  // Given a load module path and text address, encode the address for
  // logging using the specified encoding method if the load module is
  // an OAT file (if not, then the address is simply returned as is).
  //
  uint64_t encode_addr(const char *loadmodulepath,
                       OatAddressEncoding encoding,
                       uint64_t addr);

  //
  // Exposed for unit testing.
  //
  // Return cache path for specific OAT.
  //
  std::string cachepath(const char *oatfilepath);

 private:
  std::unique_ptr<OatMapperImpl> impl_;
};

#endif
