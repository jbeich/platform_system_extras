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

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <ext2fs/ext2fs.h>

#if defined(__linux__)
# include <linux/fs.h>
# include <sys/ioctl.h>
# ifndef BLKDISCARD
#  define BLKDISCARD _IO(0x12,119)
# endif
# ifndef BLKSECDISCARD
#  define BLKSECDISCARD _IO(0x12,125)
# endif
# ifndef BLKZEROOUT
#  define BLKZEROOUT _IO(0x12,127)
# endif
#elif defined(__APPLE__)
# include <sys/disk.h>
# include <sys/ioctl.h>
#endif

#ifndef TEMP_FAILURE_RETRY
# define TEMP_FAILURE_RETRY(exp) ({            \
	typeof (exp) _rc;                      \
	do {                                   \
		_rc = (exp);                   \
	} while (_rc == -1 && errno == EINTR); \
	_rc; })
#endif

int ext4_read_superblock_fd(int fd, struct ext2_super_block *sb)
{
	if (TEMP_FAILURE_RETRY(lseek(fd, 1024, SEEK_SET)) == -1) {
		fprintf(stderr, "lseek: %d (%s)\n", errno, strerror(errno));
		return -1;
	}
	if (TEMP_FAILURE_RETRY(read(fd, sb, 1024)) != 1024) {
		fprintf(stderr, "read: %d (%s)\n", errno, strerror(errno));
		return -1;
	}
	return 0;
}

bool ext4_detect_fd(int fd)
{
	struct ext2_super_block sb;

	if (ext4_read_superblock_fd(fd, &sb) < 0)
		return false;
	return sb.s_magic == EXT2_SUPER_MAGIC;
}

bool ext4_detect(const char *file)
{
	bool ret;
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open file \"%s\": %d (%s)\n", file, errno, strerror(errno));
		return false;
	}
	ret = ext4_detect_fd(fd);
	close(fd);
	return ret;
}

int64_t ext4_get_volume_size_sb(struct ext2_super_block *sb)
{
	return (int64_t)EXT2_BLOCK_SIZE(sb) *
	       ((int64_t)sb->s_blocks_count + ((int64_t)sb->s_blocks_count_hi << 32));
}

int64_t ext4_get_volume_size_fd(int fd)
{
	int64_t size;
	struct ext2_super_block sb;

	if (ext4_read_superblock_fd(fd, &sb) == -1)
		return -1;

	if (sb.s_magic != EXT2_SUPER_MAGIC) {
		fprintf(stderr, "fd %d does not contain an ext4 partition\n", fd);
		return -1;
	}

	return ext4_get_volume_size_sb(&sb);
}

int64_t ext4_get_volume_size(const char *file)
{
	int64_t size;
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open file \"%s\": %d (%s)\n", file, errno, strerror(errno));
		return -1;
	}
	size = ext4_get_volume_size_fd(fd);
	close(fd);
	return size;
}

bool is_block_device_fd(int fd)
{
	struct stat st;
	int ret = fstat(fd, &st);
	if (ret < 0) {
		fprintf(stderr, "fstat: %d (%s)\n", errno, strerror(errno));
		return false;
	}
	return S_ISBLK(st.st_mode);
}

bool is_block_device(const char *file)
{
	bool ret;
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open file \"%s\": %d (%s)\n", file, errno, strerror(errno));
		return false;
	}
	ret = is_block_device_fd(fd);
	close(fd);
	return ret;
}

uint64_t get_block_device_size(int fd)
{
	int ret = 0;
	struct stat buf;
	uint64_t size = 0;

	ret = fstat(fd, &buf);
	if (ret)
		return 0;

	if (S_ISREG(buf.st_mode))
		return buf.st_size;
	else if ((S_ISBLK(buf.st_mode))) {
#if defined(__linux__)
		ret = ioctl(fd, BLKGETSIZE64, &size);
#elif defined(__APPLE__)
		ret = ioctl(fd, DKIOCGETBLOCKCOUNT, &size);
#else
        fprintf(stderr, "blkgetsize not implemented\n");
        return 0;
#endif
	}
	if (ret != 0)
		fprintf(stderr, "blkgetsize failed\n");
	return ret ? 0 : size;
}

uint64_t get_file_size(int fd)
{
	struct stat buf;
	int ret;
	uint64_t reserve_len = 0; //TODO: retrieved from info...
	int64_t computed_size;

	ret = fstat(fd, &buf);
	if (ret)
		return 0;

	if (S_ISREG(buf.st_mode))
		computed_size = buf.st_size - reserve_len;
	else if (S_ISBLK(buf.st_mode))
		computed_size = get_block_device_size(fd) - reserve_len;
	else
		computed_size = 0;

	return computed_size < 0 ? 0 : computed_size;
}
