//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define LOG_TAG "profcollectd_binder"

#include "binder_service.h"

#include <functional>

#include <android-base/logging.h>

#include "config_utils.h"
#include "hwtrace_provider.h"
#include "scheduler.h"

namespace android {
namespace profcollectd {

using ::android::binder::Status;
using ::com::android::server::profcollect::IProfCollectd;
using std::placeholders::_1;

static constexpr config_t CONFIG_ENABLED = {"enabled", "0"};  // Disabled by default.

ProfcollectdBinder::ProfcollectdBinder() {
  static bool enabled = getConfigFlagBool(CONFIG_ENABLED);

  if (enabled) {
    ProfcollectdBinder::Scheduler = std::make_unique<ProfcollectdScheduler>();
    LOG(INFO) << "Binder service started";
  } else {
    LOG(INFO) << "profcollectd is not enabled through device config.";
  }
}

Status ProfcollectdBinder::ReadConfig() {
  return ForwardScheduler(&ProfcollectdScheduler::ReadConfig);
}

Status ProfcollectdBinder::ScheduleCollection() {
  return ForwardScheduler(&ProfcollectdScheduler::ScheduleCollection);
}

Status ProfcollectdBinder::TerminateCollection() {
  return ForwardScheduler(&ProfcollectdScheduler::TerminateCollection);
}

Status ProfcollectdBinder::TraceOnce(const std::string& tag) {
  return ForwardScheduler(std::bind(&ProfcollectdScheduler::TraceOnce, _1, tag));
}

Status ProfcollectdBinder::ProcessProfile() {
  return ForwardScheduler(&ProfcollectdScheduler::ProcessProfile);
}

Status ProfcollectdBinder::CreateProfileReport() {
  return ForwardScheduler(&ProfcollectdScheduler::CreateProfileReport);
}

Status ProfcollectdBinder::GetSupportedProvider(std::string* provider) {
  return ForwardScheduler(std::bind(&ProfcollectdScheduler::GetSupportedProvider, _1, *provider));
}

Status ProfcollectdBinder::ForwardScheduler(
    const std::function<OptError(ProfcollectdScheduler*)>& action) {
  if (Scheduler == nullptr) {
    return Status::fromExceptionCode(1, "profcollectd is not enabled through device config.");
  }

  auto errmsg = action(&*Scheduler);
  if (errmsg) {
    LOG(ERROR) << errmsg.value();
    return Status::fromExceptionCode(1, errmsg.value().c_str());
  }
  return Status::ok();
}

}  // namespace profcollectd
}  // namespace android
