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

#ifndef SYSTEM_EXTRAS_PERFPROFD_QUIPPER_ZIP_INSPECTOR_H
#define SYSTEM_EXTRAS_PERFPROFD_QUIPPER_ZIP_INSPECTOR_H


#include <string>
#include <map>

//
// A class to help with reading ELF files that are part of
// *.apk or *.zip archives.
//
class ZipInspector {

 public:
  ZipInspector() {}
  ~ZipInspector() {}

  struct ZipElfEntry {
    std::string zip_entry_name;
    size_t offset; // file offset in zip file
    size_t esize;  // size of ELF file in zip

    ZipElfEntry() : offset(0) { }
    ZipElfEntry(const std::string &ename,
                size_t off,
                size_t sz)
        : zip_entry_name(ename)
        , offset(off)
        , esize(sz) { }
  };

  // Does the specified offset within the zipfile correspond to an
  // uncompressed ELF file?
  ZipElfEntry *FindElfInZipByMmapOffset(std::string zipfile_path,
                                        size_t mmap_offset);

 private:
  struct ZipMmapInfo {
    std::string zipfile_path;
    size_t mmap_offset;
    ZipMmapInfo() : mmap_offset(0) { }
    ZipMmapInfo(const std::string &zpath,
                size_t offset)
        : zipfile_path(zpath)
        , mmap_offset(offset)
    { }

    // for stl set/map use
    bool operator<(const ZipMmapInfo& other) const {
      if (zipfile_path != other.zipfile_path)
        return zipfile_path < other.zipfile_path;
      return mmap_offset < other.mmap_offset;
    }
  };

  std::map<ZipMmapInfo, ZipElfEntry> cache_;
};

#endif
