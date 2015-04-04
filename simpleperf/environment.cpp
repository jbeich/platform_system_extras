#include "environment.h"

#include <asm/unistd.h>
#include <ctype.h>
#include <linux/perf_event.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

std::vector<int> Environment::GetOnlineCpus() {
  std::vector<int> result;
  FILE* fp = fopen("/sys/devices/system/cpu/online", "r");
  if (fp != nullptr) {
    char buf[1024];
    if (fgets(buf, sizeof(buf), fp) != nullptr) {
      bool have_dash = false;
      bool have_cpu = false;
      int cpu = 0;
      for (char* p = buf; *p != '\0'; ++p) {
        if (isdigit(*p)) {
          if (!have_cpu) {
            cpu = *p - '0';
            have_cpu = true;
          } else {
            cpu = cpu * 10 + *p - '0';
          }
        } else {
          if (have_cpu) {
            if (have_dash) {
              for (int t = result.back() + 1; t < cpu; ++t) {
                result.push_back(t);
              }
              have_dash = false;
            }
            result.push_back(cpu);
            have_cpu = false;
          }
          if (*p == '-') {
            have_dash = true;
          } else {
            have_dash = false;
          }
        }
      }
      if (have_cpu) {
        if (have_dash) {
          for (int t = result.back() + 1; t < cpu; ++t) {
            result.push_back(t);
          }
        }
        result.push_back(cpu);
      }
    }
  }
  fclose(fp);
  return result;
}

int64_t Environment::NanoTime() {
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<int64_t>(t.tv_sec) * 1000000000 + t.tv_nsec;
}
