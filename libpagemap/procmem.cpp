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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <inttypes.h>

#include <pagemap/pagemap.h>

/* Information about a single mapping */
struct map_info {
    pm_map_t *map;
    pm_memusage_t usage;
    /* page counts */
    unsigned long shared_clean;
    unsigned long shared_dirty;
    unsigned long private_clean;
    unsigned long private_dirty;
};

/* display the help screen */
static void usage(const char *cmd);

/* qsort compare function to compare maps by PSS */
int comp_pss(const void *a, const void *b);

/* qsort compare function to compare maps by USS */
int comp_uss(const void *a, const void *b);

/* qsort compare function to compare maps by their start
 * addresses
 */
static int sort_by_vma_start(const void *a, const void *b) {
    struct map_info *ma, *mb;
    ma = *((struct map_info **)a);
    mb = *((struct map_info **)b);

    if (mb->map->start < ma->map->start) return -1;
    if (mb->map->start > ma->map->start) return 1;
    return 0;

}

/* map info is stored as follows:
 * uint64_t pid
 * uint64_t num_maps
 *  pm_map_t map[0]
 *  struct map_info mi[0]
 *  ....
 *
 * byteorder is native
 */
static int load_maps_from_file(pid_t pid, const char *file,
                               struct map_info ***mi_out, size_t *len) {

    struct map_info **mi, *m;
    pm_map_t *map;
    size_t num_maps, i;
    int fd;
    int error;
    pid_t file_pid;
    char *f, *ptr;
    struct stat st;

    fd = open(file, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file %s - %d\n", file, errno);
        return -errno;
    }

    error = fstat(fd, &st);
    if (error < 0) {
        fprintf(stderr, "Failed to get file stat %s - %d\n", file, errno);
        error = -errno;
        goto out;
    }

    f = ptr = (char *)mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "Failed to map file %s - %d\n", file, errno);
        error = -errno;
        goto out;
    }

    file_pid = *(pid_t *)ptr;
    if (file_pid != pid) {
        fprintf(stderr, "Pid mismatch for file %s : %d != %d\n", file, pid, file_pid);
        error = -EINVAL;
        goto out_unmap;
    }
    ptr += sizeof(pid_t);

    /* read maps entries */
    num_maps = *(size_t *)ptr;
    ptr += sizeof(num_maps);


    mi = (struct map_info **)calloc(num_maps, sizeof(struct map_info *));
    if (!mi) {
        fprintf(stderr, "Failed to allocate map_info array\n");
        error = -ENOMEM;
        goto out_unmap;
    }

    map = (pm_map_t *)calloc(num_maps, sizeof(pm_map_t) + sizeof(struct map_info));
    if (!map) {
        fprintf(stderr, "failed to map + map_info\n");
        error = -ENOMEM;
        goto out_free_mi;
    }
    m = (struct map_info *) (map + num_maps);

   /* start reading one map at a time */
    for (i = 0; i < num_maps; i++, map++, m++) {
        *map = *(pm_map_t *)ptr;
        ptr += sizeof(*map);

        map->name = strdup((char *)ptr);
        ptr += strlen(map->name) + 1;

        map->proc = NULL;

        *m = *(struct map_info *)ptr;
        ptr += sizeof(struct map_info);

        m->map = map;
        mi[i] = m;
    }

    *mi_out = mi;
    *len = num_maps;

    return 0;

out_free_mi:
    free(mi);
out_unmap:
    munmap(f, st.st_size);
out:
    close(fd);
    return error;

}

static int store_maps_to_file(pid_t pid, const char *file,
                              struct map_info **mis, size_t len) {
    size_t filesz;
    int fd;
    size_t i;
    int error;
    pm_map_t map;
    char *map_name;

    /* first of all, sort the map in the ascending order of start addresses */
    qsort(mis, len, sizeof(mis[0]), sort_by_vma_start);

    fd = creat(file, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "failed to open file %s\n.", file);
        return -errno;
    }

    error = TEMP_FAILURE_RETRY(write(fd, &pid, sizeof(pid)));
    if (error != sizeof(pid)) {
        fprintf(stderr, "failed to write pid to file %s:%d:%d\n", file, error, errno);
        error = -errno;
        goto out;
    }
    filesz = sizeof(pid);

    error = TEMP_FAILURE_RETRY(write(fd, &len, sizeof(len)));
    if (error != sizeof(len)) {
        fprintf(stderr, "failed to write num_maps to file %s:%d:%d\n", file, error, errno);
        error = -errno;
        goto out;
    }
    filesz += error;

    for (i = 0; i < len; i++) {
        size_t name_len;
        struct map_info *mi = mis[i];
        map = *mi->map;
        map_name = strdup(map.name);
        name_len = strlen(map_name) + 1;

        error = TEMP_FAILURE_RETRY(write(fd, &map, sizeof(pm_map_t)));
        if (error != sizeof(pm_map_t)) {
            fprintf(stderr, "failed to write pm_map_t to file %s:%d:%d\n", file, error, errno);
            error = -errno;
            goto out;
        }
        filesz += error;

        /* now write the name of the map, followed by the map_info */
        error = TEMP_FAILURE_RETRY(write(fd, map_name, name_len));
        if (error != name_len) {
            fprintf(stderr, "failed to write map_name to file %s:%d:%d\n", file, error, errno);
            error = -errno;
            goto out;
        }
        filesz += error;
        free(map_name);

        error = TEMP_FAILURE_RETRY(write(fd, mi, sizeof(*mi)));
        if (error != sizeof(*mi)) {
            fprintf(stderr, "failed to write map_info to file %s:%d:%d\n", file, error, errno);
            error = -errno;
            goto out;
        }
        filesz += error;
    }

    error = 0;
out:
    close(fd);
    return error;
}

static int same_vma(struct map_info *a, struct map_info *b) {
    pm_map_t *ma = a->map;
    pm_map_t *mb = b->map;

    return (ma->start == mb->start && ma->end == mb->end &&
            ma->offset == mb->offset && ma->flags == mb->flags &&
            !strcmp(ma->name, mb->name));
}

static int subset_vma(struct map_info *a, struct map_info *b) {
    pm_map_t *ma = a->map;
    pm_map_t *mb = b->map;

    if (!same_vma(a, b))
       return mb->start <= ma->start && mb->end >= ma->end;

    return 1;
}

static void diff_usage(pm_memusage_t *res, pm_memusage_t *a, pm_memusage_t *b) {
    res->vss = b->vss > a->vss ? b->vss - a->vss : 0;
    res->rss = b->rss > a->rss ? b->rss - a->rss : 0;
    res->pss = b->pss > a->pss ? b->pss - a->pss : 0;
    res->uss = b->uss > a->uss ? b->uss - a->uss : 0;
    res->swap = b->swap > a->swap ? b->swap - a->swap : 0;
}

void diff_vma(struct map_info *res, struct map_info *a, struct map_info *b) {
    res->shared_clean = b->shared_clean > a->shared_clean ? b->shared_clean - a->shared_clean : 0;
    res->shared_dirty = b->shared_dirty > a->shared_dirty ? b->shared_dirty - a->shared_dirty : 0;
    res->private_clean = b->private_clean > a->private_clean ? b->private_clean - a->private_clean : 0;
    res->private_dirty = b->private_dirty > a->private_dirty ? b->private_dirty - a->private_dirty : 0;

    diff_usage(&res->usage, &a->usage, &b->usage);
}

static int compare_maps(pid_t pid, struct map_info **mia, size_t len_a,
                        struct map_info **mib, size_t len_b,
                        struct map_info ***result, size_t *result_len) {

    struct map_info **mi;
    struct map_info *m;
    pm_map_t *maps;
    size_t len = 0;
    size_t i, j;
    int error;

    mi = (struct map_info **)calloc(len_a + len_b, sizeof(struct map_info *));
    if (!mi) {
        fprintf(stderr, "failed to allocate map_info array\n");
        return -ENOMEM;
    }

    m = (struct map_info *)calloc(len_a + len_b, sizeof(*m) + sizeof(pm_map_t));
    if (!m) {
        fprintf(stderr, "Failed to allocate mapinfo\n");
        error = -ENOMEM;
        goto out_free_mi;
    }
    maps = (pm_map_t *) (m + len_a + len_b);


    /* Make sure to sort both ma and mb by the vma start address */
    qsort(mia, len_a, sizeof(mia[0]), sort_by_vma_start);
    qsort(mib, len_b, sizeof(mib[0]), sort_by_vma_start);

    for (i = 0; i < len_a; i++) {
        for (j = 0; j < len_b; j++)
        if (same_vma(mia[i], mib[j]) || subset_vma(mia[i], mib[j])) {
            struct map_info *t = m + len;
            t->map = maps + len;
            diff_vma(t, mia[i], mib[j]);
            *t->map = *mib[j]->map;
            t->map->name = strdup(mib[j]->map->name);
            /* mark vma as processed */
            mib[j]->map->proc = (pm_process_t *)0xa0b0c0d0;
            mi[len] = t;
            len++;
        }
    }

    /* second pass to go through the b array to pick up any VMAs that were
     * left over */
    for (j = 0; j < len_b; j++) {
        if (mib[j]->map->proc != (pm_process_t *)0xa0b0c0d0) {
            struct map_info *t = m + len;
            *t = *mib[j];
            t->map = maps + len;
            *t->map = *mib[j]->map;
            t->map->name = strdup(mib[j]->map->name);
            mi[len] = t;
            len++;
        }
    }

    qsort(mi, len, sizeof(mi[0]), sort_by_vma_start);

    *result = mi;
    *result_len = len;
    return 0;

out_free_mi:
    return error;
}

static char *get_file_from_args(size_t i, char *argv[], int check_access) {

    char *file = strdup(argv[i]);

    if (check_access) {
        if (access(file, R_OK) < 0) {
          fprintf(stderr, "Access error for file: %s - (%s: %d)\n", file,
                  strerror(errno), errno);
          free(file);
          return NULL;
        }
    }

    return file;
}

int main(int argc, char *argv[]) {
    pid_t pid;

    /* libpagemap context */
    pm_kernel_t *ker;
    int pagesize; /* cached for speed */
    pm_process_t *proc;

    /* maps and such */
    pm_map_t **maps; size_t num_maps;

    struct map_info **mis = NULL;
    struct map_info *mi;

    /* pagemap information */
    uint64_t *pagemap; size_t num_pages;
    uint64_t mapentry;
    uint64_t count, flags;

    /* totals */
    unsigned long total_shared_clean, total_shared_dirty, total_private_clean, total_private_dirty;
    pm_memusage_t total_usage;

    /* command-line options */
    int ws;
#define WS_OFF (0)
#define WS_ONLY (1)
#define WS_RESET (2)
    int (*compfn)(const void *a, const void *b);
    int hide_zeros;

    /* Use kernel's idle page trackign interface for working set determination */
    int use_pageidle;

    char *infile = NULL, *outfile = NULL;

    /* temporary variables */
    size_t i, j;
    char *endptr;
    int error;

    if (argc < 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    ws = WS_OFF;
    compfn = NULL;
    hide_zeros = 0;
    use_pageidle = 0;
    for (i = 1; i < (size_t)(argc - 1); i++) {
        if (!strcmp(argv[i], "-w")) { ws = WS_ONLY; continue; }
        if (!strcmp(argv[i], "-W")) { ws = WS_RESET; continue; }
        if (!strcmp(argv[i], "-i")) { use_pageidle = 1; continue; }
        if (!strcmp(argv[i], "-m")) { compfn = NULL; continue; }
        if (!strcmp(argv[i], "-p")) { compfn = &comp_pss; continue; }
        if (!strcmp(argv[i], "-u")) { compfn = &comp_uss; continue; }
        if (!strcmp(argv[i], "-h")) { hide_zeros = 1; continue; }
        if (!strcmp(argv[i], "-c")) {
            if (i < (size_t)argc - 1) {
                infile = get_file_from_args(++i, argv, 1);
                if (!infile) {
                    fprintf(stderr, "Invalid of absent output file\n");
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
            }
            continue;
        }
        if (!strcmp(argv[i], "-o")) {
            if (i < (size_t)argc - 1) {
                outfile = get_file_from_args(++i, argv, 0);
                if (!outfile) {
                    fprintf(stderr, "Invalid of absent output file\n");
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
            }
            continue;
        }
        fprintf(stderr, "Invalid argument \"%s\".\n", argv[i]);
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    pid = (pid_t)strtol(argv[argc - 1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid PID \"%s\".\n", argv[argc - 1]);
        exit(EXIT_FAILURE);
    }

    error = pm_kernel_create(&ker);
    if (error) {
        fprintf(stderr, "error creating kernel interface -- "
                        "does this kernel have pagemap?\n");
        exit(EXIT_FAILURE);
    }

    if (ws != WS_OFF && use_pageidle) {
        error = pm_kernel_init_page_idle(ker);
        if (error) {
            fprintf(stderr, "error initalizing idle page tracking -- "
                            "enable CONFIG_IDLE_PAGE_TRACKING in kernel.\n");
            exit(EXIT_FAILURE);
        }
    }

    pagesize = pm_kernel_pagesize(ker);

    error = pm_process_create(ker, pid, &proc);
    if (error) {
        fprintf(stderr, "error creating process interface -- "
                        "does process %d really exist?\n", pid);
        exit(EXIT_FAILURE);
    }

    if (ws != WS_OFF) {
        /*
         * The idle page tracking interface will update the PageIdle flags
         * upon writing. So, even if we are called only to read the *current*
         * working set, we need to reset the bitmap to make sure we get
         * the updated page idle flags. This is not true with the 'clear_refs'
         * implementation.
         */
        if (ws == WS_RESET || use_pageidle) {
            error = pm_process_workingset(proc, NULL, 1);
            if (error) {
                fprintf(stderr, "error resetting working set for process.\n");
                exit(EXIT_FAILURE);
            }
        }
        if (ws == WS_RESET)
            exit(EXIT_SUCCESS);
    }

    /* get maps, and allocate our map_info array */
    error = pm_process_maps(proc, &maps, &num_maps);
    if (error) {
        fprintf(stderr, "error listing maps.\n");
        exit(EXIT_FAILURE);
    }

    mis = (struct map_info **)calloc(num_maps, sizeof(struct map_info *));
    if (!mis) {
        fprintf(stderr, "error allocating map_info array: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* print header */
    if (ws == WS_ONLY) {
        printf("%7s  %7s  %7s  %7s  %7s  %7s  %7s  %50s  %20s  %6s  %6s\n",
               "WRss", "WPss", "WUss", "WShCl", "WShDi", "WPrCl", "WPrDi", "Name", "Range", "Perms", "Offset");
        printf("%7s  %7s  %7s  %7s  %7s  %7s  %7s  %50s  %16s  %5s  %7s\n",
               "-------", "-------", "-------", "-------", "-------", "-------", "-------",
               "-----------------------------------------------", "--------------------", "----", "---------");
    } else {
        printf("%7s  %7s  %7s  %7s  %7s  %7s  %7s  %7s  %s\n",
               "Vss", "Rss", "Pss", "Uss", "ShCl", "ShDi", "PrCl", "PrDi", "Name");
        printf("%7s  %7s  %7s  %7s  %7s  %7s  %7s  %7s  %s\n",
               "-------", "-------", "-------", "-------", "-------", "-------", "-------", "-------", "");
    }

    /* zero things */
    pm_memusage_zero(&total_usage);
    total_shared_clean = total_shared_dirty = total_private_clean = total_private_dirty = 0;

    for (i = 0; i < num_maps; i++) {
        mi = (struct map_info *)calloc(1, sizeof(struct map_info));
        if (!mi) {
            fprintf(stderr, "error allocating map_info: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        mi->map = maps[i];

        /* get, and sum, memory usage */

        if (ws == WS_ONLY)
            error = pm_map_workingset(mi->map, &mi->usage);
        else
            error = pm_map_usage(mi->map, &mi->usage);
        if (error) {
            fflush(stdout);
            fprintf(stderr, "error getting usage for map.\n");
            continue;
        }

        /* get, and sum, individual page counts */

        error = pm_map_pagemap(mi->map, &pagemap, &num_pages);
        if (error) {
            fflush(stdout);
            fprintf(stderr, "error getting pagemap for map.\n");
            continue;
        }

        mi->shared_clean = mi->shared_dirty = mi->private_clean = mi->private_dirty = 0;

        for (j = 0; j < num_pages; j++) {
            mapentry = pagemap[j];

            if (PM_PAGEMAP_PRESENT(mapentry) && !PM_PAGEMAP_SWAPPED(mapentry)) {

                error = pm_kernel_count(ker, PM_PAGEMAP_PFN(mapentry), &count);
                if (error) {
                    fflush(stdout);
                    fprintf(stderr, "error getting count for frame.\n");
                }

                error = pm_kernel_flags(ker, PM_PAGEMAP_PFN(mapentry), &flags);
                if (error) {
                    fflush(stdout);
                    fprintf(stderr, "error getting flags for frame.\n");
                }

                if ((ws != WS_ONLY) ||
                    pm_kernel_page_is_accessed(ker, PM_PAGEMAP_PFN(mapentry),
                                               &flags)) {
                  if (count > 1) {
                    if (flags & (1 << KPF_DIRTY)) {
                      mi->shared_dirty++;
                    } else {
                      mi->shared_clean++;
                    }
                  } else {
                    if (flags & (1 << KPF_DIRTY)) {
                      mi->private_dirty++;
                    } else {
                      mi->private_clean++;
                    }
                  }
                }
            }
        }

        /* add to array */
        mis[i] = mi;
    }

    if (ws == WS_ONLY) {
        if (infile) {
            struct map_info **mi_a;
            struct map_info **result;
            size_t num_maps_a;
            size_t num_result;
            error = load_maps_from_file(pid, infile, &mi_a, &num_maps_a);
            if (error < 0) {
                fprintf(stderr, "Failed to load maps for process %d from %s\n", pid, infile);
                exit(EXIT_FAILURE);
            }

            error = compare_maps(pid, mi_a, num_maps_a, mis, num_maps, &result, &num_result);
            if (error < 0) {
                fprintf(stderr, "failed to compare maps for calculating working set of a use case\n");
                exit(EXIT_FAILURE);
            }
            /* Print the delta between what was loaded and what we read. This will represent
             * the working set of the process for a use case (assuming the usecase was what was triggered
             * from when the infile was stored to when this was run.)
             */
            free(mis);
            mis = result;
            num_maps = num_result;
        } else if (outfile) {
            error = store_maps_to_file(pid, outfile, mis, num_maps);
            if (error < 0) {
                fprintf(stderr, "Failed to store maps for process %d to file %s\n", pid, outfile);
                exit(EXIT_FAILURE);
            }
        }
    }

    /* sort the array, if requested (compfn == NULL for original order) */
    if (compfn)
        qsort(mis, num_maps, sizeof(mis[0]), compfn);

    for (i = 0; i < num_maps; i++) {
        mi = mis[i];

        if ((!mi) || (hide_zeros && !mi->usage.rss))
            continue;

        if (ws == WS_ONLY) {
            printf("%6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %50s %" PRIx64 "-%" PRIx64 "   0x%01x    %08x\n",
                (long)mi->usage.rss / 1024,
                (long)mi->usage.pss / 1024,
                (long)mi->usage.uss / 1024,
                mi->shared_clean * pagesize / 1024,
                mi->shared_dirty * pagesize / 1024,
                mi->private_clean * pagesize / 1024,
                mi->private_dirty * pagesize / 1024,
                pm_map_name(mi->map),
                mi->map->start,
                mi->map->end,
                mi->map->flags & PM_MAP_PERMISSIONS,
                (unsigned int)mi->map->offset
            );
        } else {
            printf("%6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %s\n",
                (long)mi->usage.vss / 1024,
                (long)mi->usage.rss / 1024,
                (long)mi->usage.pss / 1024,
                (long)mi->usage.uss / 1024,
                mi->shared_clean * pagesize / 1024,
                mi->shared_dirty * pagesize / 1024,
                mi->private_clean * pagesize / 1024,
                mi->private_dirty * pagesize / 1024,
                pm_map_name(mi->map)
            );
        }

        pm_memusage_add(&total_usage, &mi->usage);
        total_shared_clean += mi->shared_clean;
        total_shared_dirty += mi->shared_dirty;
        total_private_clean += mi->private_clean;
        total_private_dirty += mi->private_dirty;
    }

    /* print totals */
    if (ws == WS_ONLY) {
        printf("%7s  %7s  %7s  %7s  %7s  %7s  %7s  %50s\n",
               "-------", "-------", "-------", "-------", "-------", "-------", "-------", "");
        printf("%6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %50s\n",
            (long)total_usage.rss / 1024,
            (long)total_usage.pss / 1024,
            (long)total_usage.uss / 1024,
            total_shared_clean * pagesize / 1024,
            total_shared_dirty * pagesize / 1024,
            total_private_clean * pagesize / 1024,
            total_private_dirty * pagesize / 1024,
            "TOTAL"
        );
    } else {
        printf("%7s  %7s  %7s  %7s  %7s  %7s  %7s  %7s  %s\n",
               "-------", "-------", "-------", "-------", "-------", "-------", "-------", "-------", "");
        printf("%6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %6ldK  %s\n",
            (long)total_usage.vss / 1024,
            (long)total_usage.rss / 1024,
            (long)total_usage.pss / 1024,
            (long)total_usage.uss / 1024,
            total_shared_clean * pagesize / 1024,
            total_shared_dirty * pagesize / 1024,
            total_private_clean * pagesize / 1024,
            total_private_dirty * pagesize / 1024,
            "TOTAL"
        );
    }

    free(mis);
    return 0;
}

static void usage(const char *cmd) {
    fprintf(stderr, "Usage: %s [-i] [ -w | -W ] [ -p | -m ] [ -h ] pid\n"
                    "    -i  Uses idle page tracking for working set statistics.\n"
                    "    -w  Displays statistics for the working set only.\n"
                    "    -W  Resets the working set of the process.\n"
                    "    -p  Sort by PSS.\n"
                    "    -u  Sort by USS.\n"
                    "    -m  Sort by mapping order (as read from /proc).\n"
                    "    -c  <file> Input file to load last mapinfo for this process from <file>.\n"
                    "    -o  <file> Dump current mapinfo of the process in <file>.\n"
                    "    -h  Hide maps with no RSS.\n",
        cmd);
}

int comp_pss(const void *a, const void *b) {
    struct map_info *ma, *mb;

    ma = *((struct map_info **)a);
    mb = *((struct map_info **)b);

    if (mb->usage.pss < ma->usage.pss) return -1;
    if (mb->usage.pss > ma->usage.pss) return 1;
    return 0;
}

int comp_uss(const void *a, const void *b) {
    struct map_info *ma, *mb;

    ma = *((struct map_info **)a);
    mb = *((struct map_info **)b);

    if (mb->usage.uss < ma->usage.uss) return -1;
    if (mb->usage.uss > ma->usage.uss) return 1;
    return 0;
}
