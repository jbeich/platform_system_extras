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

#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void WriteLog(FILE* fp, const char* fmt, va_list ap) {
  vfprintf(fp, fmt, ap);
  fputc('\n', fp);
  fflush(fp);
}

void LogError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  WriteLog(stderr, fmt, ap);
  va_end(ap);
}

void LogInfo(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  WriteLog(stdin, fmt, ap);
  va_end(ap);
}

void LogErrorWithErrno(const char* fmt, ...) {
  int err = errno;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, " (system error message: %s)\n", strerror(err));
}

void PrintIndented(size_t indent, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("%*s", static_cast<int>(indent), "");
  vprintf(fmt, ap);
  va_end(ap);
}
