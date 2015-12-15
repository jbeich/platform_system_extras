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
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "MemoryReplay.h"
#include "ReplayParser.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("usage: %s <input file> <output file>\n", argv[0]);
    return 1;
  }

  ReplayParser parser;
  int infd = open(argv[1], O_RDONLY);
  if (infd < 0) {
    err(1, "failed to open input file '%s'", argv[1]);
  }

  printf("Preprocessing memory replay '%s'\n", argv[1]);

  MemoryReplay replay = ReplayParser::Parse(infd, 100000);
  close(infd);

  int outfd = open(argv[2], O_RDWR | O_TRUNC | O_CREAT, 0755);
  if (outfd < 0) {
    err(1, "failed to open output file '%s'", argv[2]);
  }

  printf("Writing memory dump to '%s'\n", argv[2]);
  replay.WriteDump(outfd);
  close(outfd);

  printf("Successfully preprocessed %s\n", argv[1]);
}
