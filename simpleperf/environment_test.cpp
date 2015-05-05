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

#include <functional>
#include <base/file.h>

#include "environment.h"

TEST(environment, GetOnlineCpusFromString) {
  ASSERT_EQ(GetOnlineCpusFromString(""), std::vector<int>());
  ASSERT_EQ(GetOnlineCpusFromString("0-2"), std::vector<int>({0, 1, 2}));
  ASSERT_EQ(GetOnlineCpusFromString("0,2-3"), std::vector<int>({0, 2, 3}));
}

static bool FindKernelSymbol(const KernelSymbol& sym1, const KernelSymbol& sym2) {
  return sym1.addr == sym2.addr && sym1.type == sym2.type && strcmp(sym1.name, sym2.name) == 0 &&
         ((sym1.module == nullptr && sym2.module == nullptr) ||
          (strcmp(sym1.module, sym2.module) == 0));
}

TEST(environment, ProcessKernelSymbols) {
  std::string data =
      "ffffffffa005c4e4 d __warned.41698   [libsas]\n"
      "aaaaaaaaaaaaaaaa T _text\n"
      "cccccccccccccccc c ccccc\n";
  const char* tempfile = "tempfile_process_kernel_symbols";
  ASSERT_TRUE(android::base::WriteStringToFile(data, tempfile));
  KernelSymbol expected_symbol;
  expected_symbol.addr = 0xffffffffa005c4e4ULL;
  expected_symbol.type = 'd';
  expected_symbol.name = "__warned.41698";
  expected_symbol.module = "libsas";
  ASSERT_TRUE(ProcessKernelSymbols(
      tempfile, std::bind(&FindKernelSymbol, std::placeholders::_1, expected_symbol)));

  expected_symbol.addr = 0xaaaaaaaaaaaaaaaaULL;
  expected_symbol.type = 'T';
  expected_symbol.name = "_text";
  expected_symbol.module = nullptr;
  ASSERT_TRUE(ProcessKernelSymbols(
      tempfile, std::bind(&FindKernelSymbol, std::placeholders::_1, expected_symbol)));

  expected_symbol.name = "non_exist_symbol";
  ASSERT_FALSE(ProcessKernelSymbols(
      tempfile, std::bind(&FindKernelSymbol, std::placeholders::_1, expected_symbol)));
  ASSERT_EQ(0, unlink(tempfile));
}

static void FindModule(uint64_t addr, const char* name, uint64_t expected_addr,
                       const char* expected_name, bool* meet_expected) {
  if (addr == expected_addr && strcmp(name, expected_name) == 0) {
    *meet_expected = true;
  }
}

TEST(environment, ProcessModules) {
  std::string data =
      "nf_defrag_ipv6 34768 1 nf_conntrack_ipv6, Live 0xffffffffa0fe5000\n"
      "a 0x01\n"
      "b 0x02\n";
  const char* tempfile = "tempfile_process_modules";
  ASSERT_TRUE(android::base::WriteStringToFile(data, tempfile));
  bool meet_expected = false;
  ASSERT_TRUE(
      ProcessModules(tempfile, std::bind(&FindModule, std::placeholders::_1, std::placeholders::_2,
                                         0xffffffffa0fe5000ULL, "nf_defrag_ipv6", &meet_expected)));
  ASSERT_TRUE(meet_expected);

  meet_expected = false;
  ASSERT_TRUE(ProcessModules(tempfile, std::bind(&FindModule, std::placeholders::_1,
                                                 std::placeholders::_2, 0x2, "a", &meet_expected)));
  ASSERT_FALSE(meet_expected);
  ASSERT_EQ(0, unlink(tempfile));
}
