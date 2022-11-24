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

#include "MemoryStats.h"

#include <AllocParser.h>

#include <unordered_map>

// Return the average execution time of all kinds of operations.
class OpAverage : public Analysis {
  public:
    OpAverage() = default;
    virtual ~OpAverage() = default;

    bool analyze(const MemoryStats::RecordsTy& records) final {
        for (auto& ptr : records) {
            AllocEntry& entry = *ptr.get();
            auto& iter = stats_[entry.type];
            iter.first += entry.et - entry.st;
            ++iter.second;
        }
        return true;
    }

    virtual bool GetResult(std::ostream& os) final {
        constexpr AllocEnum outputOrder[] = {MALLOC, CALLOC, MEMALIGN, REALLOC, FREE};
        for (AllocEnum kind : outputOrder) {
            auto [total_time, total_count] = stats_[kind];
            if (total_count == 0) continue;
            os << getOpVerboseName(kind) << ": avg exec time = ";
            os << total_time / total_count;
            uint64_t fraction = total_time % total_count;
            os << "." << (fraction * 1000) / total_count << " ns" << std::endl;
        }

        return true;
    }

  private:
    std::unordered_map<AllocEnum, std::pair<uint64_t, unsigned>> stats_;
};

// Return the min/max execution time of each kinds of operations.
class OpMinMax : public Analysis {
  public:
    OpMinMax() = default;
    virtual ~OpMinMax() = default;

    bool analyze(const MemoryStats::RecordsTy& records) final {
        for (auto& ptr : records) {
            AllocEntry& entry = *ptr.get();
            auto& iter = stats_[entry.type];
            uint64_t period = entry.et - entry.st;
            iter.first = iter.first == 0 ? period : std::min(iter.first, period);
            iter.second = std::max(iter.second, period);
        }

        return true;
    }

    virtual bool GetResult(std::ostream& os) final {
        constexpr AllocEnum outputOrder[] = {MALLOC, CALLOC, MEMALIGN, REALLOC, FREE};
        for (AllocEnum kind : outputOrder) {
            auto [min_exec, max_exec] = stats_[kind];
            if (min_exec == 0) continue;
            os << getOpVerboseName(kind) << ": min exec time = " << min_exec
               << " ns, max exec time = " << max_exec << " ns" << std::endl;
        }

        return true;
    }

  private:
    std::unordered_map<AllocEnum, std::pair<uint64_t, uint64_t>> stats_;
};
