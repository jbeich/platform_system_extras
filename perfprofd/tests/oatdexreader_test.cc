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
#include <algorithm>
#include <cctype>
#include <string>
#include <regex>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <android-base/file.h>
#include <cutils/properties.h>

#include "perfprofdmockutils.h"
#include "oatmapper.h"
#include "oatreader.h"
#include "dexread.h"

#include "perf_profile.pb.h"
#include "oatmap.pb.h"
#include "google/protobuf/text_format.h"

//
// Set to argv[0] on startup
//
static const char *executable_path;

//
// test_dir is the directory containing the test executable and
// any files associated with the test (will be created by the harness).
//
static std::string test_dir;

class OatDexReadTest : public testing::Test {
 protected:
  virtual void SetUp() {
    mock_perfprofdutils_init();
    if (test_dir == "") {
      ASSERT_TRUE(executable_path != nullptr);
      std::string s(executable_path);
      auto found = s.find_last_of("/");
      test_dir = s.substr(0,found);
    }
  }

  virtual void TearDown() {
    mock_perfprofdutils_finish();
  }
};

static bool bothWhiteSpace(char lhs, char rhs)
{
  return (std::isspace(lhs) && std::isspace(rhs));
}

//
// Squeeze out repeated whitespace from expected/actual logs.
//
static std::string squeezeWhite(const std::string &str,
                                const char *tag,
                                bool dump=false)
{
  if (dump) { fprintf(stderr, "raw %s is %s\n", tag, str.c_str()); }
  std::string result(str);
  std::replace( result.begin(), result.end(), '\n', ' ');
  auto new_end = std::unique(result.begin(), result.end(), bothWhiteSpace);
  result.erase(new_end, result.end());
  while (result.begin() != result.end() && std::isspace(*result.rbegin())) {
    result.pop_back();
  }
  if (dump) { fprintf(stderr, "squeezed %s is %s\n", tag, result.c_str()); }
  return result;
}

#define RAW_RESULT(x) #x

class CaptureDexVisitor : public OatDexVisitor {
 public:
  CaptureDexVisitor() { }
  ~CaptureDexVisitor() { }

  void visitDEX(unsigned char sha1sig[20]) {
    ss_ << "DEX sha1 ";
    for (unsigned i = 0; i < 20; ++i)
      ss_ << std::hex << (int) sha1sig[i];
    ss_ << "\n";
  }

  void visitClass(const std::string &className, uint32_t nMethods) {
    ss_ << std::dec << " class " << className << " nmethods=" << nMethods << "\n";
  }

  void visitMethod(const std::string &methodName,
                   unsigned methodIdx,
                   uint64_t methodCodeOffset) {
    ss_ << std::dec << " method " << methodIdx
        << " name=" << methodName
        << " codeOffset="
        << methodCodeOffset << "\n";
  }

  std::string result() { return ss_.str(); }

 private:
  std::stringstream ss_;
};

TEST_F(OatDexReadTest, BasicDexRead)
{
  //
  // Read DEX file into memory and visit.
  //
  std::string dexfile(test_dir);
  dexfile += "/smallish.dex";
  std::string contents;
  auto ret = android::base::ReadFileToString(dexfile.c_str(), &contents);
  ASSERT_TRUE(ret);

  //
  // Visit
  //
  CaptureDexVisitor vis;
  unsigned char *dexdata = reinterpret_cast<unsigned char *>(&contents[0]);
  unsigned char *limit = dexdata + contents.size();
  auto res = examineOatDexFile(dexdata, limit, vis);
  ASSERT_TRUE(res);

  //
  // Test for correct output
  //
  const std::string expected = RAW_RESULT(
      DEX sha1 fd56aced78355c305a953d6f3dfe1f7ff6ac440
      class Lfibonacci nmethods=6
      method 0 name=<init> codeOffset=584
      method 1 name=ifibonacci codeOffset=608
      method 2 name=main codeOffset=656
      method 3 name=rcnm1 codeOffset=1008
      method 4 name=rcnm2 codeOffset=1040
      method 5 name=rfibonacci codeOffset=1072
                                          );
  std::string sqexp = squeezeWhite(expected, "expected");
  std::string result = vis.result();
  std::string sqact = squeezeWhite(result, "actual");
  EXPECT_STREQ(sqexp.c_str(), sqact.c_str());
}

int main(int argc, char **argv) {
  executable_path = argv[0];
  // switch to / before starting testing (all code to be tested
  // should be location-independent)
  chdir("/");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
