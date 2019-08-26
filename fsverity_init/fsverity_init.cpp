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
#include <keystore/keystore_client.h>
#include <keystore/keystore_client_impl.h>
#include <keystore/keystore_get.h>

constexpr char KEY_PREFIX[] = "fsv_";
constexpr int KEY_UID = 1075;

void SetRequireSignature() {
  android::base::WriteStringToFile("1", "/proc/sys/fs/verity/require_signatures");
}

std::unique_ptr<keystore::KeystoreClient> CreateKeystoreInstance() {
  return std::unique_ptr<keystore::KeystoreClient>(
      static_cast<keystore::KeystoreClient*>(new keystore::KeystoreClientImpl));
}

bool LoadKeyToKeyring(bool is_sideloaded, const char* data, size_t size) {
  FILE* fp;
  if (is_sideloaded) {
    fp = popen("/system/bin/mini-keyctl padd asymmetric fsv_user .fs-verity", "w");
  } else {
    fp = popen("/system/bin/mini-keyctl padd asymmetric fsv_system .fs-verity", "w");
  }
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
    ALOGD("LoadKeyFromVerifiedPartitions");
  const char* dir = "/product/etc/security/fsverity";
  if (!std::filesystem::exists(dir)) {
    ALOGD("no such dir: %s", dir);
    return;
  }
  // TODO how is b/132323675 happening?
  // TODO make sure key loads
  for (const auto & entry : std::filesystem::directory_iterator(dir)) {
    ALOGE("loading key from %s", entry.path().c_str());
    printf("DD %s\n", entry.path().c_str());
    if (!android::base::EndsWithIgnoreCase(entry.path().c_str(), ".der"))
      continue;
    std::string content;
    if (!android::base::ReadFileToString(entry.path(), &content)) {
      continue;
    }
    if (!LoadKeyToKeyring(false, content.c_str(), content.size())) {
      ALOGE("Failed to load key from %s", entry.path().c_str());
    }
  }
}

void LoadKeysFromKeystore() {
    ALOGD("LoadKeysFromKeystore");
    // This needs to happen when keystore is registered in binder
  auto client = CreateKeystoreInstance();

  std::vector<std::string> aliases;
    ALOGD("LoadKeysFromKeystore %p", &aliases);
  if (client == nullptr || !client->listKeysOfUid(KEY_PREFIX, KEY_UID, &aliases)) {
    ALOGE("Failed to list key");
    return;
  }

  // Always try to load all keys even if some fails to load. The rest may still
  // be important to have.
  for (auto &alias : aliases) {
    ALOGD("LoadKeysFromKeystore alias: %s", alias.c_str());
    auto blob = client->getKey(alias, KEY_UID);
    if (!LoadKeyToKeyring(true, reinterpret_cast<char*>(blob->data()), blob->size())) {
      ALOGE("Failed to load key %s from keyring", alias.c_str());
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
  LoadKeysFromKeystore();
  if (!property_get_bool("ro.debuggable", false)) {
    RestrictKeyring();
  }
  return 0;
}
