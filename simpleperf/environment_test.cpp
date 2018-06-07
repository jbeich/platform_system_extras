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
#include <android-base/file.h>
#include <android-base/test_utils.h>

#include "environment.h"

TEST(environment, GetCpusFromString) {
  ASSERT_EQ(GetCpusFromString(""), std::vector<int>());
  ASSERT_EQ(GetCpusFromString("0-2"), std::vector<int>({0, 1, 2}));
  ASSERT_EQ(GetCpusFromString("0,2-3"), std::vector<int>({0, 2, 3}));
  ASSERT_EQ(GetCpusFromString("1,0-3,3,4"), std::vector<int>({0, 1, 2, 3, 4}));
}

static bool ModulesMatch(const char* p, const char* q) {
  if (p == nullptr && q == nullptr) {
    return true;
  }
  if (p != nullptr && q != nullptr) {
    return strcmp(p, q) == 0;
  }
  return false;
}

static bool KernelSymbolsMatch(const KernelSymbol& sym1, const KernelSymbol& sym2) {
  return sym1.addr == sym2.addr &&
         sym1.type == sym2.type &&
         strcmp(sym1.name, sym2.name) == 0 &&
         ModulesMatch(sym1.module, sym2.module);
}

TEST(environment, ProcessKernelSymbols) {
  std::string data =
      "ffffffffa005c4e4 d __warned.41698   [libsas]\n"
      "aaaaaaaaaaaaaaaa T _text\n"
      "cccccccccccccccc c ccccc\n";
  TemporaryFile tempfile;
  ASSERT_TRUE(android::base::WriteStringToFile(data, tempfile.path));
  KernelSymbol expected_symbol;
  expected_symbol.addr = 0xffffffffa005c4e4ULL;
  expected_symbol.type = 'd';
  expected_symbol.name = "__warned.41698";
  expected_symbol.module = "libsas";
  ASSERT_TRUE(ProcessKernelSymbols(
      tempfile.path, std::bind(&KernelSymbolsMatch, std::placeholders::_1, expected_symbol)));

  expected_symbol.addr = 0xaaaaaaaaaaaaaaaaULL;
  expected_symbol.type = 'T';
  expected_symbol.name = "_text";
  expected_symbol.module = nullptr;
  ASSERT_TRUE(ProcessKernelSymbols(
      tempfile.path, std::bind(&KernelSymbolsMatch, std::placeholders::_1, expected_symbol)));

  expected_symbol.name = "non_existent_symbol";
  ASSERT_FALSE(ProcessKernelSymbols(
      tempfile.path, std::bind(&KernelSymbolsMatch, std::placeholders::_1, expected_symbol)));
}

TEST(environment, GetHardwareFromCpuInfo) {
  std::string cpu_info = "CPU revision : 10\n\n"
      "Hardware : Symbol i.MX6 Freeport_Plat Quad/DualLite (Device Tree)\n";
  ASSERT_EQ("Symbol i.MX6 Freeport_Plat Quad/DualLite (Device Tree)",
            GetHardwareFromCpuInfo(cpu_info));
}
