/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "read_elf.h"

#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "util.h"

template <typename NhdrType>
static bool GetBuildIdFromNoteSegment(FILE* fp, size_t segment_size, BuildId& build_id) {
  while (segment_size >= sizeof(NhdrType)) {
    NhdrType nhdr;
    if (fread(&nhdr, sizeof(nhdr), 1, fp) != 1) {
      perror("fread");
      return false;
    }
    size_t namesz = ALIGN(nhdr.n_namesz, 4);
    size_t descsz = ALIGN(nhdr.n_descsz, 4);

    if (segment_size < sizeof(nhdr) + namesz + descsz) {
      return false;
    }
    segment_size -= sizeof(nhdr) + namesz + descsz;

    unsigned char buf[namesz + descsz];
    if (fread(buf, namesz + descsz, 1, fp) != 1) {
      perror("fread");
      return false;
    }

    if ((nhdr.n_type == NT_GNU_BUILD_ID) && (memcmp(buf, "GNU", 4) == 0)) {
      std::fill(build_id.begin(), build_id.end(), 0);
      size_t copy_size = std::min(build_id.size(), descsz);
      std::copy_n(buf + namesz, copy_size, build_id.begin());
      return true;
    }
  }
  return true;
}

bool GetBuildIdFromNoteFile(const char* filename, BuildId& build_id) {
  FILE* fp = fopen(filename, "rb");
  if (fp == nullptr) {
    perror("fopen");
    return false;
  }
  // Elf64_Nhdr and Elf32_Nhdr are the same, so we can use either one.
  bool result = GetBuildIdFromNoteSegment<Elf64_Nhdr>(fp, UINT_MAX, build_id);
  fclose(fp);
  return result;
}

template <typename HdrType, typename PhdrType, typename NhdrType>
static bool GetBuildId(FILE* fp, unsigned char* e_ident, BuildId& build_id) {
  HdrType hdr;

  memcpy(&hdr.e_ident[0], e_ident, EI_NIDENT);

  // First read the rest of the header.
  if (fread(reinterpret_cast<unsigned char*>(&hdr) + EI_NIDENT, sizeof(HdrType) - EI_NIDENT, 1, fp) != 1) {
    return false;
  }

  for (size_t i = 0; i < hdr.e_phnum; ++i) {
    PhdrType phdr;
    if (fseek(fp, hdr.e_phoff + i * hdr.e_phentsize, SEEK_SET) != 0) {
      return false;
    }
    if (fread(&phdr, sizeof(phdr), 1, fp) != 1) {
      return false;
    }
    if (phdr.p_type == PT_NOTE) {
      if (fseek(fp, phdr.p_offset, SEEK_SET) != 0) {
        return false;
      }
      if (GetBuildIdFromNoteSegment<NhdrType>(fp, phdr.p_filesz, build_id)) {
        return true;
      }
    }
  }
  return false;
}

bool GetBuildIdFromElfFile(const char* filename, BuildId& build_id) {
  FILE* fp = fopen(filename, "rb");
  if (fp == nullptr) {
    return false;
  }

  unsigned char e_ident[EI_NIDENT];
  if (fread(e_ident, SELFMAG, 1, fp) != 1) {
    return false;
  }
  if (memcmp(e_ident, ELFMAG, SELFMAG) != 0) {
    return false;
  }
  if (fread(e_ident + SELFMAG, EI_NIDENT - SELFMAG, 1, fp) != 1) {
    return false;
  }

  bool result = false;
  if (e_ident[EI_CLASS] == ELFCLASS32) {
    result = GetBuildId<Elf32_Ehdr, Elf32_Phdr, Elf32_Nhdr>(fp, e_ident, build_id);
  } else if (e_ident[EI_CLASS] == ELFCLASS64) {
    result = GetBuildId<Elf64_Ehdr, Elf64_Phdr, Elf64_Nhdr>(fp, e_ident, build_id);
  }
  fclose(fp);
  return result;
}
