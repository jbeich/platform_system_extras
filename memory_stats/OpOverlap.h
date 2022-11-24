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

// Accumulate the time intervals that have at least two Alloc requests executed.
// For example,
//
//   1234: malloc 0x0 32 1 100
//   5678: malloc 0x0 32 50 120
//
// The interval [50, 100] has two malloc requests happened and the length is 51.
// This analysis will return the amount of length of all overlap intervals.
class OpOverlap : public Analysis {
  public:
    OpOverlap() = default;
    virtual ~OpOverlap() = default;

    bool analyze(const MemoryStats::RecordsTy& records) final {
        if (records.size() <= 1) return true;

        auto [cur_st, cur_et] = std::tie(records[0]->st, records[0]->et);

        totalOverlapping_ = 0;

        for (size_t i = 1; i < records.size(); ++i) {
            auto [st, et] = std::tie(records[i]->st, records[i]->et);
            if (et <= cur_st) continue;

            if (st >= cur_et) {
                std::tie(cur_st, cur_et) = std::tie(st, et);
            } else if (et < cur_et) {
                totalOverlapping_ += et - std::max(st, cur_st);
                std::tie(cur_st, cur_et) = std::tie(et, cur_et);
            } else {
                totalOverlapping_ += cur_et - std::max(st, cur_st);
                std::tie(cur_st, cur_et) = std::tie(cur_et, et);
            }
        }

        return true;
    }

    bool GetResult(std::ostream& os) final {
        os << "Overlap of all operations is " << totalOverlapping_ << " ns" << std::endl;
        return true;
    }

  private:
    uint64_t totalOverlapping_;
};
