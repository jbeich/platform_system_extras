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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int64_t NanoTime() {
  timespec ts;
  ts.tv_sec = ts.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char** argv) {
  int nop_sec = 1;
  if (argc == 2) {
    int tmp = atoi(argv[1]);
    if (tmp != 0) {
      nop_sec = tmp;
    }
  }
  int64_t start_time = NanoTime();
  int64_t expected_end_time = start_time + nop_sec * 1000000000LL;
  while (NanoTime() < expected_end_time) {
    int i = 0;
    while (++i != 10000000) {
    }
  }
}
