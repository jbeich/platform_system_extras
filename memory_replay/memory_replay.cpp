#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stack>
#include <vector>

#include "GetPss.h"

#define MAX_THREADS 1024

enum alloc_e {
  TYPE_MALLOC,
  TYPE_FREE,
  TYPE_CALLOC,
  TYPE_REALLOC,
  TYPE_MEMALIGN,
  TYPE_THREAD_DONE,
};

struct malloc_t {
  size_t size;
};

struct calloc_t {
  size_t n_elements;
  size_t elem_size;
};

struct realloc_t {
  size_t size;
  void* old_pointer;
};

struct memalign_t {
  size_t align;
  size_t size;
};

struct alloc_data_t {
  alloc_e type;
  void* pointer;
  union {
    malloc_t malloc_d;
    calloc_t calloc_d;
    realloc_t realloc_d;
    memalign_t memalign_d;
  } d;
};

class Pointers {
public:
  Pointers() : mutex_(PTHREAD_MUTEX_INITIALIZER) {}
  virtual ~Pointers() {}

  void Allocate(alloc_data_t* alloc) {
    switch (alloc->type) {
    case TYPE_FREE:
      if (alloc->pointer) {
        Free(alloc->pointer);
      }
      break;
    case TYPE_MALLOC:
      {
      void* ptr = AddAllocation(malloc(alloc->d.malloc_d.size), alloc->pointer);
      if (ptr != nullptr) {
        memset(ptr, 0, alloc->d.malloc_d.size);
      }
      }
      break;
    case TYPE_CALLOC:
      {
      void* ptr = AddAllocation(calloc(alloc->d.calloc_d.n_elements, alloc->d.calloc_d.elem_size), alloc->pointer);
      if (ptr != nullptr) {
        memset(ptr, 0, alloc->d.calloc_d.n_elements * alloc->d.calloc_d.elem_size);
      }
      }
      break;
    case TYPE_REALLOC:
      {
      void* ptr = Realloc(alloc->d.realloc_d.old_pointer, alloc->d.realloc_d.size, alloc->pointer);
      if (ptr != nullptr) {
        memset(ptr, 0, alloc->d.realloc_d.size);
      }
      }
      break;
    case TYPE_MEMALIGN:
      {
      void* ptr = AddAllocation(memalign(alloc->d.memalign_d.align, alloc->d.memalign_d.size), alloc->pointer);
      if (ptr != nullptr) {
        memset(ptr, 0, alloc->d.memalign_d.size);
      }
      }
      break;
    default:
      printf("Unknown type %d\n", alloc->type);
    }
  }

private:
  void* AddAllocation(void* pointer, void* alloc_pointer) {
    if (pointer == nullptr) {
      printf("Failed to allocate a pointer.\n");
      exit(1);
    }
    pthread_mutex_lock(&mutex_);
    if (free_list_.empty()) {
      ptrs_.push_back(pointer);
      value_ptrs_.push_back(alloc_pointer);
    } else {
      size_t idx = free_list_.top();
      free_list_.pop();
      ptrs_[idx] = pointer;
      value_ptrs_[idx] = alloc_pointer;
    }
    pthread_mutex_unlock(&mutex_);

    return pointer;
  }

  size_t Find(void* value_ptr, bool locked = false) {
    if (!locked) {
      pthread_mutex_lock(&mutex_);
    }
    for (size_t i = 0; i < value_ptrs_.size(); i++) {
      if (value_ptr == value_ptrs_[i]) {
        if (!locked) {
          pthread_mutex_unlock(&mutex_);
        }
        return i;
      }
    }
    if (!locked) {
      pthread_mutex_unlock(&mutex_);
    }
    printf("Cannot find pointer %p\n", value_ptr);
    exit(1);
  }

  void Free(void* value_ptr) {
    pthread_mutex_lock(&mutex_);
    size_t idx = Find(value_ptr, true);
    free_list_.push(idx);
    free(ptrs_[idx]);
    ptrs_[idx] = nullptr;
    value_ptrs_[idx] = nullptr;
    pthread_mutex_unlock(&mutex_);
  }

  void* Realloc(void* old_value_ptr, size_t size, void* value_ptr) {
    void* old_pointer = nullptr;
    size_t old_idx = 0;
    if (old_pointer) {
      old_idx = Find(old_value_ptr);
      old_pointer = ptrs_[old_idx];
    }
    void* ptr = AddAllocation(realloc(old_pointer, size), value_ptr);
    if (old_pointer != nullptr && value_ptr != nullptr) {
      // The old pointer value needs to be cleared out.
      pthread_mutex_lock(&mutex_);
      free_list_.push(old_idx);
      free(ptrs_[old_idx]);
      ptrs_[old_idx] = nullptr;
      pthread_mutex_unlock(&mutex_);
    }
    return ptr;
  }

  std::vector<void*> ptrs_;
  std::vector<void*> value_ptrs_;

  pthread_mutex_t mutex_;

  std::stack<size_t> free_list_;
};

class ThreadData {
public:
  ThreadData() : ptrs_(nullptr), mutex_(PTHREAD_MUTEX_INITIALIZER), tid_(0), pending_(false) {
    pthread_cond_init(&cond_, nullptr);
  }
  virtual ~ThreadData() {
    pthread_cond_destroy(&cond_);
  }

  void WaitForReady() {
    pthread_mutex_lock(&mutex_);
    while (pending_) {
      pthread_cond_wait(&cond_, &mutex_);
    }
    pthread_mutex_unlock(&mutex_);
  }

  void WaitForPending() {
    pthread_mutex_lock(&mutex_);
    while (!pending_) {
      pthread_cond_wait(&cond_, &mutex_);
    }
    pthread_mutex_unlock(&mutex_);
  }

  void SetPending() {
    pthread_mutex_lock(&mutex_);
    pending_ = true;
    pthread_mutex_unlock(&mutex_);
    pthread_cond_signal(&cond_);
  }

  void ClearPending() {
    pthread_mutex_lock(&mutex_);
    pending_ = false;
    pthread_mutex_unlock(&mutex_);
    pthread_cond_signal(&cond_);
  }

  alloc_data_t* alloc_data() { return &alloc_data_; }
  Pointers* ptrs() { return ptrs_; }
  void set_ptrs(Pointers* ptrs) { ptrs_ = ptrs; }

  pid_t tid() { return tid_; }
  void set_tid(pid_t tid) { tid_ = tid; }

private:
  Pointers* ptrs_;
  pthread_mutex_t mutex_;
  pthread_cond_t cond_;
  alloc_data_t alloc_data_;
  pid_t tid_;
  bool pending_;
};

void* alloc_thread(void* data) {
  ThreadData* thread_data = reinterpret_cast<ThreadData*>(data);

  Pointers* ptrs = thread_data->ptrs();
  while (true) {
    thread_data->WaitForPending();
    if (thread_data->alloc_data()->type == TYPE_THREAD_DONE) {
      thread_data->set_tid(0);
      thread_data->ClearPending();
      return nullptr;
    }
    ptrs->Allocate(thread_data->alloc_data());
    thread_data->ClearPending();
  }
  return nullptr;
}

class Threads {
public:
  Threads() : num_threads_(0) {}
  virtual ~Threads() {}

  ThreadData* CreateThread(pid_t tid, Pointers* ptrs) {
    if (num_threads_ == MAX_THREADS) {
      printf("Too many threads created.\n");
      exit(1);
    }
    data_[num_threads_].set_ptrs(ptrs);
    data_[num_threads_].set_tid(tid);
    if (pthread_create(&threads_[num_threads_], nullptr, alloc_thread, &data_[num_threads_]) != 0) {
      printf("Failed to create thread %d: %s\n", tid, strerror(errno));
      exit(1);
    }
    return &data_[num_threads_++];
  }

  ThreadData* GetThread(pid_t tid) {
    for (size_t i = 0; i < num_threads_; i++) {
      if (data_[i].tid() == tid) {
        return &data_[i];
      }
    }
    return nullptr;
  }

  void WaitForCompletion() {
    for (size_t i = 0; i < num_threads_; i++) {
      data_[i].WaitForReady();
    }
  }

private:
  pid_t tids_[MAX_THREADS];
  pthread_t threads_[MAX_THREADS];
  ThreadData data_[MAX_THREADS];

  size_t num_threads_;
};

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Requires one argument.\n");
    return 1;
  }
  FILE* dump_file = fopen(argv[1], "r");
  if (dump_file == nullptr) {
    printf("Failed to open %s\n", argv[1]);
    return 1;
  }

  Threads threads;
  char type[128];
  pid_t tid;
  void* ptr;
  size_t line_number = 0;
  Pointers ptrs;
  while (fscanf(dump_file, "%d: %s %p", &tid, type, &ptr) > 0) {
    line_number++;
    if ((line_number % 10000) == 0) {
      printf("Processing at line %zu\n", line_number);
    }
    ThreadData* thread_data = threads.GetThread(tid);
    if (thread_data == nullptr) {
      thread_data = threads.CreateThread(tid, &ptrs);
      printf("Creating thread %d\n", tid);
    }

    thread_data->WaitForReady();
    alloc_data_t* data = thread_data->alloc_data();
    data->pointer = ptr;
    if (strcmp(type, "malloc") == 0) {
      data->type = TYPE_MALLOC;
      fscanf(dump_file, " %zu", &data->d.malloc_d.size);
    } else if (strcmp(type, "free") == 0) {
      // We need to make sure that all other threads have processed their
      // allocations.
      threads.WaitForCompletion();
      data->type = TYPE_FREE;
    } else if (strcmp(type, "calloc") == 0) {
      data->type = TYPE_CALLOC;
      fscanf(dump_file, " %zu %zu", &data->d.calloc_d.n_elements, &data->d.calloc_d.elem_size);
    } else if (strcmp(type, "realloc") == 0) {
      data->type = TYPE_REALLOC;
      fscanf(dump_file, " %p %zu", &data->d.realloc_d.old_pointer, &data->d.realloc_d.size);
    } else if (strcmp(type, "memalign") == 0) {
      data->type = TYPE_MEMALIGN;
      fscanf(dump_file, " %zu %zu", &data->d.memalign_d.align, &data->d.memalign_d.size);
    } else if (strcmp(type, "thread_done") == 0) {
      data->type = TYPE_THREAD_DONE;
      printf("Killing thread %d\n", thread_data->tid());
    } else {
      printf("Unknown type %s\n", type);
      return 1;
    }
    thread_data->SetPending();
  }
  fclose(dump_file);

  threads.WaitForCompletion();

  printf("PSS bytes: %zu\n", GetPssBytes());
}
