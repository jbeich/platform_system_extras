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

#include "vold_keymaster/Keymaster.h"

#include <android-base/logging.h>
#include <keymasterV4_1/authorization_set.h>
#include <keymasterV4_1/keymaster_utils.h>

namespace android {
namespace vold_keymaster {

using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::keymaster::V4_0::SecurityLevel;

KeymasterOperation::~KeymasterOperation() {
    if (mDevice) mDevice->abort(mOpHandle);
}

bool KeymasterOperation::updateCompletely(const char* input, size_t inputLen,
                                          const std::function<void(const char*, size_t)> consumer) {
    uint32_t inputConsumed = 0;

    km::ErrorCode km_error;
    auto hidlCB = [&](km::ErrorCode ret, uint32_t inputConsumedDelta,
                      const hidl_vec<km::KeyParameter>& /*ignored*/,
                      const hidl_vec<uint8_t>& _output) {
        km_error = ret;
        if (km_error != km::ErrorCode::OK) return;
        inputConsumed += inputConsumedDelta;
        consumer(reinterpret_cast<const char*>(&_output[0]), _output.size());
    };

    while (inputConsumed != inputLen) {
        size_t toRead = static_cast<size_t>(inputLen - inputConsumed);
        auto inputBlob = km::support::blob2hidlVec(
                reinterpret_cast<const uint8_t*>(&input[inputConsumed]), toRead);
        auto error = mDevice->update(mOpHandle, hidl_vec<km::KeyParameter>(), inputBlob,
                                     km::HardwareAuthToken(), km::VerificationToken(), hidlCB);
        if (!error.isOk()) {
            LOG(ERROR) << "update failed: " << error.description();
            mDevice = nullptr;
            return false;
        }
        if (km_error != km::ErrorCode::OK) {
            LOG(ERROR) << "update failed, code " << int32_t(km_error);
            mDevice = nullptr;
            return false;
        }
        if (inputConsumed > inputLen) {
            LOG(ERROR) << "update reported too much input consumed";
            mDevice = nullptr;
            return false;
        }
    }
    return true;
}

bool KeymasterOperation::finish(std::string* output) {
    km::ErrorCode km_error;
    auto hidlCb = [&](km::ErrorCode ret, const hidl_vec<km::KeyParameter>& /*ignored*/,
                      const hidl_vec<uint8_t>& _output) {
        km_error = ret;
        if (km_error != km::ErrorCode::OK) return;
        if (output) output->assign(reinterpret_cast<const char*>(&_output[0]), _output.size());
    };
    auto error = mDevice->finish(mOpHandle, hidl_vec<km::KeyParameter>(), hidl_vec<uint8_t>(),
                                 hidl_vec<uint8_t>(), km::HardwareAuthToken(),
                                 km::VerificationToken(), hidlCb);
    mDevice = nullptr;
    if (!error.isOk()) {
        LOG(ERROR) << "finish failed: " << error.description();
        return false;
    }
    if (km_error != km::ErrorCode::OK) {
        LOG(ERROR) << "finish failed, code " << int32_t(km_error);
        return false;
    }
    return true;
}

/* static */ bool Keymaster::hmacKeyGenerated = false;

Keymaster::Keymaster() {
    auto devices = KmDevice::enumerateAvailableDevices();
    if (!hmacKeyGenerated) {
        KmDevice::performHmacKeyAgreement(devices);
        hmacKeyGenerated = true;
    }
    for (auto& dev : devices) {
        // Do not use StrongBox for device encryption / credential encryption.  If a security chip
        // is present it will have Weaver, which already strengthens CE.  We get no additional
        // benefit from using StrongBox here, so skip it.
        if (dev->halVersion().securityLevel != SecurityLevel::STRONGBOX) {
            mDevice = std::move(dev);
            break;
        }
    }
    if (!mDevice) return;
    auto& version = mDevice->halVersion();
    LOG(INFO) << "Using " << version.keymasterName << " from " << version.authorName
              << " for encryption.  Security level: " << toString(version.securityLevel)
              << ", HAL: " << mDevice->descriptor() << "/" << mDevice->instanceName();
}

bool Keymaster::generateKey(const km::AuthorizationSet& inParams, std::string* key) {
    km::ErrorCode km_error;
    auto hidlCb = [&](km::ErrorCode ret, const hidl_vec<uint8_t>& keyBlob,
                      const km::KeyCharacteristics& /*ignored*/) {
        km_error = ret;
        if (km_error != km::ErrorCode::OK) return;
        if (key) key->assign(reinterpret_cast<const char*>(&keyBlob[0]), keyBlob.size());
    };

    auto error = mDevice->generateKey(inParams.hidl_data(), hidlCb);
    if (!error.isOk()) {
        LOG(ERROR) << "generate_key failed: " << error.description();
        return false;
    }
    if (km_error != km::ErrorCode::OK) {
        LOG(ERROR) << "generate_key failed, code " << int32_t(km_error);
        return false;
    }
    return true;
}

bool Keymaster::exportKey(const KeyBuffer& kmKey, std::string* key) {
    auto kmKeyBlob = km::support::blob2hidlVec(std::string(kmKey.data(), kmKey.size()));
    km::ErrorCode km_error;
    auto hidlCb = [&](km::ErrorCode ret, const hidl_vec<uint8_t>& exportedKeyBlob) {
        km_error = ret;
        if (km_error != km::ErrorCode::OK) return;
        if (key)
            key->assign(reinterpret_cast<const char*>(&exportedKeyBlob[0]), exportedKeyBlob.size());
    };
    auto error = mDevice->exportKey(km::KeyFormat::RAW, kmKeyBlob, {}, {}, hidlCb);
    if (!error.isOk()) {
        LOG(ERROR) << "export_key failed: " << error.description();
        return false;
    }
    if (km_error != km::ErrorCode::OK) {
        LOG(ERROR) << "export_key failed, code " << int32_t(km_error);
        return false;
    }
    return true;
}

bool Keymaster::deleteKey(const std::string& key) {
    auto keyBlob = km::support::blob2hidlVec(key);
    auto error = mDevice->deleteKey(keyBlob);
    if (!error.isOk()) {
        LOG(ERROR) << "delete_key failed: " << error.description();
        return false;
    }
    if (error != km::ErrorCode::OK) {
        LOG(ERROR) << "delete_key failed, code " << int32_t(km::ErrorCode(error));
        return false;
    }
    return true;
}

bool Keymaster::upgradeKey(const std::string& oldKey, const km::AuthorizationSet& inParams,
                           std::string* newKey) {
    auto oldKeyBlob = km::support::blob2hidlVec(oldKey);
    km::ErrorCode km_error;
    auto hidlCb = [&](km::ErrorCode ret, const hidl_vec<uint8_t>& upgradedKeyBlob) {
        km_error = ret;
        if (km_error != km::ErrorCode::OK) return;
        if (newKey)
            newKey->assign(reinterpret_cast<const char*>(&upgradedKeyBlob[0]),
                           upgradedKeyBlob.size());
    };
    auto error = mDevice->upgradeKey(oldKeyBlob, inParams.hidl_data(), hidlCb);
    if (!error.isOk()) {
        LOG(ERROR) << "upgrade_key failed: " << error.description();
        return false;
    }
    if (km_error != km::ErrorCode::OK) {
        LOG(ERROR) << "upgrade_key failed, code " << int32_t(km_error);
        return false;
    }
    return true;
}

KeymasterOperation Keymaster::begin(km::KeyPurpose purpose, const std::string& key,
                                    const km::AuthorizationSet& inParams,
                                    const km::HardwareAuthToken& authToken,
                                    km::AuthorizationSet* outParams) {
    auto keyBlob = km::support::blob2hidlVec(key);
    uint64_t mOpHandle;
    km::ErrorCode km_error;

    auto hidlCb = [&](km::ErrorCode ret, const hidl_vec<km::KeyParameter>& _outParams,
                      uint64_t operationHandle) {
        km_error = ret;
        if (km_error != km::ErrorCode::OK) return;
        if (outParams) *outParams = _outParams;
        mOpHandle = operationHandle;
    };

    auto error = mDevice->begin(purpose, keyBlob, inParams.hidl_data(), authToken, hidlCb);
    if (!error.isOk()) {
        LOG(ERROR) << "begin failed: " << error.description();
        return KeymasterOperation(km::ErrorCode::UNKNOWN_ERROR);
    }
    if (km_error != km::ErrorCode::OK) {
        LOG(ERROR) << "begin failed, code " << int32_t(km_error);
        return KeymasterOperation(km_error);
    }
    return KeymasterOperation(mDevice.get(), mOpHandle);
}

bool Keymaster::isSecure() {
    return mDevice->halVersion().securityLevel != km::SecurityLevel::SOFTWARE;
}

void Keymaster::earlyBootEnded() {
    auto devices = KmDevice::enumerateAvailableDevices();
    for (auto& dev : devices) {
        auto error = dev->earlyBootEnded();
        if (!error.isOk()) {
            LOG(ERROR) << "earlyBootEnded call failed: " << error.description() << " for "
                       << dev->halVersion().keymasterName;
        }
        km::V4_1_ErrorCode km_error = error;
        if (km_error != km::V4_1_ErrorCode::OK && km_error != km::V4_1_ErrorCode::UNIMPLEMENTED) {
            LOG(ERROR) << "Error reporting early boot ending to keymaster: "
                       << static_cast<int32_t>(km_error) << " for "
                       << dev->halVersion().keymasterName;
        }
    }
}

}  // namespace vold_keymaster
}  // namespace android
