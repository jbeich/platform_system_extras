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

#define LOG_TAG "TC_EXTRAS"

#include <android/log.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

// Use _system_properties.h to use __system_property_wait_any()
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#define ALOGI(...) (__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define ALOGE(...) (__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

extern "C" {

void __gcov_flush(void);

sighandler_t old_sigusr1_handler = SIG_IGN;

void gcov_signal_handler(int signum) {
  if (signum == SIGUSR1) {
    ALOGI("__gcov_flush triggered by signal");
    __gcov_flush();
  }
  if (signum == SIGUSR1 && old_sigusr1_handler != SIG_IGN && old_sigusr1_handler != SIG_DFL) {
    old_sigusr1_handler(signum);
  }
}

static const char kCoveragePropName[] = "coverage.flush";

// In a loop, wait for any change to sysprops and trigger a __gcov_flush
// when kCoveragePropName sysprop transistions to "1" after a transistion to
// "0".
void *property_watch_loop(__attribute__((unused)) void *arg) {
  uint32_t serial = 0;
  bool prop_was_reset = false;

  while(true) {
    serial = __system_property_wait_any(serial);
    const struct prop_info *pi = __system_property_find(kCoveragePropName);
    if (!pi)
      continue;

    char value[PROP_VALUE_MAX];
    __system_property_read(pi, nullptr, value);
    if (strcmp(value, "0") == 0) {
      prop_was_reset = true;
    } else if (strcmp(value, "1") == 0) {
      if (prop_was_reset) {
        ALOGI("__gcov_flush triggered by sysprop");
        __gcov_flush();
        prop_was_reset = false;
      }
    }
  }
}

// Initialize libprofile-extras:
// - Install a signal handler that triggers __gcov_flush on SIGUSR1
// - Create a thread that calls __gcov_flush when kCoveragePropName sysprop
// transistions to "1" after a transistion to "0".
int init_profile_extras(void) {
  sighandler_t ret1 = signal(SIGUSR1, gcov_signal_handler);
  if (ret1 == SIG_ERR) {
    ALOGE("Setting signal handler (signal()) failed: %s\n", strerror(errno));
    return -1;
  }
  else {
    // Store existing signal handler.  This gets invoked by gcov_signal_handler.
    old_sigusr1_handler = ret1;
  }

  pthread_t thread;
  if(pthread_create(&thread, nullptr, property_watch_loop, nullptr) != 0) {
    ALOGE("Creating property watcher thread (pthread_create()) failed: %s\n",
          strerror(errno));
    return -1;
  }
  return 0;
}

int __profile_extras = init_profile_extras();
}
