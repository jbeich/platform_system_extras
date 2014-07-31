/*
 * Copyright (C) 2014 The Android Open Source Project
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
 *
 * This implementation of FAT32 formatting is provided to fulfil the
 * needs of UEFI devices.
 *
 * This implementation relies on the following documents:
 * - http://staff.washington.edu/dittrich/misc/fatgen103.pdf
 * - http://www.gnu.org/software/mtools/manual/fat_size_calculation.pdf
 *
 * No proper boot code is provided with this implementation since we
 * do not need this for UEFI.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sparse/sparse.h>
#include <time.h>
#include <sys/time.h>

#include "make_vfat.h"

#ifndef bswap16
#define bswap16(__x) (((((__x) & 0xFF00) >> 8) | (((__x) & 0xFF) << 8)))
#define bswap32(__x) (((((__x) & 0xFF) << 24) | (((__x) & 0xFF00) << 8)) \
		(((__x) & 0xFF0000) >> 8) | (((__x) & 0xFF000000) >> 24))
#endif	/* bswap16 */

#ifdef USE_MINGW
#include <winsock2.h>
#include <sys/param.h>
#if BYTE_ORDER == LITTLE_ENDIAN
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#else  /* BYTE_ORDER == LITTLE_ENDIAN */
#define	htole16(x)	bswap16((x))
#define	htole32(x)	bswap32((x))
#endif	/* BYTE_ORDER == LITTLE_ENDIAN */
#endif   /* USE_MINGW */

static const size_t   SECTOR_SIZE	 = 512;
static const uint8_t  CLUSTER_SIZE       = 1;
static const uint8_t  FAT_NB             = 2;
static const uint16_t ROOT_DIR_SIZE	 = 512;
static const uint16_t SECTOR_PER_TRACK   = 32;
static const uint16_t BACKUP_BOOT_SECTOR = 6;
static const uint16_t RESERVED_SECTOR_NB = 32;
static const uint16_t HEAD_NB            = 64;

static const uint8_t  DISK_TYPE          = 0xF8;
static const uint8_t  FAT_SIGNATURE	 = 0x29;
static const uint8_t  VOLUME_ATTRIBUTE   = 0x8;

struct __attribute__((__packed__)) bpb {
	/* Common */
	uint8_t		jump[3];
	char		program_name[8];
	uint16_t	bytes_per_sector;
	uint8_t		cluster_size;
	uint16_t	reserved_sector_nb;
	uint8_t		FAT_nb;
	uint16_t	root_directory_size;
	uint16_t	total_sectors;
	uint8_t		disk_type;
	uint16_t	obsolete;	/* FAT16 FAT_size */
	uint16_t	sector_per_track;
	uint16_t	head_nb;
	uint32_t	hidden_sector;
	uint32_t	sector_nb;

	/* FAT32 */
	uint32_t	FAT_size;
	uint16_t	disk_attr;
	uint8_t		maj;
	uint8_t		min;
	uint32_t	first_cluster_nb;
	uint16_t	disk_info_add;
	uint16_t	backup_boot_sector;
	uint8_t		reserved[12];
	uint8_t		disk_id;
	uint8_t		reserved2;
	uint8_t		signature;
	char		serial[4];
	char		name[11];
	char		file_system_type[8];
	char		boot_code[420];
	uint8_t		boot_sig[2];
};

static void copy_volname(char *dst, const char *volname, size_t size)
{
	size_t i = 0;

	if (volname)
		for (; i < strlen(volname) && i < size ; i++)
			dst[i] = volname[i];

	for (; i <= size ; i++)
		dst[i] = ' ';
}

static void build_bios_parameter_block(char *buf, int sector_nb,
				       uint32_t FAT_size, const char *volname)
{
	struct bpb *header = (struct bpb *)buf;

	strncpy(header->program_name, "VFATUTIL", sizeof(header->program_name));

	header->bytes_per_sector = htole16(SECTOR_SIZE);
	header->cluster_size = CLUSTER_SIZE;
	header->reserved_sector_nb = htole16(RESERVED_SECTOR_NB);
	header->FAT_nb = FAT_NB;
	header->disk_type = DISK_TYPE;
	header->sector_nb = htole32(sector_nb);
	header->head_nb = htole16(HEAD_NB);
	header->first_cluster_nb = htole32(2);
	header->backup_boot_sector = htole16(BACKUP_BOOT_SECTOR);
	header->FAT_size = htole32(FAT_size);
	header->disk_info_add = htole32(1);
	header->signature = FAT_SIGNATURE;
	header->sector_per_track = SECTOR_PER_TRACK;

	copy_volname(header->name, volname, sizeof(header->name));
	strncpy(header->file_system_type, "FAT32   ",
		sizeof(header->file_system_type));

	header->boot_sig[0] = 0x55;
	header->boot_sig[1] = 0xAA;

	memcpy(buf + (BACKUP_BOOT_SECTOR * SECTOR_SIZE), buf, SECTOR_SIZE);
}

static void build_file_table_allocation(char *buf)
{
	*((uint32_t *)buf)     = htole32(0x0FFFFFF8);
	*((uint32_t *)buf + 1) = htole32(0x0FFFFFFF);
	*((uint32_t *)buf + 2) = htole32(0x0FFFFFF8);
}

struct __attribute__((__packed__)) root_directory {
	char     name[11];
	uint8_t  attribute;
	uint8_t  reserved;
	uint8_t  hour;
	uint16_t create_time;
	uint16_t create_date;
	uint16_t last_access_date;
	uint16_t index;
	uint16_t last_change_time;
	uint16_t last_change_date;
	uint16_t first_cluster;
	uint32_t file_size;
};

static void build_root_directory(char *root_dir_sector, const char *volname)
{
	struct root_directory *root = (struct root_directory *)root_dir_sector;
	static struct timeval cur_time;
	struct tm *ctime;
	uint16_t create_time, create_date;

	copy_volname(root->name, volname, sizeof(root->name));
	root->attribute = VOLUME_ATTRIBUTE;

	gettimeofday(&cur_time, NULL);
	ctime = localtime(&cur_time.tv_sec);
	create_time = (uint16_t)((ctime->tm_sec >> 1) +
				 (ctime->tm_min << 5) +
				 (ctime->tm_hour << 11));
	create_date = (uint16_t)(ctime->tm_mday +
				 ((ctime->tm_mon + 1) << 5) +
				 ((ctime->tm_year - 80) << 9));

	root->create_time = htole16(create_time);
	root->create_date = htole16(create_date);

	root->last_access_date = root->create_date;
	root->last_change_time = root->create_time;
	root->last_change_date = root->create_date;
}

static const uint32_t LEAD_SIG   = 0x41615252;
static const uint32_t STRUCT_SIG = 0x61417272;
static const uint32_t TRAIL_SIG  = 0xAA550000;

struct __attribute__((__packed__)) fsinfo {
	uint32_t lead_sig;
	uint8_t  reserved1[480];
	uint32_t struct_sig;
	uint32_t free_count;
	uint32_t next_free;
	uint8_t  reserved2[12];
	uint32_t trail_sig;
};

static void build_fsinfo(char *fsinfo_sector, uint32_t sector_nb,
			 uint32_t FAT_size)
{
	struct fsinfo *info = (struct fsinfo *)fsinfo_sector;
	uint32_t free_sec = sector_nb - (FAT_size * FAT_NB);

	info->lead_sig = htole32(LEAD_SIG);
	info->struct_sig = htole32(STRUCT_SIG);
	info->free_count = htole32(free_sec / CLUSTER_SIZE);
	info->next_free = htole32(2);
	info->trail_sig = htole32(TRAIL_SIG);
}

static void *build_vfat_header(size_t len, const char *volname,
			       size_t *buf_size)
{
	uint32_t sector_nb = len / SECTOR_SIZE;
	uint32_t FAT_size = ((sector_nb - RESERVED_SECTOR_NB) * 8)
		/ (2 * (CLUSTER_SIZE * SECTOR_SIZE) + (FAT_NB * 8));
	char *buf, *cur;
	uint8_t i;

	*buf_size = (RESERVED_SECTOR_NB * SECTOR_SIZE)
		+ (((FAT_NB * FAT_size) + 1) * SECTOR_SIZE);

	cur = buf = calloc(1, *buf_size);
	if (buf == NULL) {
		perror("vfat failed to allocate FAT header.");
		return NULL;
	}

	build_bios_parameter_block(cur, sector_nb, FAT_size, volname);
	build_fsinfo(cur + SECTOR_SIZE, sector_nb, FAT_size);

	cur += (RESERVED_SECTOR_NB * SECTOR_SIZE);

	for (i = 0 ; i < FAT_NB ; i++) {
		build_file_table_allocation(cur);
		cur += FAT_size * SECTOR_SIZE;
	}

	build_root_directory(cur, volname);

	return buf;
}

int make_vfat_sparse_fd(int fd, long long len)
{
	struct sparse_file *sfile;
	void *vfathd = NULL;
	size_t vfathd_size;
	int ret = -1;

	/* Verify minimum size requirement. */
	if (len < ((RESERVED_SECTOR_NB * SECTOR_SIZE)
		   + (SECTOR_SIZE * FAT_NB) + SECTOR_SIZE))
		return -1;

	sfile = sparse_file_new(4096, len);
	if (!sfile)
		return -1;

	vfathd = build_vfat_header(len, "VOLUME", &vfathd_size);
	if (!vfathd)
		goto exit;

	if (sparse_file_add_data(sfile, vfathd, vfathd_size, 0))
		goto exit;

	ret = sparse_file_write(sfile, fd, 0, 1, 0);
	if (ret)
		goto exit;

	ret = 0;

exit:
	free(vfathd);
	sparse_file_destroy(sfile);
	return ret;
}
