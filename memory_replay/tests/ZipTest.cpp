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

#include <malloc.h>
#include <stdint.h>

#include <string>

#include <android-base/file.h>
#include <gtest/gtest.h>

#include "Alloc.h"
#include "Zip.h"

std::string GetTestZip() {
  return android::base::GetExecutableDirectory() + "/tests/test.zip";
}

TEST(ZipTest, zip_get_contents) {
  EXPECT_EQ("12345: malloc 0x1000 16\n12345: free 0x1000\n", ZipGetContents(GetTestZip().c_str()));
}

TEST(ZipTest, zip_get_contents_bad_file) {
  EXPECT_EQ("", ZipGetContents("/does/not/exist"));
}

TEST(ZipTest, zip_get_unwind_info) {
  // This might allocate, so do it before getting mallinfo.
  std::string file_name = GetTestZip();

  size_t mallinfo_before = mallinfo().uordblks;
  AllocEntry* entries;
  size_t num_entries;
  ZipGetUnwindInfo(file_name.c_str(), &entries, &num_entries);
  size_t mallinfo_after = mallinfo().uordblks;

  // Verify no memory is allocated.
  EXPECT_EQ(mallinfo_after, mallinfo_before);

  ASSERT_EQ(2U, num_entries);
  EXPECT_EQ(12345, entries[0].tid);
  EXPECT_EQ(MALLOC, entries[0].type);
  EXPECT_EQ(0x1000U, entries[0].ptr);
  EXPECT_EQ(16U, entries[0].size);
  EXPECT_EQ(0U, entries[0].u.old_ptr);

  EXPECT_EQ(12345, entries[1].tid);
  EXPECT_EQ(FREE, entries[1].type);
  EXPECT_EQ(0x1000U, entries[1].ptr);
  EXPECT_EQ(0U, entries[1].size);
  EXPECT_EQ(0U, entries[1].u.old_ptr);

  ZipFreeEntries(entries, num_entries);
}

TEST(ZipTest, zip_get_unwind_info_bad_file) {
  AllocEntry* entries;
  size_t num_entries;
  EXPECT_DEATH(ZipGetUnwindInfo("/does/not/exist", &entries, &num_entries), "");
}
