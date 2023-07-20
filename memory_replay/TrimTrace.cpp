
#include <err.h>
#include <inttypes.h>
#include <stdio.h>

#include <set>
#include <string>

#include <android-base/file.h>
#include <android-base/strings.h>

#include "AllocParser.h"
#include "File.h"

static void Parse(const char* filename, uint64_t threshold) {
  std::string contents;
  if (android::base::EndsWith(filename, ".zip")) {
    fprintf(stderr, "Unzipping contents of file...\n");
    contents = ZipGetContents(filename);
  } else if (!android::base::ReadFileToString(filename, &contents)) {
    fprintf(stderr, "Reading contents of file...\n");
    errx(1, "Unable to get contents of %s", filename);
  }
  if (contents.empty()) {
    errx(1, "Contents of file %s is empty.", filename);
  }

  std::set<uint64_t> ptrs;
  size_t start_str = 0;
  size_t end_str = 0;
  fprintf(stderr, "Processing file...\n");
  while (true) {
    end_str = contents.find('\n', start_str);
    if (end_str == std::string::npos) {
      break;
    }
    contents[end_str] = '\0';
    AllocEntry entry {};
    AllocGetData(&contents[start_str], &entry);
    start_str = end_str + 1;
    switch (entry.type) {
    case MALLOC:
      if (entry.size >= threshold) {
        ptrs.insert(entry.ptr);
        printf("1: malloc 0x%" PRIx64 " %" PRId64 "\n", entry.ptr, entry.size);
      }
      break;
    case CALLOC: {
      uint64_t total_size = entry.size * entry.u.n_elements;
      if (total_size >= threshold) {
        printf("1: calloc 0x%" PRIx64 " %" PRId64 " %" PRId64 "\n", entry.ptr, entry.u.n_elements, entry.size);
      }
      break;
    }
    case MEMALIGN:
      if (entry.size >= threshold) {
        ptrs.insert(entry.ptr);
        printf("1: memalign 0x%" PRIx64 " %" PRId64 " %" PRId64 "\n", entry.ptr, entry.u.align, entry.size);
      }
      break;
    case REALLOC:
      if (ptrs.count(entry.u.old_ptr) != 0) {
        if (entry.size >= threshold) {
          printf("1: realloc 0x%" PRIx64 " %" PRIx64 " %" PRId64 "\n", entry.u.old_ptr, entry.ptr, entry.size);
        } else {
          printf("1: free 0x%" PRIx64 "\n", entry.u.old_ptr);
          ptrs.erase(entry.u.old_ptr);
        }
      } else if (entry.size >= threshold) {
        printf("1: malloc 0x%" PRIx64 " %" PRId64 "\n", entry.ptr, entry.size);
      }
      break;
    case FREE:
      if (ptrs.count(entry.ptr) != 0) {
        printf("1: free 0x%" PRIx64 "\n", entry.ptr);
        ptrs.erase(entry.ptr);
      }
      break;
    case THREAD_DONE:
      break;
    }
  }
  fprintf(stderr, "Finished processing file.\n");
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Requires a single argument.\n");
    return 1;
  }

  Parse(argv[1], 65535);
}
