#define TAG "ext4_utils"

#include "ext4_crypt.h"

#include <string>
#include <fstream>
#include <map>

#include <errno.h>
#include <sys/mount.h>

#include <cutils/klog.h>
#include <cutils/properties.h>

#include "unencrypted_properties.h"

namespace {
    std::map<std::string, std::string> s_password_store;

    // ext4enc:TODO Include structure from somewhere sensible
    // MUST be in sync with ext4_crypto.c in kernel
    const int EXT4_MAX_KEY_SIZE = 76;
    struct ext4_encryption_key {
            uint32_t mode;
            char raw[EXT4_MAX_KEY_SIZE];
            uint32_t size;
    };
}

int e4crypt_enable(const char* path)
{
    UnencryptedProperties props(path);
    if (props.Get<std::string>(properties::key).empty()) {
        // Create new key since it doesn't already exist
        std::ifstream urandom("/dev/urandom", std::ifstream::binary);
        if (!urandom) {
            KLOG_ERROR(TAG, "Failed to open /dev/urandom\n");
            return -1;
        }

        // ext4enc:TODO Don't hardcode 32
        std::string key_material(32, '\0');
        urandom.read(&key_material[0], key_material.length());
        if (!urandom) {
            KLOG_ERROR(TAG, "Failed to read random bytes\n");
            return -1;
        }

        if (!props.Set(properties::key, key_material)) {
            KLOG_ERROR(TAG, "Failed to write key material");
            return -1;
        }
    }

    if (!props.Remove(properties::ref)) {
        KLOG_ERROR(TAG, "Failed to remove key ref\n");
        return -1;
    }

    return e4crypt_check_passwd(path, "");
}

bool e4crypt_non_default_key(const char* dir)
{
    int type = e4crypt_get_password_type(dir);

    // ext4enc:TODO Use consts, not 1 here
    return type != -1 && type != 1;
}

int e4crypt_get_password_type(const char* path)
{
    UnencryptedProperties props(path);
    if (props.Get<std::string>(properties::key).empty()) {
        KLOG_INFO(TAG, "No master key, so not ext4enc\n");
        return -1;
    }

    return props.Get<int>(properties::type, 1);
}

int e4crypt_change_password(const char* path, int crypt_type,
                            const char* password)
{
    // ext4enc:TODO Encrypt master key with password securely. Store hash of
    // master key for validation
    UnencryptedProperties props(path);
    if (   props.Set(properties::password, password)
        && props.Set(properties::type, crypt_type))
        return 0;
    return -1;
}

int e4crypt_crypto_complete(const char* path)
{
    KLOG_INFO(TAG, "ext4 crypto complete called on %s\n", path);
    if (UnencryptedProperties(path).Get<std::string>(properties::key).empty()) {
        KLOG_INFO(TAG, "No master key, so not ext4enc\n");
        return -1;
    }

    return 0;
}

int e4crypt_check_passwd(const char* path, const char* password)
{
    UnencryptedProperties props(path);
    auto key = props.Get<std::string>(properties::key);
    if (key.empty()) {
        KLOG_INFO(TAG, "No master key, so not ext4enc\n");
        return -1;
    }

    auto actual_password = props.Get<std::string>(properties::password);
    if (actual_password != password) {
        return -1;
    }

    s_password_store[path] = password;

    // Install password into global keyring
    ext4_encryption_key ext4_key = {0, {0}, 0};
    if (key.length() > sizeof(ext4_key.raw)) {
        KLOG_ERROR(TAG, "Key too long\n");
        return -1;
    }

    ext4_key.mode = 0;
    memcpy(ext4_key.raw, &key[0], key.length());
    ext4_key.size = key.length();

    // ext4enc:TODO Use better reference not 1234567890
    key_serial_t device_keyring = keyctl_search(KEY_SPEC_SESSION_KEYRING,
                                                "keyring", "e4crypt", 0);

    KLOG_INFO(TAG, "Found device_keyring - id is %d\n", device_keyring);

    key_serial_t key_id = add_key("logon", "ext4-key:1234567890",
                                  (void*)&ext4_key, sizeof(ext4_key),
                                  device_keyring);

    if (key_id == -1) {
        KLOG_ERROR(TAG,"Failed to insert key into keyring with error %s\n",
                   strerror(errno));
        return -1;
    }

    KLOG_INFO(TAG, "Added key %d to keyring %d in process %d\n",
              key_id, device_keyring, getpid());

    // ext4enc:TODO set correct permissions
    long result = keyctl_setperm(key_id, 0x3f3f3f3f);
    if (result) {
        KLOG_ERROR(TAG, "KEYCTL_SETPERM failed with error %ld\n", result);
        return -1;
    }

    // Save reference to key so we can set policy later
    if (!props.Set(properties::ref, "@s.ext4-key:1234567890")) {
        KLOG_ERROR(TAG, "Cannot save key reference\n");
        return -1;
    }

    return 0;
}

int e4crypt_restart(const char* path)
{
    int rc = 0;

    KLOG_INFO(TAG, "ext4 restart called on %s\n", path);
    property_set("vold.decrypt", "trigger_reset_main");
    KLOG_INFO(TAG, "Just asked init to shut down class main\n");
    sleep(2);

    std::string tmp_path = std::string() + path + "/tmp_mnt";

    // ext4enc:TODO add retry logic
    rc = umount(tmp_path.c_str());
    if (rc) {
        KLOG_ERROR(TAG, "umount %s failed with rc %d, msg %s\n",
                   tmp_path.c_str(), rc, strerror(errno));
        return rc;
    }

    // ext4enc:TODO add retry logic
    rc = umount(path);
    if (rc) {
        KLOG_ERROR(TAG, "umount %s failed with rc %d, msg %s\n",
                   path, rc, strerror(errno));
        return rc;
    }

    return 0;
}

const char* e4crypt_get_password(const char* path)
{
    // ext4enc:TODO scrub password after timeout
    auto i = s_password_store.find(path);
    if (i == s_password_store.end()) {
        return 0;
    } else {
        return i->second.c_str();
    }
}
