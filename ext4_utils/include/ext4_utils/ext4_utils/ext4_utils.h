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

#ifndef EXT4_UTILS_H
# define EXT4_UTILS_H

# ifdef __cplusplus
extern "C" {
# endif

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif

# include <stdint.h>
# include <stdbool.h>
# include <ext2fs/ext2fs.h>

/* ext2 and ext4 use the same structures */
# define ext4_super_block ext2_super_block
# define EXT4_SUPER_MAGIC EXT2_SUPER_MAGIC

/* EXT4 utils */
bool ext4_detect_fd(int fd);
bool ext4_detect(const char *dev);

int64_t ext4_get_volume_size_sb(struct ext2_super_block *sb);
int64_t ext4_get_volume_size_fd(int fd);
int64_t ext4_get_volume_size(const char *dev);
int ext4_read_superblock_fd(int fd, struct ext2_super_block *sb);

/* Block device utils */
bool is_block_device_fd(int fd);
bool is_block_device(const char *dev);
uint64_t get_block_device_size(int fd);
uint64_t get_file_size(int fd);

# ifdef __cplusplus
}
# endif

#endif
