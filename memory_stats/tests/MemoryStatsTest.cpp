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

#include <gtest/gtest.h>

#include "MemoryStats.h"
#include "OpGenStats.h"
#include "OpOverlap.h"

#include <string_view>

class AnalysisTest : public ::testing::Test {
  protected:
    template <typename Analysis>
    void Run(std::stringstream& res) {
        auto analysis = std::make_unique<Analysis>();
        memory_stats_.runAnalysis(analysis.get());
        analysis->GetResult(res);
    }

    bool ParseData(std::string_view content) { return memory_stats_.InitFromString(content); }

  private:
    MemoryStats memory_stats_;
};

TEST_F(AnalysisTest, Average) {
    const char* const records = R"(
    1234: malloc 0x0 32 1 1001
    1234: malloc 0x10 32 2001 3001
    1234: malloc 0x20 32 4001 5001)";

    ParseData(records);
    std::stringstream result;
    Run<OpAverage>(result);

    EXPECT_STREQ(result.str().c_str(), "malloc: avg exec time = 1000.0 ns\n");
}

TEST_F(AnalysisTest, MinMax) {
    const char* const records = R"(
    1234: malloc 0x0 32 1 1001
    1234: malloc 0x10 32 2001 3001
    1234: malloc 0x20 32 4001 5001)";

    ParseData(records);
    std::stringstream result;
    Run<OpMinMax>(result);

    EXPECT_STREQ(result.str().c_str(),
                 "malloc: min exec time = 1000 ns, max exec time = 1000 ns\n");
}

TEST_F(AnalysisTest, Overlap) {
    const char* const records = R"(
    1234: malloc 0x0 32 1 1001
    1235: malloc 0x30 256 600 800
    1236: malloc 0x10 48 500 2500
    1237: malloc 0x20 128 2200 3200)";

    ParseData(records);
    std::stringstream result;
    Run<OpOverlap>(result);

    EXPECT_STREQ(result.str().c_str(), "Overlap of all operations is 801 ns\n");
}
