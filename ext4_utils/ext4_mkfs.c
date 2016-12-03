#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

#include <create_inode.h>
#include <ext2fs/ext2fs.h>
#include <ext4_utils/make_ext4fs.h>

#if defined(_WIN32)
void sha256_uuid_generate(const char *ns, const char *name, uint8_t out[16]);
# else
#include <uuid/uuid.h>
#endif

/* Valid values: 0 (1024), 1 (2048), 2 (4096) */
#define BLOCK_SIZE_LOG 2
#define BLOCK_SIZE (1024 << BLOCK_SIZE_LOG)
#define INODE_RATIO 16384

int make_ext4fs_internal(int fd, long long len, const char *mountpoint,
			 struct selabel_handle *sehnd, const char *directory, bool sparse)
{
	(void)sehnd;
	(void)directory;

	char buf[32];
	ext2_filsys fs;
	int flags, dup_fd;
	errcode_t retval = 0;
	struct ext2_super_block sb;
        char *sparse_io_params = NULL;
	uint64_t blocks_count = len / BLOCK_SIZE;

	memset(&sb, 0, sizeof(sb));
	sb.s_blocks_count = blocks_count;
	sb.s_inodes_count = len / INODE_RATIO;
	sb.s_inode_size = 256;
	sb.s_rev_level = 1;
	sb.s_errors = EXT2_ERRORS_CONTINUE;
	sb.s_feature_compat = EXT2_FEATURE_COMPAT_EXT_ATTR
	    | EXT3_FEATURE_COMPAT_HAS_JOURNAL
	    | EXT4_FEATURE_COMPAT_SPARSE_SUPER2;
	sb.s_feature_ro_compat = EXT4_FEATURE_RO_COMPAT_DIR_NLINK
			       | EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE
			       | EXT2_FEATURE_RO_COMPAT_LARGE_FILE;
	sb.s_feature_incompat = EXT3_FEATURE_INCOMPAT_EXTENTS
			      | EXT2_FEATURE_INCOMPAT_FILETYPE
			      | EXT2_FEATURE_INCOMPAT_META_BG
			      | EXT4_FEATURE_INCOMPAT_FLEX_BG;
	/* Not sure if the OTA tools can handle inline data. */
			      //| EXT4_FEATURE_INCOMPAT_64BIT;
			      //| EXT4_FEATURE_INCOMPAT_INLINE_DATA
	sb.s_log_block_size = BLOCK_SIZE_LOG;
	flags = IO_FLAG_RW | EXT2_FLAG_RW | EXT2_FLAG_64BITS;

	dup_fd = dup(fd);
	if (dup_fd == -1) {
		fprintf(stderr, "Error %d: duplicating fd\n", errno);
		return errno;
	}
	if (sparse) {
                if (asprintf(&sparse_io_params, "%d:%llu:%u", dup_fd,
			    (unsigned long long)blocks_count, BLOCK_SIZE)
			== -1) {
			close(dup_fd);
			return -ENOMEM;
		}
		retval = ext2fs_initialize(sparse_io_params, flags, &sb,
					   sparsefd_io_manager, &fs);
		free(sparse_io_params);
	} else {
	        snprintf(buf, 32, "%d", dup_fd);
		retval = ext2fs_initialize(buf, flags, &sb, unixfd_io_manager, &fs);
        }
	if (retval) {
		fprintf(stderr, "initializing superblock in fd:%d", fd);
		close(dup_fd);
		return retval;
	}

	/* Generate UUID */
#if defined(_WIN32)
	sha256_uuid_generate("libext2fs", mountpoint ? mountpoint : "",
			     fs->super->s_uuid);
#else
	uuid_generate(fs->super->s_uuid);
#endif

	/* Set creator OS */
	sb.s_creator_os = EXT2_OS_LINUX;

	if (mountpoint) {
		/* Set last mount directory */
		memset(fs->super->s_last_mounted, 0, sizeof(fs->super->s_last_mounted));
		strncpy(fs->super->s_last_mounted, mountpoint, sizeof(fs->super->s_last_mounted));

		/* Set the volume label */
		memset(fs->super->s_volume_name, 0, sizeof(fs->super->s_volume_name));
		strncpy(fs->super->s_volume_name, mountpoint, sizeof(fs->super->s_volume_name));
	}

	/* Alloc blocks/inodes tables */
	retval = ext2fs_allocate_tables(fs);
	if (retval) {
		fprintf(stderr, "Error %ld: allocating filesystem tables\n", retval);
		goto err_after_init;
	}
	ext2fs_clear_inode_bitmap(fs->inode_map);
	retval = ext2fs_convert_subcluster_bitmap(fs, &fs->block_map);
	if (retval) {
		fprintf(stderr, "Error %ld: converting subcluster bitmap\n", retval);
		goto err_after_init;
	}
	write_inode_tables(fs, 0, 0, 0);

	/* Create special inodes */
	create_root_dir(fs, 0, 0);
	create_lost_and_found(fs);
	reserve_inodes(fs);
	create_bad_block_inode(fs, 0);

	/* Create journal */
	int journal_blocks = ext2fs_default_journal_size(ext2fs_blocks_count(fs->super));
	if (journal_blocks < 0) {
		fprintf(stderr, "File system too small for a journal\n");
		retval = -EINVAL;
		goto err_after_init;
	}
	retval = ext2fs_add_journal_inode2(fs, journal_blocks, ~0ULL, EXT2_MKJOURNAL_NO_MNT_CHECK);
	if (retval) {
		fprintf(stderr, "Error %ld: converting subcluster bitmap\n", retval);
		goto err_after_init;
	}

	/* Copy files from the specified directory */
        //TODO: enable it (not supported on windows)
#if 0
	if (directory) {
		save_working_dir();
		retval = populate_fs(fs, EXT2_ROOT_INO, src_root_dir, EXT2_ROOT_INO);
		restore_working_dir();
		if (retval) {
			fprintf(stderr, "Error %ld: populating file system\n", retval);
			goto err_after_init;
		}
	}
#endif

	ext2fs_mark_super_dirty(fs);
	retval = ext2fs_close_free(&fs);
	if (retval) {
		fprintf(stderr, "Error %ld: writing superblock\n", retval);
		return retval;
	}
	return 0;

err_after_init:
	ext2fs_close(fs);
	return retval;
}

int make_ext4fs_directory(const char *filename, long long len, const char *mountpoint,
			  struct selabel_handle *sehnd, const char *directory)
{
	int ret;
	int fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
	if (fd == -1)
		return -1;
	ret = make_ext4fs_internal(fd, len, mountpoint, sehnd, directory, false);
	close(fd);
	return ret;
}

int make_ext4fs(const char *filename, long long len, const char *mountpoint,
		struct selabel_handle *sehnd)
{
	return make_ext4fs_directory(filename, len, mountpoint, sehnd, NULL);
}

int make_ext4fs_fd(int fd, long long len, const char *mountpoint, struct selabel_handle *sehnd)
{
	return make_ext4fs_internal(fd, len, mountpoint, sehnd, NULL, false);
}

int make_ext4fs_sparse_fd_directory(int fd, long long len, const char *mountpoint,
				    struct selabel_handle *sehnd, const char *directory)
{
	return make_ext4fs_internal(fd, len, mountpoint, sehnd, directory, true);
}

int make_ext4fs_sparse_fd(int fd, long long len, const char *mountpoint,
			  struct selabel_handle *sehnd)
{
	return make_ext4fs_sparse_fd_directory(fd, len, mountpoint, sehnd, NULL);
}
