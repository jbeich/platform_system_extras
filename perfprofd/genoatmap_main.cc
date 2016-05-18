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

//
// This stand-alone executable examines an input OAT file (supplied
// as command line argument) and writes out a mapping file (encoded
// proto) that allows a client to map OAT text addresses to locations
// in the DEX files used to compile the OAT.
//

#include "genoatmap.h"

#include <unistd.h>

inline char* string_as_array(std::string* str) {
  return str->empty() ? NULL : &*str->begin();
}

int main(int argc, char **argv) {

  const char *me = argv[0];
  if (argc != 3) {
    fprintf(stderr, "%s: usage: %s <oatfile> <outputfile>\n", me, me);
    return 1;
  }

  const char *oatfile = argv[1];
  if (access(argv[1], R_OK) == -1) {
    fprintf(stderr, "%s: no read permission for %s\n", me, oatfile);
    return 1;
  }

  oatmap::MapOatFile mapfile;
  bool success = genmap_for_oat(oatfile, mapfile);
  if (!success) {
    fprintf(stderr, "%s: genmap_for_oat call failed on %s\n", me, oatfile);
    return 1;
  }

  const char *outfile = argv[2];
  FILE *fp = fopen(outfile, "w");
  if (!fp) {
    fprintf(stderr, "%s: fopen of %s for writing failed\n", me, outfile);
  }

  //
  // Serialize protobuf to array
  //
  int size = mapfile.ByteSize();
  std::string data;
  data.resize(size);
  ::google::protobuf::uint8* dtarget =
        reinterpret_cast<::google::protobuf::uint8*>(string_as_array(&data));
  mapfile.SerializeWithCachedSizesToArray(dtarget);

  //
  // Write result to file
  //
  size_t fsiz = size;
  if (fwrite(dtarget, fsiz, 1, fp) != 1) {
    fprintf(stderr, "%s: fwrite to %s failed\n", me, outfile);
    fclose(fp);
    return 1;
  }
  fclose(fp);

  return 0;
}
