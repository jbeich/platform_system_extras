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

//
// Main entry point invoked by the perf.data converter.
//
// Examines the specified load module to see if it is an OAT file. If
// it is, checksum/mapping info is harvested from the OAT file and
// loadmodule is augmented with the new info. Returns "true" if the
// loadmodule is indeed an OAT file and the post-processing succeeded.
//
bool postprocess_oatfile(const char *loadmodulepath,
                         wireless_android_play_playlog::LoadModule &loadmodule);


//
// Exposed for unit testing. Here "oatpath" is a path to an OAT file
// of interest (expected to exist and be readable), "oatmapcachepath"
// is a path where to the oat mapping file, "cksumcachepath" is a path
// to the oatinfo cache file, and "oatinfo" is the actual data to be
// returned.
//
bool collect_oatfile_checksums(const char *oatpath,
                               const char *oatmapcachepath,
                               const char *cksumcachepath,
                 wireless_android_play_playlog::OatFileInfo &oatinfo);


#endif
