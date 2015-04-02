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

#include "squashfs_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "squashfs_fs.h"

int squashfs_parse_sb(char *blk_device, struct squashfs_info *info) {
    struct squashfs_super_block sb;
    int data_device;
    data_device = TEMP_FAILURE_RETRY(open(blk_device, O_RDONLY | O_CLOEXEC));
    if (data_device == -1) {
        fprintf(stderr, "Error opening block device (%s)\n", strerror(errno));
        return -1;
    }

    if (TEMP_FAILURE_RETRY(read(data_device, &sb, sizeof(sb)))
            != sizeof(sb)) {
        fprintf(stderr, "Error reading superblock\n");
        TEMP_FAILURE_RETRY(close(data_device));
        return -1;
    }
    if (sb.s_magic != SQUASHFS_MAGIC) {
        fprintf(stderr, "Not a valid squashfs filesystem\n");
        TEMP_FAILURE_RETRY(close(data_device));
        return -1;
    }

    info->block_size = sb.block_size;
    info->inodes = sb.inodes;
    info->bytes_used = sb.bytes_used;
    // by default mksquashfs pads the filesystem to 4K blocks
    info->bytes_used_4K_padded =
        sb.bytes_used + (4096 - (sb.bytes_used & (4096 - 1)));

    TEMP_FAILURE_RETRY(close(data_device));
    return 0;
}
