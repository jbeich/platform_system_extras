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

#define INIT_PIDS 20

int pm_get_pids(pid_t** pids_out, size_t* len) {
    DIR* proc;
    struct dirent* dir;
    pid_t pid, *pids, *new_pids;
    size_t pids_count, pids_size;
    int error;

    proc = opendir("/proc");
    if (!proc) return -errno;

    pids = malloc(INIT_PIDS * sizeof(pid_t));
    if (!pids) {
        closedir(proc);
        return -ENOMEM;
    }
    pids_count = 0;
    pids_size = INIT_PIDS;

    while ((dir = readdir(proc))) {
        if (sscanf(dir->d_name, "%d", &pid) < 1) continue;

        if (pids_count >= pids_size) {
            new_pids = realloc(pids, 2 * pids_size * sizeof(pid_t));
            if (!new_pids) {
                error = -ENOMEM;
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
        error = -ENOMEM;
        free(pids);
        return error;
    }

    *pids_out = new_pids;
    *len = pids_count;

    return 0;
}
