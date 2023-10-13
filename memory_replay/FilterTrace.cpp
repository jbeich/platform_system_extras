/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <limits>
#include <string_view>
#include <unordered_map>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "AllocParser.h"
#include "File.h"

static void Usage() {
  fprintf(
      stderr,
      "Usage: %s [--min_size SIZE] [--max_size SIZE] [--print_trace_format] [--help] TRACE_FILE\n",
      android::base::Basename(android::base::GetExecutablePath()).c_str());
  fprintf(stderr, "  --min_size SIZE\n");
  fprintf(stderr, "      Display all allocations that are greater than or equal to SIZE\n");
  fprintf(stderr, "  --max_size SIZE\n");
  fprintf(stderr, "      Display all allocations that are less than or equal to SIZE\n");
  fprintf(stderr, "  --print_trace_format\n");
  fprintf(stderr, "      Display all allocations from the trace in the trace format\n");
  fprintf(stderr, "  --help\n");
  fprintf(stderr, "      Display this usage message\n");
  fprintf(stderr, "  TRACE_FILE\n");
  fprintf(stderr, "      The name of the trace file to filter\n");
  fprintf(stderr, "\n  Display all of the allocations from the trace file that meet the filter\n");
  fprintf(stderr, "  criteria. By default, without changing the min size or max size, all\n");
  fprintf(stderr, "  allocations in the trace will be printed.\n");
}

bool ParseOptions(const std::vector<std::string_view>& args,
                  std::unordered_map<std::string_view, std::string_view>& values) {
  std::string_view trace;
  for (auto it = args.begin(); it != args.end(); ++it) {
    if (*it == "--min_size" || *it == "--max_size") {
      if (it + 1 == args.end()) {
        fprintf(stderr, "%s requires an argument.\n", it->data());
        return false;
      }
      values[*it] = *(it + 1);
      ++it;
    } else if (*it == "--print_trace_format") {
      values[*it] = "set";
    } else if (*it == "--help") {
      return false;
    } else if (android::base::StartsWith(*it, "-")) {
      fprintf(stderr, "Unknown option %s\n", it->data());
      return false;
    } else if (trace != "") {
      fprintf(stderr, "Only one trace file allowed, unknown argument %s\n", it->data());
      return false;
    } else {
      trace = *it;
    }
  }
  if (trace == "") {
    fprintf(stderr, "No trace file passed on command line.\n");
    return false;
  }

  values["trace"] = trace;
  return true;
}

static void PrintEntry(const AllocEntry& entry, size_t size, bool print_trace_format) {
  if (print_trace_format) {
    switch (entry.type) {
      case REALLOC:
        if (entry.u.old_ptr == 0) {
          // Convert to a malloc since it is functionally the same.
          printf("%d: malloc %p %zu\n", entry.tid, reinterpret_cast<void*>(entry.ptr), entry.size);
        } else {
          printf("%d: realloc %p %p %zu\n", entry.tid, reinterpret_cast<void*>(entry.ptr),
                 reinterpret_cast<void*>(entry.u.old_ptr), entry.size);
        }
        break;
      case MALLOC:
        printf("%d: malloc %p %zu\n", entry.tid, reinterpret_cast<void*>(entry.ptr), entry.size);
        break;
      case MEMALIGN:
        printf("%d: memalign %p %zu %zu\n", entry.tid, reinterpret_cast<void*>(entry.ptr),
               entry.u.align, entry.size);
        break;
      case CALLOC:
        printf("%d: calloc %p %zu %zu\n", entry.tid, reinterpret_cast<void*>(entry.ptr),
               entry.u.n_elements, entry.size);
        break;
      default:
        break;
    }
  } else {
    printf("%s size %zu\n", entry.type == REALLOC && entry.u.old_ptr != 0 ? "realloc" : "alloc",
           size);
  }
}

static void ProcessTrace(const std::string_view& trace, size_t min_size, size_t max_size,
                         bool print_trace_format) {
  AllocEntry* entries;
  size_t num_entries;
  GetUnwindInfo(trace.data(), &entries, &num_entries);

  if (!print_trace_format) {
    if (min_size > 0) {
      printf("Scanning for allocations > %zu", min_size);
    } else {
      printf("Scanning for allocations");
    }
    if (max_size != std::numeric_limits<size_t>::max()) {
      printf(" %s< than %zu", min_size > 0 ? "and " : "", max_size);
    }
    printf("\n");
  }
  size_t total_allocs = 0;
  size_t total_reallocs = 0;
  for (size_t i = 0; i < num_entries; i++) {
    const AllocEntry& entry = entries[i];
    switch (entry.type) {
      case MALLOC:
      case MEMALIGN:
      case REALLOC:
        if (entry.size >= min_size && entry.size <= max_size) {
          PrintEntry(entry, entry.size, print_trace_format);
          if (entry.type == REALLOC) {
            total_reallocs++;
          } else {
            total_allocs++;
          }
        }
        break;

      case CALLOC:
        if (size_t size = entry.u.n_elements * entry.size;
            size >= min_size && entry.size <= max_size) {
          PrintEntry(entry, size, print_trace_format);
        }
        break;

      case FREE:
      case THREAD_DONE:
      default:
        break;
    }
  }
  if (!print_trace_format) {
    printf("Total allocs:   %zu\n", total_allocs);
    printf("Total reallocs: %zu\n", total_reallocs);
  }

  FreeEntries(entries, num_entries);
}

int main(int argc, char** argv) {
  if (argc == 1) {
    Usage();
    return 1;
  }

  std::vector<std::string_view> args(&argv[1], &argv[argc]);
  std::unordered_map<std::string_view, std::string_view> values;
  if (!ParseOptions(args, values)) {
    Usage();
    return 1;
  }

  size_t min_size = 0;
  auto entry = values.find("--min_size");
  if (entry != values.end()) {
    if (!android::base::ParseUint<size_t>(entry->second.data(), &min_size)) {
      fprintf(stderr, "--min_size parameter is not %s: %s\n",
              (errno == ERANGE) ? "in the valid range for a size_t" : "a valid number",
              entry->second.data());
      Usage();
      return 1;
    }
  }
  size_t max_size = std::numeric_limits<size_t>::max();
  entry = values.find("--max_size");
  if (entry != values.end()) {
    if (!android::base::ParseUint<size_t>(entry->second.data(), &max_size)) {
      fprintf(stderr, "--max_size parameter is not %s: %s\n",
              (errno == ERANGE) ? "in the valid range for a size_t" : "a valid number",
              entry->second.data());
      Usage();
      return 1;
    }
  }

  ProcessTrace(values["trace"], min_size, max_size,
               values.find("--print_trace_format") != values.end());
  return 0;
}
