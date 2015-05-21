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

#include "sample_tree.h"

#include <gtest/gtest.h>

struct ExpectedSampleInMap {
  int pid;
  int tid;
  int map_pid;
  uint64_t map_start_addr;
  size_t sample_count;
};

static bool SampleMatchExpectation(const SampleInMap& sample, const ExpectedSampleInMap& expected) {
  EXPECT_EQ(expected.pid, sample.pid);
  EXPECT_EQ(expected.tid, sample.tid);
  EXPECT_TRUE(sample.map != nullptr);
  EXPECT_EQ(expected.map_pid, sample.map->pid);
  EXPECT_EQ(expected.map_start_addr, sample.map->start_addr);
  EXPECT_EQ(expected.sample_count, sample.samples.size());
  if (sample.pid != expected.pid) {
    return false;
  }
  if (sample.tid != expected.tid) {
    return false;
  }
  if (sample.map == nullptr) {
    return false;
  }
  if (sample.map->pid != expected.map_pid) {
    return false;
  }
  if (sample.map->start_addr != expected.map_start_addr) {
    return false;
  }
  if (sample.samples.size() != expected.sample_count) {
    return false;
  }
  return true;
}

void CheckSampleCallback(const SampleInMap& sample,
                         std::vector<ExpectedSampleInMap>& expected_samples, size_t* pos) {
  ASSERT_LT(*pos, expected_samples.size());
  ASSERT_TRUE(SampleMatchExpectation(sample, expected_samples[*pos]))
      << "Error matching sample at pos " << *pos;
  ++*pos;
}

class SampleTreeTest : public testing::Test {
 protected:
  virtual void SetUp() {
    sample_tree.AddMap(1, 1, 10, 0, "", 0);
    sample_tree.AddMap(1, 11, 10, 0, "", 0);
    sample_tree.AddMap(2, 1, 20, 0, "", 0);
    sample_tree.AddMap(-1, 11, 20, 0, "", 0);
  }

  void VisitSampleTree(std::vector<ExpectedSampleInMap>& expected_samples) {
    size_t pos = 0;
    sample_tree.VisitAllSamples(
        std::bind(&CheckSampleCallback, std::placeholders::_1, expected_samples, &pos));
    ASSERT_EQ(expected_samples.size(), pos);
  }

  SampleTree sample_tree;
};

TEST_F(SampleTreeTest, ip_in_map) {
  sample_tree.AddSample(1, 1, 1, 0, 0, 0);
  sample_tree.AddSample(1, 1, 5, 0, 0, 0);
  sample_tree.AddSample(1, 1, 10, 0, 0, 0);
  std::vector<ExpectedSampleInMap> expected_samples = {
      {1, 1, 1, 1, 3},
  };
  VisitSampleTree(expected_samples);
}

TEST_F(SampleTreeTest, different_pid) {
  sample_tree.AddSample(1, 1, 1, 0, 0, 0);
  sample_tree.AddSample(2, 2, 2, 0, 0, 0);
  std::vector<ExpectedSampleInMap> expected_samples = {
      {1, 1, 1, 1, 1}, {2, 2, 2, 1, 1},
  };
  VisitSampleTree(expected_samples);
}

TEST_F(SampleTreeTest, different_tid) {
  sample_tree.AddSample(1, 1, 1, 0, 0, 0);
  sample_tree.AddSample(1, 11, 1, 0, 0, 0);
  std::vector<ExpectedSampleInMap> expected_samples = {
      {1, 1, 1, 1, 1}, {1, 11, 1, 1, 1},
  };
  VisitSampleTree(expected_samples);
}

TEST_F(SampleTreeTest, different_map) {
  sample_tree.AddSample(1, 1, 1, 0, 0, 0);
  sample_tree.AddSample(1, 1, 11, 0, 0, 0);
  std::vector<ExpectedSampleInMap> expected_samples = {
      {1, 1, 1, 1, 1}, {1, 1, 1, 11, 1},
  };
  VisitSampleTree(expected_samples);
}

TEST_F(SampleTreeTest, unmapped_sample) {
  sample_tree.AddSample(1, 1, 0, 0, 0, 0);
  sample_tree.AddSample(1, 1, 31, 0, 0, 0);
  sample_tree.AddSample(1, 1, 70, 0, 0, 0);
  std::vector<ExpectedSampleInMap> expected_samples = {};
  VisitSampleTree(expected_samples);
}

TEST_F(SampleTreeTest, map_kernel) {
  sample_tree.AddSample(1, 1, 11, 0, 0, 0);
  sample_tree.AddSample(1, 1, 21, 0, 0, 0);
  std::vector<ExpectedSampleInMap> expected_samples = {
      {1, 1, -1, 11, 1}, {1, 1, 1, 11, 1},
  };
  VisitSampleTree(expected_samples);
}
