/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
