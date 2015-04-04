#include "util.h"

#include <ctype.h>
#include <time.h>

std::vector<std::string> SplitString(const char* s) {
  std::vector<std::string> args;
  std::string arg;

  for (const char* p = s; *p != '\0'; ++p) {
    if (isspace(*p)) {
      if (arg.size() > 0) {
        args.push_back(arg);
        arg.clear();
      }
    } else {
      arg.push_back(*p);
    }
  }
  if (arg.size() > 0) {
    args.push_back(arg);
  }
  return args;
}

int64_t NanoTime() {
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<int64_t>(t.tv_sec) * 1000000000 + t.tv_nsec;
}
