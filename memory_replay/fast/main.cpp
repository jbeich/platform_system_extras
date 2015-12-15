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
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>

#include "./NativeInfo.h"
#include "MemoryReplay.h"
#include "ReplayParser.h"
#include "memreplay_fast.h"

static uint64_t nanotime() {
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<uint64_t>(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static void perform_dump(uint32_t line_number) {
  if (line_number == 0) {
    PrintNativeInfo("Initial ");
  } else {
    printf("  At line %" PRIu32 "\n", line_number);
    PrintNativeInfo("    ");
  }
}

// pread, except it automatically advances the offset.
ssize_t offset_read(int fd, void* dst, size_t len, off_t& offset) {
  size_t orig_len = len;
  while (len > 0) {
    int result = pread(fd, dst, len, offset);
    if (result == 0) {
      errx(1, "premature end of file");
    } else if (result < 0) {
      err(1, "read failed");
    }
    len -= result;
    offset += result;
  }
  return orig_len;
}

ssize_t xread(int fd, void* dst, size_t len) {
  size_t orig_len = len;
  while (len > 0) {
    int result = read(fd, dst, len);
    if (result == 0) {
      errx(1, "premature end of file");
    } else if (result < 0) {
      err(1, "read failed");
    }
    len -= result;
  }
  return orig_len;
}

void print_command(const command& cmd) {
  switch (cmd.type) {
    case command_type::malloc:
      printf("%d = malloc(%d)\n", cmd.arg1, cmd.arg2);
      break;

    case command_type::calloc:
      printf("%d = calloc(%d, %d)\n", cmd.arg1, cmd.arg2, cmd.arg3);
      break;

    case command_type::realloc:
      printf("%d = realloc(%d, %d)\n", cmd.arg1, cmd.arg1, cmd.arg2);
      break;

    case command_type::memalign:
      printf("%d = memalign(%d, %d)n", cmd.arg1, cmd.arg2, cmd.arg3);
      break;

    case command_type::free:
      printf("free(%d)\n", cmd.arg1);
      break;

    case command_type::local_sync:
      printf("sync(%d)\n", cmd.arg1);
      break;

    case command_type::dump:
      printf("dump(%d)\n", cmd.arg1);
      break;

    case command_type::thread_start:
      printf("thread_start(%d, %d)\n", cmd.arg1, cmd.arg2);
      break;

    case command_type::thread_exit:
      printf("thread_exit(%d, %d)\n", cmd.arg1, cmd.arg2);
      break;

    default:
      errx(1, "unknown command type: %d", int(cmd.type));
  }
}

template <typename T>
T* mmap_alloc(size_t count) {
  size_t required_size = count * sizeof(T);
  if (required_size % 4096) {
    required_size += 4096 - (required_size & 4095);
  }
  void* result =
    mmap(nullptr, required_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (result == MAP_FAILED) {
    err(1, "failed to allocate memory");
  }
  return reinterpret_cast<T*>(result);
}

struct barrier_info {
  // Barrier for the final threads.
  pthread_barrier_t* exit;

  // Array of thread pairwise sync barriers.
  pthread_barrier_t* local;

  // Array of global sync barriers.
  pthread_barrier_t* global;
};

struct thread_info {
  pthread_t thread;
  uint16_t thread_id;
  thread_info* thread_list;

  int fd;
  off_t fd_offset;

  void** allocations;
  barrier_info barriers;
  std::atomic<uint64_t>* allocation_time;
};

bool sync(pthread_barrier_t* barrier) {
  int rc = pthread_barrier_wait(barrier);
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
    errx(1, "sync failed: %s\n", strerror(errno));
  }
  return rc == PTHREAD_BARRIER_SERIAL_THREAD;
}

static void* thread_main(void* arg);
static void start_thread(thread_info* threads, uint32_t id) {
  pthread_attr_t thread_attr;
  if (pthread_attr_init(&thread_attr) != 0) {
    errx(1, "failed to initialize thread attributes");
  }

  if (pthread_attr_setstacksize(&thread_attr, PTHREAD_STACK_MIN) != 0) {
    errx(1, "failed to set stack size");
  }

  if (pthread_attr_setguardsize(&thread_attr, 0) != 0) {
    errx(1, "failed to set guard size");
  }

  pthread_create(&threads[id].thread, &thread_attr, thread_main, &threads[id]);
}

static void* thread_main(void* arg) {
  thread_info* info = reinterpret_cast<thread_info*>(arg);
  char buf[sizeof("memreplay-12345")];
  snprintf(buf, sizeof(buf), "mr-%d", info->thread_id);
  pthread_setname_np(info->thread, buf);

  int fd = info->fd;
  off_t fd_offset = info->fd_offset;

  thread_header thread;
  offset_read(fd, &thread, sizeof(thread), fd_offset);

  for (uint32_t i = 0; i < thread.command_count; ++i) {
    command cmd;
    offset_read(fd, &cmd, sizeof(cmd), fd_offset);

    uint64_t start = nanotime();
    void* result;
    switch (cmd.type) {
      case command_type::invalid:
        err(1, "malformed input");

      case command_type::malloc:
        if (info->allocations[cmd.arg1]) {
          errx(1, "malloc attempted to reuse allocation id %" PRIu32, cmd.arg1);
        }

        result = malloc(cmd.arg2);
        if (!result) {
          err(1, "allocation failed");
        }
        memset(result, 1, cmd.arg2);
        info->allocations[cmd.arg1] = result;
        break;

      case command_type::calloc:
        if (info->allocations[cmd.arg1]) {
          errx(1, "calloc attempted to reuse allocation id %" PRIu32, cmd.arg1);
        }

        result = calloc(cmd.arg2, cmd.arg3);
        if (!result) {
          err(1, "allocation failed");
        }
        memset(result, 1, cmd.arg2 * cmd.arg3);
        info->allocations[cmd.arg1] = result;
        break;

      case command_type::realloc: {
        result = realloc(info->allocations[cmd.arg1], cmd.arg2);
        if (!result) {
          err(1, "reallocation failed");
        }

        memset(result, 1, cmd.arg2);
        info->allocations[cmd.arg1] = result;
        break;
      }

      case command_type::memalign:
        if (info->allocations[cmd.arg1]) {
          errx(1, "memalign attempted to reuse allocation id %" PRIu32, cmd.arg1);
        }

        result = memalign(cmd.arg2, cmd.arg3);
        if (!result) {
          err(1, "allocation failed");
        }
        memset(result, 1, cmd.arg3);
        info->allocations[cmd.arg1] = result;
        break;

      case command_type::free:
        if (!info->allocations[cmd.arg1]) {
          errx(1, "attempted to free unused allocation id %" PRIu32, cmd.arg1);
        }

        free(info->allocations[cmd.arg1]);
        info->allocations[cmd.arg1] = nullptr;
        break;

      case command_type::local_sync:
        sync(&info->barriers.local[cmd.arg1]);
        continue;

      case command_type::dump: {
        pthread_barrier_t* dump_barrier = &info->barriers.global[cmd.arg1];

        // Wait on the barrier once to pick a thread to do the dump, then again to continue.
        if (sync(dump_barrier)) {
          perform_dump(cmd.arg2);
        }
        sync(dump_barrier);
        continue;
      }

      case command_type::thread_start: {
        pthread_barrier_t* barrier = &info->barriers.global[cmd.arg1];
        if (sync(barrier)) {
          start_thread(info->thread_list, cmd.arg2);
        }
        continue;
      }

      case command_type::thread_exit: {
        pthread_barrier_t* barrier = &info->barriers.global[cmd.arg1];
        sync(barrier);

        if (cmd.arg2 == info->thread_id) {
          pthread_exit(0);
        }
        continue;
      }
    }

    info->allocation_time->fetch_add(nanotime() - start);
  }

  sync(info->barriers.exit);
  pthread_exit(0);
}

void run_replay(int fd) {
  file_header header;
  xread(fd, &header, sizeof(header));
  if (memcmp(header.magic, "MEM_RPLY", sizeof(header.magic)) != 0) {
    errx(1, "invalid memory dump - magic number doesn't match");
  }

  printf("Thread count: %" PRIu32 "\n", header.thread_count);
  printf("Allocation count: %" PRIu32 "\n", header.allocation_count);
  printf("Local sync count: %" PRIu32 "\n", header.local_sync_count);
  printf("Global sync count: %" PRIu32 "\n", header.global_sync_count);
  printf("\n");

  void** allocations = mmap_alloc<void*>(header.allocation_count);
  thread_info* threads = mmap_alloc<thread_info>(header.thread_count);
  std::atomic<uint64_t> allocation_time(0);

  // Set up the barrriers.
  pthread_barrier_t exit_barrier;
  auto barriers = mmap_alloc<pthread_barrier_t>(header.local_sync_count + header.global_sync_count);
  pthread_barrier_t* local_sync_barriers = barriers;
  pthread_barrier_t* global_sync_barriers = barriers + header.local_sync_count;

  barrier_info barrier_info = {
    .exit = &exit_barrier, .local = local_sync_barriers, .global = global_sync_barriers
  };

  if (pthread_barrier_init(&exit_barrier, nullptr, header.final_thread_count + 1) != 0) {
    errx(1, "failed to create pthread_barrier");
  }

  for (uint32_t i = 0; i < header.local_sync_count; ++i) {
    if (pthread_barrier_init(&local_sync_barriers[i], nullptr, 2) != 0) {
      errx(1, "failed to create local pthread_barrier");
    }
  }

  for (uint32_t i = 0; i < header.global_sync_count; ++i) {
    uint32_t barrier_count;
    xread(fd, &barrier_count, sizeof(barrier_count));
    if (pthread_barrier_init(&global_sync_barriers[i], nullptr, barrier_count) != 0) {
      errx(1, "failed to create global pthread_barrier: %" PRIu32 " => %" PRIu32 "\n", i,
           barrier_count);
    }
  }

  for (uint32_t i = 0; i < header.thread_count; ++i) {
    thread_header thread;

    threads[i].thread_id = i;
    threads[i].thread_list = threads;
    threads[i].fd = fd;
    threads[i].fd_offset = lseek(fd, 0, SEEK_CUR);
    threads[i].allocations = allocations;
    threads[i].barriers = barrier_info;
    threads[i].allocation_time = &allocation_time;

    xread(fd, &thread, sizeof(thread));
#if 1
    lseek(fd, sizeof(command) * thread.command_count, SEEK_CUR);
#else
    printf("\nThread %d\n", i);
    for (uint32_t j = 0; j < thread.command_count; ++j) {
      command cmd;
      xread(fd, &cmd, sizeof(cmd));
      print_command(cmd);
    }
#endif
  }

  perform_dump(0);

  // Start the first thread.
  start_thread(threads, 0);
  pthread_barrier_wait(&exit_barrier);

  for (uint32_t i = 0; i < header.thread_count; ++i) {
    pthread_join(threads[i].thread, nullptr);
  }

  PrintNativeInfo("Final ");
  uint64_t total_time = allocation_time.load();
  printf("Total Allocation/Free Time: %" PRIu64 "ns %0.2fs\n", total_time,
         total_time / (1000.0 * 1000.0 * 1000.0));
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("usage: %s <filename>\n", argv[0]);
    exit(0);
  }

  printf("Processing: %s\n", argv[1]);
  int fd = open(argv[1], O_RDONLY);

  char buf[8];
  if (pread(fd, buf, sizeof(buf), 0) != sizeof(buf)) {
    err(1, "failed to read file header");
  }

  if (memcmp("MEM_RPLY", buf, sizeof(buf)) != 0) {
    printf("Magic number didn't match, attempting to preprocess text replay.\n");
    FILE* temp = tmpfile();

    int tempfd = fileno(temp);

    int pid = fork();
    if (pid < 0) {
      err(1, "failed to fork");
    }

    if (pid == 0) {
      MemoryReplay replay = ReplayParser::Parse(fd);
      replay.WriteDump(tempfd);
      close(fd);
      exit(0);
    }

    if (dup2(tempfd, fd) < 0) {
      err(1, "failed to dup temporary file");
    }

    int status;
    if (wait(&status) < 0) {
      err(1, "failed to wait on parser process");
    }

    if (WEXITSTATUS(status) != 0) {
      err(1, "failed to preprocess memory dump");
    }

    lseek(fd, 0, SEEK_SET);
    printf("Done preprocessing text replay.\n");
  }

  run_replay(fd);
}
