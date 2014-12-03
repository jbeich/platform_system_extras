/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

// This is an extremely simplified version of libpagemap.

#define _BITS(x, offset, bits) (((x) >> offset) & ((1LL << (bits)) - 1))

#define PAGEMAP_PRESENT(x)     (_BITS(x, 63, 1))
#define PAGEMAP_SWAPPED(x)     (_BITS(x, 62, 1))
#define PAGEMAP_SHIFT(x)       (_BITS(x, 55, 6))
#define PAGEMAP_PFN(x)         (_BITS(x, 0, 55))
#define PAGEMAP_SWAP_OFFSET(x) (_BITS(x, 5, 50))
#define PAGEMAP_SWAP_TYPE(x)   (_BITS(x, 0,  5))

static bool ReadData(int fd, unsigned long place, uint64_t *data) {
  if (lseek(fd, place * sizeof(uint64_t), SEEK_SET) < 0) {
    return false;
  }
  if (read(fd, (void*)data, sizeof(uint64_t)) != (ssize_t)sizeof(uint64_t)) {
    return false;
  }
  return true;
}

static char g_line[8192];

size_t GetPssBytes() {
  int maps_fd = open("/proc/self/maps", O_RDONLY);
  if (maps_fd < 0) {
    printf("Failed to open /proc/self/maps %s\n", strerror(errno));
    exit(1);
  }

  int pagecount_fd = open("/proc/kpagecount", O_RDONLY);
  if (pagecount_fd < 0) {
    printf("Failed to open /proc/kpagecount %s\n", strerror(errno));
    exit(1);
  }

  int pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  if (pagemap_fd < 0) {
    printf("Failed to open /proc/self/pagemap %s\n", strerror(errno));
    exit(1);
  }

  size_t total_pss = 0;
  int pagesize = getpagesize();

  g_line[0] = '\0';
  char* line = g_line;
  size_t cur_processed = 0;
  ssize_t bytes_read = 0;
  while (true) {
    char* newline = strchr(line, '\n');
    if (newline) {
      *newline = '\0';
      cur_processed += newline - line + 1;
    } else {
      if (bytes_read - cur_processed > 0) {
        memmove(g_line, g_line + cur_processed, bytes_read - cur_processed);
        cur_processed = bytes_read - cur_processed;
      }
      bytes_read = read(maps_fd, g_line+cur_processed, sizeof(g_line) - cur_processed);
      if (bytes_read <= 0) {
        break;
      }
      line = g_line;
      bytes_read += cur_processed;
      cur_processed = 0;
      continue;
    }
    uintptr_t start, end;
    if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " ", &start, &end) != 2) {
      total_pss = 0;
      break;
    }
    for (size_t page = start/pagesize; page < end/pagesize; page++) {
      uint64_t data;
      if (ReadData(pagemap_fd, page, &data)) {
        if (PAGEMAP_PRESENT(data) && !PAGEMAP_SWAPPED(data)) {
          uint64_t count;
          if (ReadData(pagecount_fd, PAGEMAP_PFN(data), &count)) {
            total_pss += (count >= 1) ? pagesize / count : 0;
          }
        }
      }
    }
    line = g_line + cur_processed;
  }

  close(maps_fd);
  close(pagecount_fd);
  close(pagemap_fd);

  return total_pss;
}
