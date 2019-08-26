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

#define LOG_TAG "fsverity_init"

#include <sys/types.h>

#include <stdio.h>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <cutils/properties.h>
#include <log/log.h>
#include <logwrap/logwrap.h>

void SetRequireSignature() {
  android::base::WriteStringToFile("1", "/proc/sys/fs/verity/require_signatures");
}

bool LoadKeyToKeyring(const char* data, size_t size) {
  FILE* fp = popen("/system/bin/mini-keyctl padd asymmetric fsv_system .fs-verity", "w");
  if (!fp) {
    ALOGE("popen failed: %s", strerror(errno));
    return false;
  }
  if (!android::base::WriteFully(fileno(fp), data, size)) {
    ALOGE("WriteFully failed: %s", strerror(errno));
    return false;
  }
  pclose(fp);
  return true;
}

void LoadKeyFromVerifiedPartitions() {
  const char* dir = "/product/etc/security/fsverity";
  if (!std::filesystem::exists(dir)) {
    ALOGD("no such dir: %s", dir);
    return;
  }
  for (const auto & entry : std::filesystem::directory_iterator(dir)) {
    if (!android::base::EndsWithIgnoreCase(entry.path().c_str(), ".der"))
      continue;
    std::string content;
    if (!android::base::ReadFileToString(entry.path(), &content)) {
      continue;
    }
    if (!LoadKeyToKeyring(content.c_str(), content.size())) {
      ALOGE("Failed to load key from %s", entry.path().c_str());
    }
  }
}

void RestrictKeyring() {
  const char* const args[] = {
    "/system/bin/mini-keyctl", "restrict_keyring", ".fs-verity",
  };
  android_fork_execvp_ext(arraysize(args), const_cast<char**>(args), nullptr, true,
                          LOG_ALOG, true, nullptr, nullptr, 0);
}

int main(int /*argc*/, const char** /*argv*/) {
  SetRequireSignature();
  LoadKeyFromVerifiedPartitions();
  if (!property_get_bool("ro.debuggable", false)) {
    RestrictKeyring();
  }
  return 0;
}
