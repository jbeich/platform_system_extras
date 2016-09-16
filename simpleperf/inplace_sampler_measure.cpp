#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <thread>
#include <vector>

constexpr int LOOP_COUNT = 500000000;

void FunctionRecursiveTwo(int loop);

void FunctionRecursiveOne(int loop) {
  for (volatile int i = 0; i < LOOP_COUNT; ++i) {
  }
  if (loop >= 0) {
    FunctionRecursiveTwo(loop);
  }
}

void FunctionRecursiveTwo(int loop) {
  for (volatile int i = 0; i < LOOP_COUNT; ++i) {
  }
  if (loop > 0) {
    FunctionRecursiveOne(loop - 1);
  }
}

static inline uint64_t GetSystemClock() {
  timespec ts;
  // Assume clock_gettime() doesn't fail.
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(int argc, char** argv) {
  int thread_num = 0;
  if (argc == 2) {
    thread_num = atoi(argv[1]);
  }
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; ++i) {
    threads.emplace_back([]() {FunctionRecursiveOne(10); } );
  }
  printf("pid = %d\n", getpid());
  uint64_t start = GetSystemClock();
  FunctionRecursiveOne(10);
  for (int i = 0; i < thread_num; ++i) {
    threads[i].join();
  }
  uint64_t end = GetSystemClock();
  printf("time cost is %f ns\n", (end - start) / 1000000000.0);
  return 0;
}
