/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <base/logging.h>
#include <cutils/properties.h>

static void RestartCpuHotplug() {
  property_set("ctl.start", "mpdecision");
}

void DisableCpuHotplug() {
  char propBuf[PROPERTY_VALUE_MAX];
  property_get("init.svc.mpdecision", propBuf, "");
  if (strcmp(propBuf, "running") == 0) {
    if (property_set("ctl.stop", "mpdecision")) {
      PLOG(ERROR) << "can't stop mpdecision";
    }
    for (int i = 0; i < 10; ++i) {
      property_get("init.svc.mpdecision", propBuf, "");
      if (strcmp(propBuf, "stopped") == 0) {
        if (atexit(RestartCpuHotplug) != 0) {
          PLOG(ERROR) << "register RestartHotplug to run at exit failed";
        }
        return;
      }
      sleep(1);
    }
    LOG(ERROR) << "mpdecision is not stopped, in " << propBuf << " state";
  }
}
