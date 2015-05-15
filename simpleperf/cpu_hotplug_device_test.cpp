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

#include <gtest/gtest.h>

#include <stdlib.h>
#include <string.h>

#include <base/logging.h>
#include <cutils/properties.h>

#include "event_attr.h"
#include "event_fd.h"
#include "event_type.h"

static bool WaitCpuHotplugState(const char* waited_state) {
  char propBuf[PROPERTY_VALUE_MAX];
  for (int i = 0; i < 10; ++i) {
    property_get("init.svc.mpdecision", propBuf, "");
    if (strcmp(propBuf, waited_state) == 0) {
      return true;
    }
    sleep(1);
  }
  return false;
}

static void CheckCpuHotplugDisable() {
  {
    // Make sure event file is closed before exit() below.
    const EventType* event_type = EventTypeFactory::FindEventTypeByName("cpu-cycles");
    ASSERT_TRUE(event_type != nullptr);
    auto event_fd =
        EventFd::OpenEventFileForProcess(CreateDefaultPerfEventAttr(*event_type), getpid());
    ASSERT_TRUE(event_fd != nullptr);
  }

  ASSERT_TRUE(WaitCpuHotplugState("stopped"));
  exit(0);
}

TEST(cpu_hotplug_device, disable_plug) {
  char propBuf[PROPERTY_VALUE_MAX];
  property_get("init.svc.mpdecision", propBuf, "");
  if (strcmp(propBuf, "") == 0) {
    GTEST_LOG_(INFO) << "No mpdecision, this test does nothing.\n";
    return;
  }
  if (strcmp(propBuf, "stopped") == 0) {
    ASSERT_EQ(0, property_set("ctl.start", "mpdecision"));
    ASSERT_TRUE(WaitCpuHotplugState("running"));
    sleep(1);  // Wait for the mpdecision process to start.
  }

  ASSERT_EXIT(CheckCpuHotplugDisable(), testing::ExitedWithCode(0), "");
  // Check if mpdecision is restarted.
  ASSERT_TRUE(WaitCpuHotplugState("running"));

  // Restore previous state.
  if (strcmp(propBuf, "stopped") == 0) {
    sleep(1);  // Wait for the mpdecision process to start.
    ASSERT_EQ(0, property_set("ctl.stop", "mpdecision"));
    ASSERT_TRUE(WaitCpuHotplugState("stopped"));
  }
}
