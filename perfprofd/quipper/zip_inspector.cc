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

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

#include <base/file.h>

#include "zip_inspector.h"
#include "ziparchive/zip_archive.h"

class FileHelper {
 public:
  explicit FileHelper(const char *filename) : fd_(-1) {
    fd_ = TEMP_FAILURE_RETRY(open(filename, O_RDONLY | O_CLOEXEC));
  }
  int fd() const { return fd_; };
  ~FileHelper() {
    if (fd_ != -1) close(fd_);
  }

 private:
  int fd_;
};

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

static std::string unpack_zipstring(const ZipString& zstring)
{
  std::string result;
  result.resize(zstring.name_length+1);
  memcpy(&result[0], zstring.name, zstring.name_length);
  result[zstring.name_length] = '\0';
  return result;
}

ZipInspector::ZipElfEntry *
ZipInspector::FindElfInZipByMmapOffset(std::string zipfile_path,
                                         size_t mmap_offset)
{
  //
  // Already in cache?
  //
  ZipMmapInfo zmi(zipfile_path, mmap_offset);
  auto it = cache_.find(zmi);
  if (it != cache_.end()) {
    return &it->second;
  }

  //
  // Crack open the zip file and take a look.
  //
  FileHelper fhelper(zipfile_path.c_str());
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
  while ((zrc = Next(iteration_cookie, &entry, &name)) == 0) {
    if (entry.method == kCompressStored &&
        mmap_offset >= entry.offset &&
        mmap_offset < entry.offset + entry.uncompressed_length) {
      // Found.
      found = true;
      break;
    }
  }
  EndIteration(iteration_cookie);
  if (!found) {
    return nullptr;
  }

  // Unpack name
  std::string entry_name(unpack_zipstring(name));

  //
  // We found something in the zip file at the right spot. Is it an ELF?
  // [Read 32-bit header even for elf64, since we are only looking at ident]
  //
  Elf32_Ehdr hdr;
  ssize_t rc = TEMP_FAILURE_RETRY(pread(fhelper.fd(), &hdr, sizeof(hdr), entry.offset));
  if (rc < 0 || rc != sizeof(hdr) || memcmp(ELFMAG, hdr.e_ident, SELFMAG) != 0) {
    return nullptr;
  }

  //
  // All systems go: add entry containing info on embedded elf file to cache
  //
  ZipElfEntry zee(entry_name, entry.offset, entry.uncompressed_length);
  cache_[zmi] = zee;
  return &(cache_[zmi]);
}
