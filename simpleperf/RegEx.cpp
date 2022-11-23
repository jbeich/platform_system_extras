/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "RegEx.h"

#include <regex>

#include <android-base/logging.h>

namespace simpleperf {

RegExMatch::~RegExMatch() {}

class RegExMatchImpl : public RegExMatch {
 public:
  RegExMatchImpl(const std::string_view& s, const std::regex& re)
      : match_it_(s.data(), s.data() + s.size(), re) {}

  bool IsValid() const override { return match_it_ != std::cregex_iterator(); }

  std::string GetField(size_t index) const override { return match_it_->str(index); }

  void MoveToNextMatch() override { ++match_it_; }

 private:
  std::cregex_iterator match_it_;
};

class RegExImpl : public RegEx {
 public:
  RegExImpl(const char* pattern)
      : pattern_(pattern), re_(pattern, std::regex::ECMAScript | std::regex::optimize) {}

  const std::string& GetPattern() const override { return pattern_; }
  bool Match(const std::string& s) const override { return std::regex_match(s, re_); }
  bool Search(const std::string& s) const override { return std::regex_search(s, re_); }
  std::unique_ptr<RegExMatch> SearchAll(std::string_view s) const override {
    return std::unique_ptr<RegExMatch>(new RegExMatchImpl(s, re_));
  }

 private:
  const std::string pattern_;
  std::regex re_;
};

std::unique_ptr<RegEx> RegEx::Create(const char* s) {
  try {
    return std::make_unique<RegExImpl>(s);
  } catch (std::regex_error) {
    LOG(ERROR) << "invalid regex: " << s;
    return nullptr;
  }
}

}  // namespace simpleperf
