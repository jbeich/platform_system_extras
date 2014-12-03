/*
** Copyright 2010 The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
 * Micro-benchmarking of sleep/cpu speed/memcpy/memset/memory reads/strcmp.
 */

#include <inttypes.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "GetPss.h"
#include "NanoTime.h"

static void* g_values[1000000];

void alloc_test(size_t alloc_size, size_t num_allocs) {
  size_t start_pss_bytes = GetPssBytes();
  printf("Starting PSS %zu bytes %fK %fM\n", start_pss_bytes, start_pss_bytes/1024.0, start_pss_bytes/1024.0/1024.0);
  struct mallinfo start_mi = mallinfo();
  printf("Starting mapped %zu allocated %zu\n", start_mi.usmblks, start_mi.uordblks);
  printf("Starting diff %zu\n", start_pss_bytes - start_mi.uordblks);
  for (size_t i = 0; i < num_allocs; i++) {
    g_values[i] = malloc(alloc_size);
    memset(g_values[i], 0, alloc_size);
    size_t pss_bytes = GetPssBytes();
    struct mallinfo mi = mallinfo();
    printf("  %zu:%zu PSS %zu bytes %fK %fM\n", i, alloc_size, pss_bytes, pss_bytes/1024.0, pss_bytes/1024.0/1024.0);
    printf("    mapped %zu allocated %zu\n", mi.usmblks, mi.uordblks);
    printf("    diff %zu\n", pss_bytes - mi.uordblks);
    printf("\n");
    printf("    diff PSS %zu\n", pss_bytes - start_pss_bytes);
    printf("    diff mapped %zu allocated %zu\n", mi.usmblks - start_mi.usmblks, mi.uordblks - start_mi.uordblks);
    printf("\n");
  }

  for (int i = num_allocs-1; i >= 0; i--) {
    free(g_values[i]);
    size_t pss_bytes = GetPssBytes();
    struct mallinfo mi = mallinfo();
    printf("  %d:%zu PSS %zu bytes %fK %fM\n", i, alloc_size, pss_bytes, pss_bytes/1024.0, pss_bytes/1024.0/1024.0);
    printf("    mapped %zu allocated %zu\n", mi.usmblks, mi.uordblks);
    printf("    diff %zu\n", pss_bytes - mi.uordblks);
    printf("\n");
    printf("    diff PSS %zu\n", pss_bytes - start_pss_bytes);
    printf("    diff mapped %zu allocated %zu\n", mi.usmblks - start_mi.usmblks, mi.uordblks - start_mi.uordblks);
    printf("\n");
  }
}

#if 0
void time_malloc_free(size_t alloc_size, size_t keep_allocs) {
  void* allocs = new void*[keep_allocs];
  for (size_t i = 0; i < keep_allocs; i++) {
    allocs[i] = 
  }
}
#endif

struct thread_arg_t {
  size_t alloc_size;
  size_t num_iterations;
  size_t num_allocations;
  bool do_memset;
};

struct thread_data_t {
  pthread_t thread;
  bool* ready;
  pthread_cond_t* ready_cond;
  pthread_mutex_t* ready_mutex;
  size_t thread_num;

  thread_arg_t args;
};

void* ThreadAlloc(void* data) {
  thread_data_t* thread_data = reinterpret_cast<thread_data_t*>(data);

  pthread_mutex_lock(thread_data->ready_mutex);
  while (!*thread_data->ready) {
    pthread_cond_wait(thread_data->ready_cond, thread_data->ready_mutex);
  }
  pthread_mutex_unlock(thread_data->ready_mutex);

  size_t alloc_size = thread_data->args.alloc_size;
  bool do_memset = thread_data->args.do_memset;
  size_t num_iterations = thread_data->args.num_iterations;
  uint64_t start, total;
  size_t num_allocations = thread_data->args.num_allocations;
  size_t total_iterations;
  if (num_allocations == 1) {
    total_iterations = num_iterations;

    start = NanoTime();
    for (size_t i = 0; i < num_iterations; i++) {
      void* alloc = malloc(alloc_size);
      if (alloc == NULL) {
        fprintf(stderr, "alloc failed\n");
      }
      if (do_memset) {
        memset(alloc, 1, alloc_size);
      }
      free(alloc);
    }
    total = NanoTime() - start;
  } else {
    void** allocs = new void*[num_allocations];
    total_iterations = num_iterations * num_allocations;

    start = NanoTime();
    for (size_t i = 0; i < num_iterations; i++) {
      for (size_t j = 0; j < num_allocations; j++) {
        allocs[j] = malloc(alloc_size);
        if (allocs[j] == NULL) {
          fprintf(stderr, "alloc failed\n");
        }
        if (do_memset) {
          memset(allocs[j], 1, alloc_size);
        }
      }
      for (size_t j = 0; j < num_allocations; j++) {
        free(allocs[j]);
      }
    }
    total = NanoTime() - start;
    delete[] allocs;
  }
  printf("  %zu: %fns per malloc/free, total %" PRIu64 "ns\n",
         thread_data->thread_num, total/double(total_iterations), total);

  return NULL;
}

void TimeThreadedAlloc(size_t num_threads, thread_arg_t* args) {
  thread_data_t* thread_data = new thread_data_t[num_threads];
  bool ready = false;
  pthread_cond_t ready_cond;
  pthread_cond_init(&ready_cond, NULL);
  pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;

  for (size_t i = 0; i < num_threads; i++) {
    thread_data[i].ready_cond = &ready_cond;
    thread_data[i].ready_mutex = &ready_mutex;
    thread_data[i].ready = &ready;
    thread_data[i].thread_num = i;
    thread_data[i].args = *args;
    int ret = pthread_create(&thread_data[i].thread, NULL, ThreadAlloc, thread_data + i);
    if (ret != 0) {
      fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
      exit(1);
    }
  }

  sleep(2);
  pthread_mutex_lock(&ready_mutex);
  ready = true;
  pthread_mutex_unlock(&ready_mutex);

  pthread_cond_broadcast(&ready_cond);

  // Wait for the threads to complete.
  for (size_t i = 0; i < num_threads; i++) {
    int ret = pthread_join(thread_data[i].thread, NULL);
    if (ret != 0) {
      fprintf(stderr, "pthread_join failed: %s\n", strerror(ret));
      exit(1);
    }
  }

  pthread_cond_destroy(&ready_cond);
  delete[] thread_data;
}

#if 0
struct test_arg_t {
  
};

struct test_t {
  const char* test_name;
  size_t num_args;
  test_arg_t* args;
};

static test_t g_tests[] = {
  {
    "thread",
    3,
  },
};
#endif

int main(int argc, char** argv) {
  if (argc != 5 && argc != 6) {
    fprintf(stderr, "Bad number of args\n");
    return 1;
  }

  size_t num_threads = atoi(argv[1]);
  thread_arg_t args;
  args.alloc_size = atoi(argv[2]);
  args.num_iterations = atoi(argv[3]);
  args.num_allocations = atoi(argv[4]);
  if (argc == 6) {
    args.do_memset = true;
  } else {
    args.do_memset = false;
  }
  printf("num_threads %zu\n", num_threads);
  printf("alloc_size = %zu\n", args.alloc_size);
  printf("num_iterations = %zu\n", args.num_iterations);
  printf("num_allocations = %zu\n", args.num_allocations);
  printf("do_memset = %s\n", (args.do_memset) ? "true" : "false");
  TimeThreadedAlloc(num_threads, &args);
#if 0
  if (argc != 3) {
    printf("Bad number of args\n");
    return 1;
  }
  size_t alloc_size = atoi(argv[1]);
  size_t num_allocs = atoi(argv[2]);
  alloc_test(alloc_size, num_allocs);
#endif
}
