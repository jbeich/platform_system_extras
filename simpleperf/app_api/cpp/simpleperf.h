/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SIMPLEPERFEXAMPLEWITHNATIVE_SIMPLEPERF_H
#define SIMPLEPERFEXAMPLEWITHNATIVE_SIMPLEPERF_H

#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

// A C++ API used to control simpleperf recording.
// To see simpleperf logs in logcat, filter logcat with "simpleperf".
namespace simpleperf {

class RecordOptionsImpl;
class RecordOptions {
 public:
  RecordOptions();
  ~RecordOptions();
  // Default is perf.data.
  RecordOptions& SetOutputFilename(const std::string& filename);
  // Default is cpu-cycles.
  RecordOptions& SetEvent(const std::string& event);
  // Default is 4000.
  RecordOptions& SetSampleFrequency(size_t freq);
  // Default is no limit, namely record until stopped.
  RecordOptions& SetDuration(double duration_in_second);
  // Default is to record whole app process.
  RecordOptions& SetSampleThreads(const std::vector<pid_t>& threads);
  RecordOptions& RecordDwarfCallGraph();
  RecordOptions& RecordFramePointerCallGraph();
  RecordOptions& TraceOffCpu();

  std::vector<std::string> ToRecordArgs() const;

 private:
  RecordOptionsImpl* impl_;
};

class ProfileSessionImpl;
class ProfileSession {
 public:
  // app_data_dir is the same as android.content.Context.getDataDir().
  // ProfileSession will store profiling data in <app_data_dir>/simpleperf_data/.
  ProfileSession(const std::string& app_data_dir);
  // Assume app_data_dir is /data/data/<app_package_name>.
  ProfileSession();
  ~ProfileSession();
  void StartRecording(const RecordOptions& options);
  void StartRecording(const std::vector<std::string>& record_args);
  void PauseRecording();
  void ResumeRecording();
  void StopRecording();
  std::string GetRecordingLog();
 private:
  ProfileSessionImpl* impl_;
};

}  // namespace simpleperf
#endif //SIMPLEPERFEXAMPLEWITHNATIVE_SIMPLEPERF_H
