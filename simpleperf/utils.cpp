#include "utils.h"

#include <stdarg.h>
#include <stdio.h>

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

void PrintWithSpace(int space, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("%*s", space, "");
  vprintf(fmt, ap);
  va_end(ap);
}
