/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <AllocParser.h>

#include <err.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

struct AllocEntry;
class Analysis;

class MemoryStats {
  public:
    using RecordsTy = std::vector<std::unique_ptr<AllocEntry>>;
    MemoryStats() = default;

    bool InitFromFile(std::string_view path) {
        bool result = readRecordsFromFile(path);
        SortRecords();
        return result;
    }
    bool InitFromString(std::string_view content) {
        bool result = readRecordsFromString(content);
        SortRecords();
        return result;
    }

    bool runAnalysis(Analysis* analysis);

  private:
    bool readRecordsFromFile(std::string_view path);
    bool readRecordsFromString(std::string_view content);

    void SortRecords() {
        std::sort(records_.begin(), records_.end(),
                  [](const std::unique_ptr<AllocEntry>& l, const std::unique_ptr<AllocEntry>& r) {
                      if (l->st != r->st) return l->st < r->st;
                      return l->et < r->et;
                  });
    }

    bool ParseString(std::stringstream content);

    RecordsTy records_;
};

class Analysis {
  public:
    virtual bool analyze(const MemoryStats::RecordsTy& records) = 0;

    virtual bool GetResult(std::ostream& os) = 0;
    bool GetResult() { return GetResult(std::cout); }

    virtual ~Analysis() {}

  protected:
    static std::string_view getOpVerboseName(AllocEnum op) {
        switch (op) {
            case MALLOC:
                return "malloc";
            case CALLOC:
                return "calloc";
            case MEMALIGN:
                return "memalign";
            case REALLOC:
                return "realloc";
            case FREE:
                return "free";
            case THREAD_DONE:
                return "thread_done";
            default:
                err(1, "unknown alloc type");
        }
    }
};
