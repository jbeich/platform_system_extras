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

#include "read_apk.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <map>

#include <android-base/file.h>
#include <android-base/logging.h>

#include "read_elf.h"
#include "utils.h"
#include "ziparchive/zip_archive.h"

bool IsValidJarOrApkPath(const std::string& filename) {
  static const char zip_preamble[] = {0x50, 0x4b, 0x03, 0x04 };
  if (!IsRegularFile(filename)) {
    return false;
  }
  FILE* fp = fopen(filename.c_str(), "reb");
  if (fp == nullptr) {
    return false;
  }
  char buf[4];
  if (fread(buf, 4, 1, fp) != 1) {
    fclose(fp);
    return false;
  }
  fclose(fp);
  return memcmp(buf, zip_preamble, 4) == 0;
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class ArchiveHelper {
 public:
  explicit ArchiveHelper(int fd) : valid_(false) {
    int rc = OpenArchiveFd(fd, "", &handle_, false);
    if (rc == 0) {
      valid_ = true;
    }
  }
  ~ArchiveHelper() {
    if (valid_) {
      CloseArchive(handle_);
    }
  }
  bool valid() const { return valid_; }
  ZipArchiveHandle &archive_handle() { return handle_; }

 private:
  ZipArchiveHandle handle_;
  bool valid_;
};

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline static std::string unpack_zipstring(const ZipString& zstring)
{
  std::string result;
  result.resize(zstring.name_length,'\0');
  memcpy(&result[0], zstring.name, zstring.name_length);
  result[zstring.name_length] = '\0';
  return result;
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// First component of pair is APK file path, second is offset into APK
typedef std::pair<std::string, size_t> ApkOffset;

class ApkInspectorImpl {
 public:
  EmbeddedElf *FindElfInApkByMmapOffset(std::string apk_path,
                                        size_t mmap_offset);
 private:
  std::map<ApkOffset, EmbeddedElf> cache_;
};

EmbeddedElf *ApkInspectorImpl::FindElfInApkByMmapOffset(std::string apk_path,
                                                        size_t mmap_offset)
{
  // Already in cache?
  ApkOffset ami(apk_path, mmap_offset);
  auto it = cache_.find(ami);
  if (it != cache_.end()) {
    return &it->second;
  }

  // Crack open the apk(zip) file and take a look.
  if (! IsValidJarOrApkPath(apk_path)) {
    return nullptr;
  }
  FileHelper fhelper(apk_path.c_str());
  if (fhelper.fd() == -1) {
    return nullptr;
  }

  ArchiveHelper ahelper(fhelper.fd());
  if (! ahelper.valid()) {
    return nullptr;
  }
  ZipArchiveHandle &handle = ahelper.archive_handle();

  // Iterate through the zip file. Look for a zip entry corresponding
  // to an uncompressed blob whose range intersects with the mmap
  // offset we're interested in.
  void* iteration_cookie;
  if (StartIteration(handle, &iteration_cookie, NULL, NULL) < 0) {
    return nullptr;
  }
  ZipEntry entry;
  ZipString name;
  bool found = false;
  int zrc;
  off64_t mmap_off64 = mmap_offset;
  while ((zrc = Next(iteration_cookie, &entry, &name)) == 0) {
    if (entry.method == kCompressStored &&
        mmap_off64 >= entry.offset &&
        mmap_off64 < entry.offset + entry.uncompressed_length) {
      // Found.
      found = true;
      break;
    }
  }
  EndIteration(iteration_cookie);
  if (!found) {
    return nullptr;
  }

  // We found something in the zip file at the right spot. Is it an ELF?
  if (lseek(fhelper.fd(), entry.offset, SEEK_SET) != entry.offset) {
    LOG(ERROR) << "malformed archive in " << apk_path << " (lseek failed)";
    return nullptr;
  }
  if (! IsValidElfFile(fhelper.fd())) {
    return nullptr;
  }

  // Elf found: add entry containing info on embedded elf file to cache
  std::string entry_name(unpack_zipstring(name));
  EmbeddedElf ee(apk_path, entry_name, entry.offset, entry.uncompressed_length);
  cache_[ami] = ee;
  return &(cache_[ami]);
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

ApkInspector::ApkInspector()
    : impl_(new ApkInspectorImpl())
{
}

ApkInspector::~ApkInspector()
{
}

EmbeddedElf *ApkInspector::FindElfInApkByMmapOffset(std::string apk_path,
                                                    size_t mmap_offset)
{
  return impl_->FindElfInApkByMmapOffset(apk_path, mmap_offset);
}
