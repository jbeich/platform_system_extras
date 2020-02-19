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

#define LOG_TAG "profcollectd_scheduler"

#include "scheduler.h"

#include <fstream>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

#include "compress.h"
#include "hwtrace_provider.h"

// Default option values.
static constexpr const int DEFAULT_COLLECTION_INTERVAL = 600;
static constexpr const int DEFAULT_SAMPLING_PERIOD_MS = 500;
static constexpr const char* DEFAULT_TRACE_OUTDIR = "/data/misc/profcollectd/trace";
static constexpr const char* DEFAULT_OUTDIR = "/data/misc/profcollectd/output";
static constexpr const char* DEFAULT_INJECT_FILTER = "";

namespace android {
namespace profcollectd {

namespace fs = std::filesystem;
using ::android::base::GetIntProperty;
using ::android::base::GetProperty;

// Hwtrace provider registry
extern std::unique_ptr<HwtraceProvider> REGISTER_SIMPLEPERF_ETM_PROVIDER();

namespace {

uintmax_t ClearDir(const fs::path& path) {
  if (!fs::exists(path)) {
    return 0;
  }
  uintmax_t count = 0;
  for (const auto& entry : fs::directory_iterator(path)) {
    count += fs::remove_all(entry);
  }
  return count;
}

bool ClearOnConfigChange(const ProfcollectdScheduler::Config& config) {
  const fs::path configFile = config.profileOutputDir / "CONFIG";
  ProfcollectdScheduler::Config oldConfig{};

  // Read old config, if exists.
  if (fs::exists(configFile)) {
    std::ifstream ifs(configFile);
    ifs >> oldConfig;
  }

  if (oldConfig != config) {
    LOG(INFO) << "Clearing profiles due to config change.";
    ClearDir(config.traceOutputDir);
    ClearDir(config.profileOutputDir);

    // Write new config.
    std::ofstream ofs(configFile);
    ofs << config;
    return true;
  }
  return false;
}

void PeriodicCollectionWorker(std::future<void> terminationSignal, ProfcollectdScheduler& scheduler,
                              std::chrono::seconds& interval) {
  do {
    scheduler.TraceOnce();
  } while ((terminationSignal.wait_for(interval)) == std::future_status::timeout);
}

}  // namespace

ProfcollectdScheduler::ProfcollectdScheduler() {
  ReadConfig();

  // Load a registered hardware trace provider.
  if ((hwtracer = REGISTER_SIMPLEPERF_ETM_PROVIDER())) {
    LOG(INFO) << "ETM provider registered.";
    return;
  } else {
    LOG(ERROR) << "No hardware trace provider found for this architecture.";
    exit(EXIT_FAILURE);
  }
}

OptError ProfcollectdScheduler::ReadConfig() {
  if (workerThread != nullptr) {
    static std::string errmsg = "Terminate the collection before refreshing config.";
    return errmsg;
  }

  const std::lock_guard<std::mutex> lock(mu);

  config.buildFingerprint = GetProperty("ro.build.fingerprint", "unknown");
  config.collectionInterval = std::chrono::seconds(
      GetIntProperty("profcollectd.collection_interval", DEFAULT_COLLECTION_INTERVAL));
  config.samplingPeriod = std::chrono::milliseconds(
      GetIntProperty("profcollectd.sampling_period_ms", DEFAULT_SAMPLING_PERIOD_MS));
  config.traceOutputDir = GetProperty("profcollectd.trace_output_dir", DEFAULT_TRACE_OUTDIR);
  config.profileOutputDir = GetProperty("profcollectd.output_dir", DEFAULT_OUTDIR);
  config.injectFilter = GetProperty("profcollectd.inject_filter", DEFAULT_INJECT_FILTER);
  ClearOnConfigChange(config);

  return std::nullopt;
}

OptError ProfcollectdScheduler::ScheduleCollection() {
  if (workerThread != nullptr) {
    static std::string errmsg = "Collection is already scheduled.";
    return errmsg;
  }

  workerThread =
      std::make_unique<std::thread>(PeriodicCollectionWorker, terminate.get_future(),
                                    std::ref(*this), std::ref(config.collectionInterval));
  return std::nullopt;
}

OptError ProfcollectdScheduler::TerminateCollection() {
  if (workerThread == nullptr) {
    static std::string errmsg = "Collection is not scheduled.";
    return errmsg;
  }

  terminate.set_value();
  workerThread->join();
  workerThread = nullptr;
  terminate = std::promise<void>();  // Reset promise.
  return std::nullopt;
}

OptError ProfcollectdScheduler::TraceOnce() {
  const std::lock_guard<std::mutex> lock(mu);
  bool success = hwtracer->Trace(config.traceOutputDir, config.samplingPeriod);
  if (!success) {
    static std::string errmsg = "Trace failed";
    return errmsg;
  }
  return std::nullopt;
}

OptError ProfcollectdScheduler::ProcessProfile() {
  const std::lock_guard<std::mutex> lock(mu);
  hwtracer->Process(config.traceOutputDir, config.profileOutputDir, config.injectFilter);
  std::vector<fs::path> profiles;
  profiles.insert(profiles.begin(), fs::directory_iterator(config.profileOutputDir),
                  fs::directory_iterator());
  bool success = CompressFiles("/sdcard/profile.zip", profiles);
  if (!success) {
    static std::string errmsg = "Compress files failed";
    return errmsg;
  }
  return std::nullopt;
}

std::ostream& operator<<(std::ostream& os, const ProfcollectdScheduler::Config& config) {
  os << config.buildFingerprint << std::endl;
  os << config.collectionInterval.count() << std::endl;
  os << config.samplingPeriod.count() << std::endl;
  os << config.traceOutputDir << std::endl;
  os << config.profileOutputDir << std::endl;
  os << config.injectFilter << std::endl;
  return os;
}

std::istream& operator>>(std::istream& is, ProfcollectdScheduler::Config& config) {
  is >> config.buildFingerprint;

  long long intervalVal;
  is >> intervalVal;
  config.collectionInterval = std::chrono::seconds(intervalVal);

  float samplingPeriodVal;
  is >> samplingPeriodVal;
  config.samplingPeriod = std::chrono::duration<float>(samplingPeriodVal);

  is >> config.traceOutputDir;
  is >> config.profileOutputDir;
  is >> config.injectFilter;
  return is;
}

}  // namespace profcollectd
}  // namespace android
