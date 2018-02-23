/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "read_dex_file.h"

#include <gtest/gtest.h>
#include "get_test_data.h"
#include "test_util.h"

TEST(read_dex_file, ParseSymbolsFromDexFile) {
  std::string file_path = GetTestData("base.vdex");
  const int DEX_OFFSET_IN_VDEX_FILE = 36;
  std::vector<DexFileSymbol> symbols;
  auto callback = [&](DexFileSymbol& symbol) {
    symbols.push_back(symbol);
  };
  ASSERT_TRUE(ParseSymbolsFromDexFile(file_path, DEX_OFFSET_IN_VDEX_FILE, callback));
  ASSERT_FALSE(symbols.empty());
}


TEST(read_dex_file, t1) {
  std::string file_path = "boot-framework.vdex";
  for (int offset : {0x2c, 0x19c678, 0x31c7cc}) {
    int DEX_OFFSET_IN_VDEX_FILE = offset;
    printf("offst 0x%x\n", DEX_OFFSET_IN_VDEX_FILE);
    std::vector<DexFileSymbol> symbols;
    auto callback = [&](DexFileSymbol& symbol) {
      symbols.push_back(symbol);
    };
    ASSERT_TRUE(ParseSymbolsFromDexFile(file_path, DEX_OFFSET_IN_VDEX_FILE, callback));
    std::sort(symbols.begin(), symbols.end(), [](const DexFileSymbol& s1, const DexFileSymbol& s2) {
      return s1.offset < s2.offset;
    });
    for (auto symbol : symbols) {
      printf("symbol [0x%" PRIx64 " - 0x%" PRIx64 "] %s\n", symbol.offset, symbol.offset + symbol.len, symbol.name.c_str());
    }
  }
}
