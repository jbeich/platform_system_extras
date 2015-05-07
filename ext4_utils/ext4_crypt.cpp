/*
 * Copyright (c) 2015 Google, Inc.
 */

#define TAG "ext4_utils"

#include "ext4_crypt_init_extensions.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <asm/ioctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cutils/klog.h>

#include "unencrypted_properties.h"

#define XATTR_NAME_ENCRYPTION_POLICY "encryption.policy"
#define EXT4_KEYREF_DELIMITER ((char)'.')

// ext4enc:TODO Include structure from somewhere sensible
// MUST be in sync with ext4_crypto.c in kernel
#define EXT4_KEY_DESCRIPTOR_SIZE 8
struct ext4_encryption_policy {
    char version;
    char contents_encryption_mode;
    char filenames_encryption_mode;
    char flags;
    char master_key_descriptor[EXT4_KEY_DESCRIPTOR_SIZE];
} __attribute__((__packed__));

#define EXT4_ENCRYPTION_MODE_AES_256_XTS    1
#define EXT4_ENCRYPTION_MODE_AES_256_CTS    4

// ext4enc:TODO Get these values from somewhere sensible
#define EXT4_IOC_GET_ENCRYPTION_POLICY \
    _IOW('f', 21, struct ext4_encryption_policy)

#define EXT4_IOC_SET_ENCRYPTION_POLICY \
    _IOR('f', 19, struct ext4_encryption_policy)

/* Validate that all path items are available and accessible. */
static int is_path_valid(const char *path)
{
    if (access(path, W_OK)) {
        KLOG_ERROR(TAG, "Can't access %s: %s\n",strerror(errno), path);
        return 0;
    }

    return 1;
}

static int is_dir_empty(const char *dirname)
{
    int n = 0;
    struct dirent *d;
    DIR *dir;

    dir = opendir(dirname);
    while ((d = readdir(dir)) != NULL) {
        if (strcmp(d->d_name, "lost+found") == 0) {
            // Skip lost+found directory
        } else if (++n > 2) {
            break;
        }
    }
    closedir(dir);
    return n <= 2;
}

int do_policy_set(const char *directory, const char *policy, int policy_length)
{
    struct stat st;
    ssize_t ret;

    if (policy_length != EXT4_KEY_DESCRIPTOR_SIZE) {
        KLOG_ERROR(TAG, "Policy wrong length\n");
        return -EINVAL;
    }

    if (!is_path_valid(directory)) {
        KLOG_ERROR(TAG, "Directory %s does not exist\n", directory);
        return -EINVAL;
    }

    stat(directory, &st);
    if (!S_ISDIR(st.st_mode)) {
        KLOG_ERROR(TAG, "Can only set policy on a directory (%s)\n", directory);
        return -EINVAL;
    }

    int fd = open(directory, O_DIRECTORY);
    if (fd == -1) {
        KLOG_ERROR(TAG, "Failed to open directory (%s)\n", directory);
        return -EINVAL;
    }

    struct Closer {
        Closer(int fd) : fd_(fd) {}
        ~Closer() {close(fd_);}
        int fd_;
    };

    Closer closer(fd);

    ext4_encryption_policy eep_existing, eep_required;
    eep_required.version = 0;
    eep_required.contents_encryption_mode = EXT4_ENCRYPTION_MODE_AES_256_XTS;
    eep_required.filenames_encryption_mode = EXT4_ENCRYPTION_MODE_AES_256_CTS;
    eep_required.flags = 0;
    memcpy(eep_required.master_key_descriptor, policy,
           EXT4_KEY_DESCRIPTOR_SIZE);

    //memcpy(&eep_existing, 0, sizeof(eep_existing));
    memcpy(&eep_existing, &eep_required, sizeof(eep_existing));

    ret = ioctl(fd, EXT4_IOC_GET_ENCRYPTION_POLICY, &eep_existing);
    if (ret == 0) {
        if (memcmp(&eep_existing, &eep_required, sizeof(eep_existing)) == 0) {
            KLOG_INFO(TAG,
                      "Encryption policy for %s already to %02x%02x%02x%02x\n",
                      directory, policy[0], policy[1], policy[2], policy[3]);
            return 0;
        } else {
            const char* existing = eep_existing.master_key_descriptor;
            KLOG_ERROR(TAG,
                      "Encryption policy for %s is %02x%02x%02x%02x",
                      "which does not match %02x%02x%02x%02x",
                      directory,
                      existing[0], existing[1], existing[2], existing[3],
                      policy[0], policy[1], policy[2], policy[3]);
            return -EINVAL;
        }
    }

    // ext4enc:TODO remove this!
    KLOG_INFO(TAG, "Getting policy failed with message %s\n", strerror(errno));

    ret = ioctl(fd, EXT4_IOC_SET_ENCRYPTION_POLICY, &eep_required);
    if (ret) {
        KLOG_ERROR(TAG, "Failed to set encryption policy for %s: %s\n",
                   directory, strerror(errno));
        return -EINVAL;
    }

    KLOG_INFO(TAG,
              "Encryption policy for %s is set to %02x%02x%02x%02x\n",
              directory, policy[0], policy[1], policy[2], policy[3]);

    return 0;
}

bool e4crypt_non_default_key(const char* dir)
{
    UnencryptedProperties props(dir);
    return props.Get<int>(properties::is_default, 1) != 1;
}
