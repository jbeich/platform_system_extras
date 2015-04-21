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

#include <gtest/gtest.h>

#include <workload.h>

#include <chrono>

using namespace std::chrono;

static void TestWorkloadOfSleepingOneSecond(std::unique_ptr<Workload>& workload) {
  ASSERT_TRUE(workload != nullptr);
  ASSERT_FALSE(workload->IsFinished());
  ASSERT_TRUE(workload->GetWorkPid() != 0);
  auto start_time = steady_clock::now();
  ASSERT_TRUE(workload->Start());
  ASSERT_FALSE(workload->IsFinished());
  ASSERT_TRUE(workload->WaitFinish());
  ASSERT_TRUE(workload->IsFinished());
  auto end_time = steady_clock::now();
  ASSERT_TRUE(end_time >= start_time + seconds(1));
}

TEST(workload, WorkloadInNewProcess) {
  auto workload = Workload::CreateWorkloadInNewProcess({"sleep", "1"});
  TestWorkloadOfSleepingOneSecond(workload);
}

TEST(workload, WorkloadOfSleep) {
  auto workload = Workload::CreateWorkloadOfSleep(seconds(1));
  TestWorkloadOfSleepingOneSecond(workload);
}
