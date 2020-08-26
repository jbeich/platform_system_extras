/*
 * Copyright (C) 2020 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "read_symbol_map.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dso.h"

namespace simpleperf {

namespace {

TEST(read_symbol_map, smoke) {
  std::string content("\n"                     // skip
                      "   0x2000 0x20 two \n"
                      "0x4000\n"               // skip
                      "       0x40 four\n"     // skip
                      "0x1000 0x10 one\n"
                      "     \n"                // skip
                      "0x5000 0x50five\n"      // skip
                      " skip this line\n"      // skip
                      "0x6000 0x60 six six\n"  // skip
                      "0x3000 48   three   \n");

  auto symbols = ReadSymbolMapFromString(content);

  ASSERT_EQ(3u, symbols.size());

  ASSERT_EQ(0x1000, symbols[0].addr);
  ASSERT_EQ(0x10, symbols[0].len);
  ASSERT_STREQ("one", symbols[0].Name());

  ASSERT_EQ(0x2000, symbols[1].addr);
  ASSERT_EQ(0x20, symbols[1].len);
  ASSERT_STREQ("two", symbols[1].Name());

  ASSERT_EQ(0x3000, symbols[2].addr);
  ASSERT_EQ(0x30, symbols[2].len);
  ASSERT_STREQ("three", symbols[2].Name());
}

}  // namespace

}  // namespace simpleperf
