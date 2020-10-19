/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <string>
#include <vector>

#include "command.h"

namespace simpleperf {

inline const OptionFormatMap& GetRecordCmdOptionFormats() {
  static const OptionFormatMap option_formats = {
      {"-a", {OptionValueType::NONE, OptionType::SINGLE, AppRunnerType::NOT_ALLOWED}},
      {"-c", {OptionValueType::UINT, OptionType::ORDERED, AppRunnerType::ALLOWED}},
      {"--call-graph", {OptionValueType::STRING, OptionType::ORDERED, AppRunnerType::ALLOWED}},
      {"--cpu-percent", {OptionValueType::UINT, OptionType::SINGLE, AppRunnerType::ALLOWED}},
      {"--duration", {OptionValueType::DOUBLE, OptionType::SINGLE, AppRunnerType::ALLOWED}},
      {"-e", {OptionValueType::STRING, OptionType::ORDERED, AppRunnerType::ALLOWED}},
      {"--exclude-perf", {OptionValueType::NONE, OptionType::SINGLE, AppRunnerType::ALLOWED}},
      {"-f", {OptionValueType::UINT, OptionType::ORDERED, AppRunnerType::ALLOWED}},
      {"-g", {OptionValueType::NONE, OptionType::ORDERED, AppRunnerType::ALLOWED}},
      {"--symfs", {OptionValueType::STRING, OptionType::SINGLE, AppRunnerType::CHECK_PATH}},
      {"-t", {OptionValueType::STRING, OptionType::MULTIPLE, AppRunnerType::ALLOWED}},
      {"--tp-filter", {OptionValueType::STRING, OptionType::ORDERED, AppRunnerType::ALLOWED}},
  };
  return option_formats;
}

}  // namespace simpleperf
