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

#include <android-base/file.h>
#include <android-base/test_utils.h>

#include "dso.h"
#include "environment.h"
#include "get_test_data.h"

TEST(environment, GetCpusFromString) {
  ASSERT_EQ(GetCpusFromString(""), std::vector<int>());
  ASSERT_EQ(GetCpusFromString("0-2"), std::vector<int>({0, 1, 2}));
  ASSERT_EQ(GetCpusFromString("0,2-3"), std::vector<int>({0, 2, 3}));
  ASSERT_EQ(GetCpusFromString("1,0-3,3,4"), std::vector<int>({0, 1, 2, 3, 4}));
}

TEST(environment, PrepareVdsoFile) {
  std::string content;
  ASSERT_TRUE(android::base::ReadFileToString("/proc/self/maps", &content));
  if (content.find("[vdso]") == std::string::npos) {
    // Vdso isn't used, no need to test.
    return;
  }
  TemporaryDir tmpdir;
  ScopedTempFiles scoped_temp_files(tmpdir.path);
  PrepareVdsoFile();
  std::unique_ptr<Dso> dso = Dso::CreateDso(DSO_ELF_FILE, "[vdso]",
                                            sizeof(size_t) == sizeof(uint64_t));
  ASSERT_TRUE(dso != nullptr);
  ASSERT_NE(dso->GetDebugFilePath(), "[vdso]");
}

TEST(environment, GetThreadMmapsInProcess) {
  std::vector<ThreadMmap> maps;
  GetThreadMmapsInProcess(GetTestData("maps"), &maps);
  ASSERT_EQ(2043u, maps.size());
  ASSERT_EQ(maps[0].start_addr, 0x12c00000u);
  ASSERT_EQ(maps[0].len, 0x2ac00000u - 0x12c00000u);
  ASSERT_FALSE(maps[0].executable);
  ASSERT_EQ(maps[0].pgoff, 0u);
  ASSERT_EQ(maps[0].name, "/dev/ashmem/dalvik-main space");
  ASSERT_EQ(maps[1260].name, "/dev/ashmem/dalvik-classes.dex extracted in memory from "
      "/data/app/com.google.sample.tunnel-HGGRU03Gu1Mwkf_-RnFmvw==/base.apk");
}
