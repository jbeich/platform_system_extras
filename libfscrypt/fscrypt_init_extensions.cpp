/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "fscrypt/fscrypt.h"
#include "fscrypt/fscrypt_init_extensions.h"

#include <dirent.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <keyutils.h>
#include <logwrap/logwrap.h>

#define TAG "fscrypt"

static int set_system_de_policy_on(char const* dir);

int fscrypt_install_keyring()
{
    key_serial_t device_keyring = add_key("keyring", "fscrypt", 0, 0,
                                          KEY_SPEC_SESSION_KEYRING);

    if (device_keyring == -1) {
        PLOG(ERROR) << "Failed to create keyring";
        return -1;
    }

    LOG(INFO) << "Keyring created with id " << device_keyring << " in process " << getpid();

    return 0;
}

int fscrypt_set_directory_policy(const char* dir)
{
    if (!dir || strncmp(dir, "/data/", 6)) {
        return 0;
    }

    // Special-case /data/media/obb per b/64566063
    if (strcmp(dir, "/data/media/obb") == 0) {
        // Try to set policy on this directory, but if it is non-empty this may fail.
        set_system_de_policy_on(dir);
        return 0;
    }

    // Only set policy on first level /data directories
    // To make this less restrictive, consider using a policy file.
    // However this is overkill for as long as the policy is simply
    // to apply a global policy to all /data folders created via makedir
    if (strchr(dir + 6, '/')) {
        return 0;
    }

    // Special case various directories that must not be encrypted,
    // often because their subdirectories must be encrypted.
    // This isn't a nice way to do this, see b/26641735
    std::vector<std::string> directories_to_exclude = {
        "lost+found",
        "system_ce", "system_de",
        "misc_ce", "misc_de",
        "vendor_ce", "vendor_de",
        "media",
        "data", "user", "user_de",
        "apex", "preloads", "app-staging",
        "gsi",
    };
    std::string prefix = "/data/";
    for (const auto& d: directories_to_exclude) {
        if ((prefix + d) == dir) {
            LOG(INFO) << "Not setting policy on " << dir;
            return 0;
        }
    }
    return set_system_de_policy_on(dir);
}

static int set_system_de_policy_on(char const* dir) {
    std::string ref_filename = std::string("/data") + fscrypt_key_ref;
    std::string policy;
    if (!android::base::ReadFileToString(ref_filename, &policy)) {
        LOG(ERROR) << "Unable to read system policy to set on " << dir;
        return -1;
    }

    auto type_filename = std::string("/data") + fscrypt_key_mode;
    std::string modestring;
    if (!android::base::ReadFileToString(type_filename, &modestring)) {
        LOG(ERROR) << "Cannot read mode";
    }

    std::vector<std::string> modes = android::base::Split(modestring, ":");

    if (modes.size() < 1 || modes.size() > 2) {
        LOG(ERROR) << "Invalid encryption mode string: " << modestring;
        return -1;
    }

    LOG(INFO) << "Setting policy on " << dir;
    int result = fscrypt_policy_ensure(dir, policy.c_str(), policy.length(),
                                       modes[0].c_str(),
                                       modes.size() >= 2 ?
                                            modes[1].c_str() : "aes-256-cts");
    if (result) {
        LOG(ERROR) << android::base::StringPrintf(
            "Setting %02x%02x%02x%02x policy on %s failed!",
            policy[0], policy[1], policy[2], policy[3], dir);
        return -1;
    }

    return 0;
}
