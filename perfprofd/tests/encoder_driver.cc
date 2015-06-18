/*
**
** Copyright 2015, The Android Open Source Project
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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "perf_data_converter.h"

inline char* string_as_array(std::string* str) {
  return str->empty() ? NULL : &*str->begin();
}

int main(int argc, char** argv)
{
  int ac;
  int rc = 0;

  // All arguments are assumed to be perf.data files
  for (ac = 1; ac < argc; ++ac) {
    std::string pfile(argv[ac]);

    // read and encode
    const wireless_android_play_playlog::AndroidPerfProfile &encodedProfile =
        wireless_android_logging_awp::RawPerfDataToAndroidPerfProfile(pfile);
    if (encodedProfile.programs().size() == 0) {
      fprintf(stderr, "error: failed to read input perf.data file %s\n", argv[ac]);
      rc = 1;
      continue;
    }

    // serialize
    int size = encodedProfile.ByteSize();
    std::string data;
    data.resize(size);
    ::google::protobuf::uint8* dtarget =
          reinterpret_cast<::google::protobuf::uint8*>(string_as_array(&data));
    encodedProfile.SerializeWithCachedSizesToArray(dtarget);

    // emit to *.encoded file
    std::string outfile(argv[ac]);
    outfile += ".encoded";
    FILE *fp = fopen(outfile.c_str(), "w");
    if (!fp) {
      fprintf(stderr, "error: unable to open %s for writing\n",
              outfile.c_str());
      perror("");
      rc = 1;
      continue;
    }
    size_t fsiz = size;
    if (fwrite(dtarget, fsiz, 1, fp) != 1) {
      fprintf(stderr, "error: fwrite failed writing to %s\n", outfile.c_str());
      perror("");
    } else {
      fprintf(stderr, "... emitted %s.encoded\n", outfile.c_str());
    }
    fclose(fp);
  }

  return rc;
}
