/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <pagemap/pagemap.h>

int pm_kernel_create(pm_kernel_t **ker_out) {
    pm_kernel_t *ker;
    int error;

    if (!ker_out)
        return 1;
    
    ker = calloc(1, sizeof(*ker));
    if (!ker)
        return -errno;

    ker->kpagecount_fd = open("/proc/kpagecount", O_RDONLY);
    if (ker->kpagecount_fd < 0) {
        error = -errno;
        free(ker);
        return error;
    }

    ker->kpageflags_fd = open("/proc/kpageflags", O_RDONLY);
    if (ker->kpageflags_fd < 0) {
        error = -errno;
        close(ker->kpagecount_fd);
        free(ker);
        return error;
    }

    /* This must be explicitly initialized through pm_kernel_init_page_idle() */
    ker->pageidle_fd = -1;

    ker->pagesize = getpagesize();

    *ker_out = ker;

    return 0;
}

int pm_kernel_init_page_idle(pm_kernel_t *ker) {
	if (!ker || ker->pageidle_fd != -1) return -EINVAL;

	ker->pageidle_fd =
	    open("/sys/kernel/mm/page_idle/bitmap", O_RDWR | O_CLOEXEC);
	if (ker->pageidle_fd < 0) return -errno;

	return 0;
}

#define INIT_PIDS 20
int pm_kernel_pids(pm_kernel_t *ker, pid_t **pids_out, size_t *len) {
    DIR *proc;
    struct dirent *dir;
    pid_t pid, *pids, *new_pids;
    size_t pids_count, pids_size;
    int error;

    proc = opendir("/proc");
    if (!proc)
        return -errno;

    pids = malloc(INIT_PIDS * sizeof(pid_t));
    if (!pids) {
        closedir(proc);
        return -errno;
    }
    pids_count = 0; pids_size = INIT_PIDS;

    while ((dir = readdir(proc))) {
        if (sscanf(dir->d_name, "%d", &pid) < 1)
            continue;

        if (pids_count >= pids_size) {
            new_pids = realloc(pids, 2 * pids_size * sizeof(pid_t));
            if (!new_pids) {
                error = -errno;
                free(pids);
                closedir(proc);
                return error;
            }
            pids = new_pids;
            pids_size = 2 * pids_size;
        }

        pids[pids_count] = pid;

        pids_count++;
    }

    closedir(proc);
    
    new_pids = realloc(pids, pids_count * sizeof(pid_t));
    if (!new_pids) {
        error = -errno;
        free(pids);
        return error;
    }

    *pids_out = new_pids;
    *len = pids_count;

    return 0;
}

int pm_kernel_count(pm_kernel_t *ker, uint64_t pfn, uint64_t *count_out) {
    off64_t off;

    if (!ker || !count_out)
        return -1;

    off = lseek64(ker->kpagecount_fd, pfn * sizeof(uint64_t), SEEK_SET);
    if (off == (off_t)-1)
        return -errno;
    if (read(ker->kpagecount_fd, count_out, sizeof(uint64_t)) <
        (ssize_t)sizeof(uint64_t))
        return -errno;

    return 0;
}

int pm_kernel_flags(pm_kernel_t *ker, uint64_t pfn, uint64_t *flags_out) {
    off64_t off;

    if (!ker || !flags_out) return -EINVAL;

    off = lseek64(ker->kpageflags_fd, pfn * sizeof(uint64_t), SEEK_SET);
    if (off == (off_t)-1)
        return -errno;
    if (read(ker->kpageflags_fd, flags_out, sizeof(uint64_t)) <
        (ssize_t)sizeof(uint64_t))
        return -errno;

    return 0;
}

int pm_kernel_has_page_idle(pm_kernel_t *ker) {
    /* Treat error to be fallback to clear_refs */
    if (!ker)
        return 0;

    return !(ker->pageidle_fd < 0);
}

int pm_kernel_get_page_idle(pm_kernel_t *ker, uint64_t pfn) {
    uint64_t bits;
    off_t offset;

    if (!ker) return -EINVAL;

    if (ker->pageidle_fd < 0) return -ENXIO;

    offset = pfn_to_page_idle_offset(pfn);
    if (pread(ker->pageidle_fd, &bits, sizeof(uint64_t), offset) < 0)
        return -errno;

    bits >>= (pfn % 64);

    return bits & 1;
}


int pm_kernel_mark_page_idle(pm_kernel_t *ker, uint64_t *pfn, int n) {
    uint64_t bits;

    if (!ker) return -EINVAL;

    if (ker->pageidle_fd < 0) return -ENXIO;

    for (int i = 0; i < n; i++) {
        off_t offset = pfn_to_page_idle_offset(pfn[i]);
        if (pread(ker->pageidle_fd, &bits, sizeof(uint64_t), offset) < 0)
		return -errno;

	bits |= 1ULL << (pfn[i] % 64);

	if (pwrite(ker->pageidle_fd, &bits, sizeof(uint64_t), offset) < 0)
		return -errno;
    }

    return 0;
}

int pm_kernel_page_is_accessed(pm_kernel_t *ker, uint64_t pfn, uint64_t *flags) {
    int error;
    uint64_t page_flags;

    if (!ker) return -EINVAL;

    if (pm_kernel_has_page_idle(ker)) {
        return pm_kernel_get_page_idle(ker, pfn);
    }

    if (!flags) {
        flags = &page_flags;
        error = pm_kernel_flags(ker, pfn, flags);
        if (error)
            return error;
    }

    return !!(*flags & (1 << KPF_REFERENCED));
}

int pm_kernel_destroy(pm_kernel_t *ker) {
    if (!ker)
        return -1;

    close(ker->kpagecount_fd);
    close(ker->kpageflags_fd);
    if (ker->pageidle_fd >= 0)
        close(ker->pageidle_fd);

    free(ker);

    return 0;
}
