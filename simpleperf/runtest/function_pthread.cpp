#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

constexpr int LOOP_COUNT = 100000000;

void* ChildThreadFunction(void*) {
  for (volatile int i = 0; i < LOOP_COUNT; ++i) {
  }
  return nullptr;
}

void MainThreadFunction() {
  for (volatile int i = 0; i < LOOP_COUNT; ++i) {
  }
}

int main() {
  pthread_t thread;
  int ret = pthread_create(&thread, nullptr, ChildThreadFunction, nullptr);
  if (ret != 0) {
    fprintf(stderr, "pthread_create failed, ret = %d\n", ret);
    exit(1);
  }
  MainThreadFunction();
  ret = pthread_join(thread, nullptr);
  if (ret != 0) {
    fprintf(stderr, "pthread_join failed, ret = %d\n", ret);
    exit(1);
  }
  return 0;
}
