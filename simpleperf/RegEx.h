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

#pragma once

#include <memory>
#include <string>

namespace simpleperf {

class RegExMatch {
 public:
  virtual ~RegExMatch();
  virtual bool IsValid() const = 0;
  virtual std::string GetField(size_t index) const = 0;
  virtual void MoveToNextMatch() = 0;
};

// A wrapper of std::regex, converting std::regex_error exception into return value.
class RegEx {
 public:
  static std::unique_ptr<RegEx> Create(const char* pattern);
  virtual ~RegEx() {}

  virtual const std::string& GetPattern() const = 0;
  virtual bool Match(const std::string& s) const = 0;
  virtual bool Search(const std::string& s) const = 0;
  // Always return a not-null RegExMatch. If no match, RegExMatch->IsValid() is false.
  virtual std::unique_ptr<RegExMatch> SearchAll(const std::string& s) const = 0;
};

}  // namespace simpleperf
