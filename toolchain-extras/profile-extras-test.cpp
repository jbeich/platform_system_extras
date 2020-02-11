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

#include <gtest/gtest.h>

#include <sys/system_properties.h>

#include "profile-extras.h"

static int flush_count = 0;

extern "C" {
void __gcov_flush() {
  flush_count++;
}
}

TEST(profile_extras, smoke) {
  flush_count = 0;

  ASSERT_EQ(0, flush_count);
  kill(getpid(), COVERAGE_FLUSH_SIGNAL);
  sleep(2);
  ASSERT_EQ(1, flush_count);
}
