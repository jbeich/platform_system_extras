/*
 * Testsuite for eBPF maps
 *
 * Copyright (c) 2014 PLUMgrid, http://plumgrid.com
 * Copyright (c) 2016 Facebook
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/wait.h>
#include <sys/resource.h>

#include <linux/bpf.h>

#include "cutils/libbpf.h"

static int map_flags;
#define MAP_SIZE 10

static void test_hashmap(int task, void *data)
{
	long long key, next_key, first_key, value;
	int fd;

	fd = bpf_create_map(BPF_MAP_TYPE_HASH, sizeof(key), sizeof(value),
			    2, map_flags);
	if (fd < 0) {
		printf("Failed to create hashmap '%s'!\n", strerror(errno));
		exit(1);
	}

	key = 1;
	value = 1234;
	/* Insert key=1 element. */
	assert(bpf_update_elem(fd, &key, &value, BPF_ANY) == 0);

	value = 0;
	/* BPF_NOEXIST means add new element if it doesn't exist. */
	assert(bpf_map_update_elem(fd, &key, &value, BPF_NOEXIST) == -1 &&
	       /* key=1 already exists. */
	       errno == EEXIST);

	/* -1 is an invalid flag. */
	assert(bpf_map_update_elem(fd, &key, &value, -1) == -1 &&
	       errno == EINVAL);

	/* Check that key=1 can be found. */
	assert(bpf_map_lookup_elem(fd, &key, &value) == 0 && value == 1234);

	key = 2;
	/* Check that key=2 is not found. */
	assert(bpf_map_lookup_elem(fd, &key, &value) == -1 && errno == ENOENT);

	/* BPF_EXIST means update existing element. */
	assert(bpf_map_update_elem(fd, &key, &value, BPF_EXIST) == -1 &&
	       /* key=2 is not there. */
	       errno == ENOENT);

	/* Insert key=2 element. */
	assert(bpf_map_update_elem(fd, &key, &value, BPF_NOEXIST) == 0);

	/* key=1 and key=2 were inserted, check that key=0 cannot be
	 * inserted due to max_entries limit.
	 */
	key = 0;
	assert(bpf_map_update_elem(fd, &key, &value, BPF_NOEXIST) == -1 &&
	       errno == E2BIG);

	/* Update existing element, though the map is full. */
	key = 1;
	assert(bpf_map_update_elem(fd, &key, &value, BPF_EXIST) == 0);
	key = 2;
	assert(bpf_map_update_elem(fd, &key, &value, BPF_ANY) == 0);
	key = 3;
	assert(bpf_map_update_elem(fd, &key, &value, BPF_NOEXIST) == -1 &&
	       errno == E2BIG);

	/* Check that key = 0 doesn't exist. */
	key = 0;
	assert(bpf_map_delete_elem(fd, &key) == -1 && errno == ENOENT);

	/* Iterate over two elements. */
	assert(bpf_map_get_next_key(fd, NULL, &first_key) == 0 &&
	       (first_key == 1 || first_key == 2));
	assert(bpf_map_get_next_key(fd, &key, &next_key) == 0 &&
	       (next_key == first_key));
	assert(bpf_map_get_next_key(fd, &next_key, &next_key) == 0 &&
	       (next_key == 1 || next_key == 2) &&
	       (next_key != first_key));
	assert(bpf_map_get_next_key(fd, &next_key, &next_key) == -1 &&
	       errno == ENOENT);

	/* Delete both elements. */
	key = 1;
	assert(bpf_map_delete_elem(fd, &key) == 0);
	key = 2;
	assert(bpf_map_delete_elem(fd, &key) == 0);
	assert(bpf_map_delete_elem(fd, &key) == -1 && errno == ENOENT);

	key = 0;
	/* Check that map is empty. */
	assert(bpf_map_get_next_key(fd, NULL, &next_key) == -1 &&
	       errno == ENOENT);
	assert(bpf_map_get_next_key(fd, &key, &next_key) == -1 &&
	       errno == ENOENT);

	close(fd);
}

static void test_map_rdonly(void)
{
	int i, fd, key = 0, value = 0;
        int ret;

	fd = bpf_create_map(BPF_MAP_TYPE_HASH, sizeof(key), sizeof(value),
			    MAP_SIZE, map_flags | BPF_F_MAP_RDONLY);
	if (fd < 0) {
		printf("Failed to create map for read only test '%s'!\n",
		       strerror(errno));
		exit(1);
	}

	key = 1;
	value = 1234;
	/* Insert key=1 element. */
	if (bpf_update_elem(fd, &key, &value, BPF_ANY) == -1 &&
	       errno == EPERM)
            printf("update element is blocked\n");

	/* Check that key=2 is not found. */
	if (bpf_lookup_elem(fd, &key, &value) == -1 && errno == ENOENT)
            printf("cannot find any elem\n");
	if (bpf_get_next_key(fd, &key, &value) == -1 && errno == ENOENT)
            printf("cannot find any elem\n");
}

static void test_map_wronly(void)
{
	int i, fd, key = 0, value = 0;

	fd = bpf_create_map(BPF_MAP_TYPE_HASH, sizeof(key), sizeof(value),
			    MAP_SIZE, map_flags | BPF_F_MAP_WRONLY);
	if (fd < 0) {
		printf("Failed to create map for read only test '%s'!\n",
		       strerror(errno));
		exit(1);
	}

	key = 1;
	value = 1234;
	/* Insert key=1 element. */
	if (bpf_update_elem(fd, &key, &value, BPF_ANY) == 0)
            printf("update elem successful!\n");

	/* Check that key=2 is not found. */
	if (bpf_lookup_elem(fd, &key, &value) == -1 && errno == EPERM)
            printf("look up elem denied\n");
	if (bpf_get_next_key(fd, &key, &value) == -1 && errno == EPERM)
            printf("look up elem denied\n");
}

static void run_all_tests(void)
{
	test_hashmap(0, NULL);

	test_map_rdonly();
	test_map_wronly();
}

int main(void)
{
	struct rlimit rinf = { RLIM_INFINITY, RLIM_INFINITY };

	setrlimit(RLIMIT_MEMLOCK, &rinf);

	map_flags = 0;
	run_all_tests();

	map_flags = BPF_F_NO_PREALLOC;
	run_all_tests();

	printf("test_maps: OK\n");
	return 0;
}
