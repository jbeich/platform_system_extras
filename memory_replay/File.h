/*
 * Copyright (C) 2019 The Android Open Source Project
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

#pragma once

#include <stdint.h>

#include <string>

// Forward Declarations.
struct AllocEntry;

std::string ZipGetContents(const char* filename);

// If filename ends with .zip, treat as a zip file to decompress.
void GetUnwindInfo(const char* filename, AllocEntry** entries, size_t* num_entries);

void ProcessDump(const AllocEntry* entries, size_t num_entries, size_t max_threads, bool free_all);

void FreeEntries(AllocEntry* entries, size_t num_entries);
