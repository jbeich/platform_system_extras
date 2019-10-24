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

#include "fscrypt/fscrypt.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <asm/ioctl.h>
#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <logwrap/logwrap.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utils/misc.h>

#include <array>
#include <string>
#include <vector>

using namespace std::string_literals;

// TODO: switch to <linux/fscrypt.h> once it's in Bionic
#ifndef FSCRYPT_POLICY_V1

// Careful: due to an API quirk this is actually 0, not 1.  We use 1 everywhere
// else, so make sure to only use this constant in the ioctl itself.
#define FSCRYPT_POLICY_V1 0
#define FSCRYPT_KEY_DESCRIPTOR_SIZE 8
struct fscrypt_policy_v1 {
    __u8 version;
    __u8 contents_encryption_mode;
    __u8 filenames_encryption_mode;
    __u8 flags;
    __u8 master_key_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
};

#define FSCRYPT_POLICY_V2 2
#define FSCRYPT_KEY_IDENTIFIER_SIZE 16
struct fscrypt_policy_v2 {
    __u8 version;
    __u8 contents_encryption_mode;
    __u8 filenames_encryption_mode;
    __u8 flags;
    __u8 __reserved[4];
    __u8 master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
};

#endif /* FSCRYPT_POLICY_V1 */

/* modes not supported by upstream kernel, so not in <linux/fs.h> */
#define FS_ENCRYPTION_MODE_AES_256_HEH      126
#define FS_ENCRYPTION_MODE_PRIVATE          127

#define HEX_LOOKUP "0123456789abcdef"

#define MAX_KEY_REF_SIZE_HEX (2 * FSCRYPT_KEY_IDENTIFIER_SIZE + 1)

struct ModeLookupEntry {
    std::string name;
    int id;
};

static const auto contents_modes = std::vector<ModeLookupEntry>{
        {"aes-256-xts"s, FS_ENCRYPTION_MODE_AES_256_XTS},
        {"software"s, FS_ENCRYPTION_MODE_AES_256_XTS},
        {"adiantum"s, FS_ENCRYPTION_MODE_ADIANTUM},
        {"ice"s, FS_ENCRYPTION_MODE_PRIVATE},
};

static const auto filenames_modes = std::vector<ModeLookupEntry>{
        {"aes-256-cts"s, FS_ENCRYPTION_MODE_AES_256_CTS},
        {"aes-256-heh"s, FS_ENCRYPTION_MODE_AES_256_HEH},
        {"adiantum"s, FS_ENCRYPTION_MODE_ADIANTUM},
};

static bool LookupModeByName(const std::vector<struct ModeLookupEntry>& modes,
                             const std::string& name, int* result) {
    for (const auto& e : modes) {
        if (e.name == name) {
            *result = e.id;
            return true;
        }
    }
    return false;
}

static bool LookupModeById(const std::vector<struct ModeLookupEntry>& modes, int id,
                           std::string* result) {
    for (const auto& e : modes) {
        if (e.id == id) {
            *result = e.name;
            return true;
        }
    }
    return false;
}

bool fscrypt_is_native() {
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.crypto.type", value, "none");
    return !strcmp(value, "file");
}

static void log_ls(const char* dirname) {
    std::array<const char*, 3> argv = {"ls", "-laZ", dirname};
    int status = 0;
    auto res =
        logwrap_fork_execvp(argv.size(), argv.data(), &status, false, LOG_ALOG, false, nullptr);
    if (res != 0) {
        PLOG(ERROR) << argv[0] << " " << argv[1] << " " << argv[2] << "failed";
        return;
    }
    if (!WIFEXITED(status)) {
        LOG(ERROR) << argv[0] << " " << argv[1] << " " << argv[2]
                   << " did not exit normally, status: " << status;
        return;
    }
    if (WEXITSTATUS(status) != 0) {
        LOG(ERROR) << argv[0] << " " << argv[1] << " " << argv[2]
                   << " returned failure: " << WEXITSTATUS(status);
        return;
    }
}

void StrToHex(const std::string& str, std::string* hex) {
    for (char c : str) {
        *hex += HEX_LOOKUP[(c & 0xF0) >> 4];
        *hex += HEX_LOOKUP[c & 0x0F];
    }
}

static uint8_t fscrypt_get_policy_flags(const Policy& policy) {
    uint8_t flags = 0;

    // In the original setting of v1 policies and AES-256-CTS we used 4-byte
    // padding of filenames, so we have to retain that for compatibility.
    //
    // For everything else, use 16-byte padding.  This is more secure (it helps
    // hide the length of filenames), and it makes the inputs evenly divisible
    // into cipher blocks which is more efficient for encryption and decryption.
    if (policy.version == 1 && policy.filenames_mode == FS_ENCRYPTION_MODE_AES_256_CTS) {
        flags |= FS_POLICY_FLAGS_PAD_4;
    } else {
        flags |= FS_POLICY_FLAGS_PAD_16;
    }

    // Use DIRECT_KEY for Adiantum, since it's much more efficient but just as
    // secure since Android doesn't reuse the same master key for multiple
    // encryption modes.
    if (policy.filenames_mode == FS_ENCRYPTION_MODE_ADIANTUM) {
        flags |= FS_POLICY_FLAG_DIRECT_KEY;
    }

    return flags;
}

static bool fscrypt_is_encrypted(int fd) {
    fscrypt_policy_v1 policy;

    // success => encrypted with v1 policy
    // EINVAL => encrypted with v2 policy
    // ENODATA => not encrypted
    return ioctl(fd, FS_IOC_GET_ENCRYPTION_POLICY, &policy) == 0 || errno == EINVAL;
}

bool FscryptPolicyString(const Policy& policy, std::string* policy_string) {
    std::string contents_mode, filenames_mode;
    if (!LookupModeById(contents_modes, policy.contents_mode, &contents_mode)) {
        return false;
    }
    if (!LookupModeById(filenames_modes, policy.filenames_mode, &filenames_mode)) {
        return false;
    }
    *policy_string = contents_mode + ":" + filenames_mode + ":v" + std::to_string(policy.version);
    return true;
}

bool FscryptParsePolicy(const std::string& policy_string, Policy* policy) {
    auto parts = android::base::Split(policy_string, ":");

    if (parts.size() != 3) return false;

    return FscryptParsePolicyParts(parts[0], parts[1], parts[2], policy);
}

bool FscryptParsePolicyParts(const std::string& contents_mode, const std::string& filenames_mode,
                             const std::string& flags, Policy* policy) {
    int policy_version;
    if (flags == "v1") {
        policy_version = 1;
    } else if (flags == "v2") {
        policy_version = 2;
    } else {
        LOG(ERROR) << "Unknown flag: " << flags;
        return false;
    }
    return FscryptParsePolicyParts(contents_mode, filenames_mode, policy_version, policy);
}

bool FscryptParsePolicyParts(const std::string& contents_mode, const std::string& filenames_mode,
                             int policy_version, Policy* policy) {
    if (!LookupModeByName(contents_modes, contents_mode, &policy->contents_mode)) {
        LOG(ERROR) << "Invalid file contents encryption mode: " << contents_mode;
        return false;
    }
    if (!LookupModeByName(filenames_modes, filenames_mode, &policy->filenames_mode)) {
        LOG(ERROR) << "Invalid file names encryption mode: " << filenames_mode;
        return false;
    }

    policy->version = policy_version;
    return true;
}

bool FscryptPolicyEnsure(const PolicyKeyRef& policy_key_ref, const std::string& directory) {
    union {
        fscrypt_policy_v1 v1;
        fscrypt_policy_v2 v2;
    } kern_policy;
    memset(&kern_policy, 0, sizeof(kern_policy));

    switch (policy_key_ref.policy.version) {
        case 1:
            if (policy_key_ref.key_raw_ref.size() != FSCRYPT_KEY_DESCRIPTOR_SIZE) {
                LOG(ERROR) << "Invalid key ref length for v1 policy: "
                           << policy_key_ref.key_raw_ref.size();
                return false;
            }
            // Careful: FSCRYPT_POLICY_V1 is actually 0 in the API, so make sure
            // to use it here instead of a literal 1.
            kern_policy.v1.version = FSCRYPT_POLICY_V1;
            kern_policy.v1.contents_encryption_mode = policy_key_ref.policy.contents_mode;
            kern_policy.v1.filenames_encryption_mode = policy_key_ref.policy.filenames_mode;
            kern_policy.v1.flags = fscrypt_get_policy_flags(policy_key_ref.policy);
            policy_key_ref.key_raw_ref.copy(
                    reinterpret_cast<char*>(kern_policy.v1.master_key_descriptor),
                    FSCRYPT_KEY_DESCRIPTOR_SIZE);
            break;
        case 2:
            if (policy_key_ref.key_raw_ref.size() != FSCRYPT_KEY_IDENTIFIER_SIZE) {
                LOG(ERROR) << "Invalid key ref length for v2 policy: "
                           << policy_key_ref.key_raw_ref.size();
                return false;
            }
            kern_policy.v2.version = FSCRYPT_POLICY_V2;
            kern_policy.v2.contents_encryption_mode = policy_key_ref.policy.contents_mode;
            kern_policy.v2.filenames_encryption_mode = policy_key_ref.policy.filenames_mode;
            kern_policy.v2.flags = fscrypt_get_policy_flags(policy_key_ref.policy);
            policy_key_ref.key_raw_ref.copy(
                    reinterpret_cast<char*>(kern_policy.v2.master_key_identifier),
                    FSCRYPT_KEY_IDENTIFIER_SIZE);
            break;
        default:
            LOG(ERROR) << "Invalid encryption policy version: " << policy_key_ref.policy.version;
            return false;
    }

    std::string ref;
    StrToHex(policy_key_ref.key_raw_ref, &ref);
    ref += " modes "s + std::to_string(policy_key_ref.policy.contents_mode) + "/" +
           std::to_string(policy_key_ref.policy.filenames_mode);

    android::base::unique_fd fd(open(directory.c_str(), O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (fd == -1) {
        PLOG(ERROR) << "Failed to open directory " << directory;
        return false;
    }

    bool already_encrypted = fscrypt_is_encrypted(fd);

    // FS_IOC_SET_ENCRYPTION_POLICY will set the policy if the directory is
    // unencrypted; otherwise it will verify that the existing policy matches.
    // Setting the policy will fail if the directory is already nonempty.
    if (ioctl(fd, FS_IOC_SET_ENCRYPTION_POLICY, &kern_policy) != 0) {
        std::string reason;
        switch (errno) {
            case EEXIST:
                reason = "The directory already has a different encryption policy.";
                break;
            default:
                reason = strerror(errno);
                break;
        }
        LOG(ERROR) << "Failed to set encryption policy of " << directory << " to " << ref << ": "
                   << reason;
        if (errno == ENOTEMPTY) {
            log_ls(directory.c_str());
        }
        return false;
    }

    if (already_encrypted) {
        LOG(INFO) << "Verified that " << directory << " has the encryption policy " << ref;
    } else {
        LOG(INFO) << "Encryption policy of " << directory << " set to " << ref;
    }
    return true;
}
