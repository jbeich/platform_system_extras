/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <stdint.h>
#define SECTION(sect) __attribute__((section(sect)))
#define RETAIN __attribute__((retain))
#define HIDDEN __attribute__((visibility("hidden")))

/* Add dummy data to ensure the section is always created. Add used attribute so
 * that they are linker GC roots on supported ELF platforms.
 * TODO(pirama) Remove this entire file when the libclang_rt.profile runtimes in
 * the toolchain have https://reviews.llvm.org/D108486.
 */
uint64_t __prof_data_sect_data[0] SECTION("__llvm_prf_data") RETAIN HIDDEN;
uint64_t __prof_cnts_sect_data[0] SECTION("__llvm_prf_cnts") RETAIN HIDDEN;
uint32_t __prof_orderfile_sect_data[0] SECTION("__llvm_orderfile") RETAIN HIDDEN;
const char __prof_nms_sect_data[0] SECTION("__llvm_prf_names") RETAIN HIDDEN;
uint64_t __prof_vnodes_sect_data[0] SECTION("__llvm_prf_vnds") RETAIN HIDDEN;
