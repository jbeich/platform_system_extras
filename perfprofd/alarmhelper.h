/*
**
** Copyright 2016, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef SYSTEM_EXTRAS_PERFPROFD_ALARMHELPER_H_
#define SYSTEM_EXTRAS_PERFPROFD_ALARMHELPER_H_

//
// Constructor takes a timeout (in seconds) and a child pid; If an
// alarm set for the specified number of seconds triggers, then a
// SIGKILL is sent to the child. Destructor resets alarm. Example:
//
//       pid_t child_pid = ...;
//       { AlarmHelper h(10, child_pid);
//         ... = read_from_child(child_pid, ...);
//       }
//
// NB: this helper is not re-entrant-- avoid nested use or
// use by multiple threads
//
class AlarmHelper {
 public:
  AlarmHelper(unsigned num_seconds, pid_t child);
  ~AlarmHelper();
  static void handler(int, siginfo_t *, void *);

 private:
  struct sigaction oldsigact_;
  static pid_t child_;
};

#endif
