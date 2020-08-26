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

#include <stdlib.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dso.h"

namespace simpleperf {

namespace {

std::optional<std::string_view> ConsumeWord(std::string_view& content_ref) {
  size_t begin = content_ref.find_first_not_of(" \t");
  if (begin == content_ref.npos) {
    return {};
  }

  size_t end = content_ref.find_first_of(" \t", begin + 1);
  if (end == content_ref.npos) {
    end = content_ref.size();
  }

  auto res = content_ref.substr(begin, end - begin);
  content_ref.remove_prefix(end);
  return res;
}

std::optional<uint64_t> ConsumeUInt(std::string_view& content_ref) {
  auto word = ConsumeWord(content_ref);
  if (!word) {
    return {};
  }

  const char* start = word.value().data();
  char* stop;
  auto res = strtoull(start, &stop, 0);
  if (stop - start != word.value().size()) {
    return {};
  }

  return res;
}

void ReadSymbol(std::string_view content, std::vector<Symbol>* symbols) {
  auto addr = ConsumeUInt(content);
  if (!addr) {
    return;
  }

  auto size = ConsumeUInt(content);
  if (!size) {
    return;
  }

  auto name = ConsumeWord(content);
  if (!name) {
    return;
  }

  if (ConsumeWord(content)) {
    return;
  }

  symbols->emplace_back(name.value(), addr.value(), size.value());
}

}  // namespace

std::vector<Symbol> ReadSymbolMapFromString(const std::string& content) {
  std::vector<Symbol> symbols;

  for (size_t begin = 0; ; ) {
    size_t end = content.find_first_of("\n\r", begin);

    if (end == content.npos) {
      ReadSymbol({content.c_str() + begin, content.size() - begin}, &symbols);
      std::sort(symbols.begin(), symbols.end(), Symbol::CompareValueByAddr);
      return symbols;
    }

    ReadSymbol({content.c_str() + begin, end - begin}, &symbols);
    begin = end + 1;
  }
}

}  // namespace simpleperf
