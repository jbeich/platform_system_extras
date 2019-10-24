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

#ifndef _FSCRYPT_H_
#define _FSCRYPT_H_

#include <string>

bool fscrypt_is_native();

static const char* fscrypt_unencrypted_folder = "/unencrypted";
static const char* fscrypt_key_ref = "/unencrypted/ref";
static const char* fscrypt_key_per_boot_ref = "/unencrypted/per_boot_ref";
static const char* fscrypt_key_mode = "/unencrypted/mode";

namespace android {
namespace fscrypt {

struct Policy {
    int version;
    int contents_mode;
    int filenames_mode;

    Policy() : version(0) {}
};

struct PolicyKeyRef {
    Policy policy;
    std::string key_raw_ref;
};

void BytesToHex(const std::string& bytes, std::string* hex);

bool PolicyString(const Policy& policy, std::string* policy_string);

// Note that right now this parses only the output from FscryptPolicyString, not the
// more general format that appears in fstabs.
bool ParsePolicy(const std::string& policy_string, Policy* policy);

bool ParsePolicyParts(const std::string& contents_mode, const std::string& filenames_mode,
                      const std::string& flags, Policy* policy);

bool ParsePolicyParts(const std::string& contents_mode, const std::string& filenames_mode,
                      int policy_version, Policy* policy);
bool EnsurePolicy(const PolicyKeyRef& policy_key_ref, const std::string& directory);

}  // namespace fscrypt
}  // namespace android

#endif // _FSCRYPT_H_
