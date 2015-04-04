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
