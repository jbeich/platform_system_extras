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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Alloc.h"
#include "File.h"
#include "NativeInfo.h"
#include "Pointers.h"
#include "Thread.h"
#include "Threads.h"

constexpr size_t kDefaultMaxThreads = 512;

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    if (argc > 3) {
      fprintf(stderr, "Only two arguments are expected.\n");
    } else {
      fprintf(stderr, "Requires at least one argument.\n");
    }
    fprintf(stderr, "Usage: %s MEMORY_LOG_FILE [MAX_THREADS]\n", basename(argv[0]));
    fprintf(stderr, "  MEMORY_LOG_FILE\n");
    fprintf(stderr, "    This can either be a text file or a zipped text file.\n");
    fprintf(stderr, "  MAX_THREADs\n");
    fprintf(stderr, "    The maximum number of threads in the trace. The default is %zu.\n",
            kDefaultMaxThreads);
    fprintf(stderr, "    This pre-allocates the memory for thread data to avoid allocating\n");
    fprintf(stderr, "    while the trace is being replayed.\n");
    return 1;
  }

#if defined(__LP64__)
  NativePrintf("64 bit environment.\n");
#else
  NativePrintf("32 bit environment.\n");
#endif

#if defined(__BIONIC__)
  NativePrintf("Setting decay time to 1\n");
  mallopt(M_DECAY_TIME, 1);
#endif

  size_t max_threads = kDefaultMaxThreads;
  if (argc == 3) {
    max_threads = atoi(argv[2]);
  }

  AllocEntry* entries;
  size_t num_entries;
  GetUnwindInfo(argv[1], &entries, &num_entries);

  NativePrintf("Processing: %s\n", argv[1]);

  ProcessDump(entries, num_entries, max_threads);

  FreeEntries(entries, num_entries);

  return 0;
}
