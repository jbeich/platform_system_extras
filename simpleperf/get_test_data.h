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

#ifndef SIMPLE_PERF_GET_TEST_DATA_H_
#define SIMPLE_PERF_GET_TEST_DATA_H_

#include <string>

#include "build_id.h"

std::string GetTestData(const std::string& filename);
const std::string& GetTestDataDir();

// The source code of elf and elf_with_mini_debug_info is testdata/elf_file_source.cpp.
static const std::string ELF_FILE = "elf";
static const std::string ELF_FILE_WITH_MINI_DEBUG_INFO = "elf_with_mini_debug_info";
// perf.data is generated by sampling on three processes running different
// executables: elf, t1, t2 (all generated by elf_file_source.cpp, but with different
// executable name).
static const std::string PERF_DATA = "perf.data";
// perf_g_fp.data is generated by sampling on one process running elf using --call-graph fp option.
static const std::string CALLGRAPH_FP_PERF_DATA = "perf_g_fp.data";
// perf_b.data is generated by sampling on one process running elf using -b option.
static const std::string BRANCH_PERF_DATA = "perf_b.data";
// perf_with_mini_debug_info.data is generated by sampling on one process running
// elf_with_mini_debug_info.
static const std::string PERF_DATA_WITH_MINI_DEBUG_INFO = "perf_with_mini_debug_info.data";

static BuildId elf_file_build_id("0b12a384a9f4a3f3659b7171ca615dbec3a81f71");


// To generate apk supporting execution on shared libraries in apk:
// 1. Add android:extractNativeLibs=false in AndroidManifest.xml.
// 2. Use `zip -0` to store native libraries in apk without compression.
// 3. Use `zipalign -p 4096` to make native libraries in apk start at page boundaries.
//
// The logical in libhello-jni.so is as below:
//  volatile int GlobalVar;
//
//  while (true) {
//    GlobalFunc() -> Func1() -> Func2()
//  }
// And most time is spent in Func2().
static const std::string APK_FILE = "data/app/com.example.hellojni-1/base.apk";
static const std::string NATIVELIB_IN_APK = "lib/arm64-v8a/libhello-jni.so";
// has_embedded_native_libs_apk_perf.data is generated by sampling on one process running
// APK_FILE using -g --no-unwind option.
static const std::string NATIVELIB_IN_APK_PERF_DATA = "has_embedded_native_libs_apk_perf.data";
// The offset and size info are extracted from the generated apk file to run read_apk tests.
constexpr size_t NATIVELIB_OFFSET_IN_APK = 0x639000;
constexpr size_t NATIVELIB_SIZE_IN_APK = 0x1678;

static BuildId native_lib_build_id("8ed5755a7fdc07586ca228b8ee21621bce2c7a97");

// perf_with_two_event_types.data is generated by sampling using -e cpu-cycles,cpu-clock option.
static const std::string PERF_DATA_WITH_TWO_EVENT_TYPES = "perf_with_two_event_types.data";

// perf_with_kernel_symbol.data is generated by `sudo simpleperf record ls -l`.
static const std::string PERF_DATA_WITH_KERNEL_SYMBOL = "perf_with_kernel_symbol.data";

// perf_with_symbols.data is generated by `sudo simpleperf record --dump-symbols sleep 1`.
static const std::string PERF_DATA_WITH_SYMBOLS = "perf_with_symbols.data";

// perf_with_kmem_slab_callgraph.data is generated by `simpleperf kmem record --slab --call-graph fp sleep 0.0001`.
static const std::string PERF_DATA_WITH_KMEM_SLAB_CALLGRAPH_RECORD = "perf_with_kmem_slab_callgraph.data";

// perf_with_kmem_page_callgraph.data is generated by `simpleperf kmem record --page -g sleep 1`.
static const std::string PERF_DATA_WITH_KMEM_PAGE_CALLGRAPH_RECORD = "perf_with_kmem_page_callgraph.data";

#endif  // SIMPLE_PERF_GET_TEST_DATA_H_
