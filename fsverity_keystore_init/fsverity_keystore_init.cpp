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

#define LOG_TAG "fsverity_keystore_init"

#include <errno.h>
#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <log/log.h>
#include <keystore/keystore_client.h>
#include <keystore/keystore_client_impl.h>
#include <keystore/keystore_get.h>

constexpr char KEY_PREFIX[] = "FSV_";
constexpr int KEY_UID = 1075;

std::unique_ptr<keystore::KeystoreClient> CreateKeystoreInstance() {
  return std::unique_ptr<keystore::KeystoreClient>(
      static_cast<keystore::KeystoreClient*>(new keystore::KeystoreClientImpl));
}

bool LoadKeyToKeyring(const char* data, size_t size) {
  std::unique_ptr<FILE, int (*)(FILE*)> fp(
    popen("/system/bin/mini-keyctl padd asymmetric fsv_user .fs-verity", "w"),
    pclose);
  if (!fp) {
    ALOGE("popen failed: %s", strerror(errno));
    return false;
  }
  if (!android::base::WriteFully(fileno(fp.get()), data, size)) {
    ALOGE("WriteFully failed: %s", strerror(errno));
    return false;
  }
  return true;
}

int main(int /*argc*/, const char** /*argv*/) {
  auto client = CreateKeystoreInstance();

  std::vector<std::string> aliases;
  if (client == nullptr || !client->listKeysOfUid(KEY_PREFIX, KEY_UID, &aliases)) {
    ALOGE("Failed to list key");
    return -1;
  }

  // Always try to load all keys even if some fails to load. The rest may still
  // be important to have.
  for (auto &alias : aliases) {
    auto blob = client->getKey(alias, KEY_UID);
    if (!LoadKeyToKeyring(reinterpret_cast<char*>(blob->data()), blob->size())) {
      ALOGE("Failed to load key %s from keyring", alias.c_str());
    }
  }
  return 0;
}
