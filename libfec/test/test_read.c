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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <fec/io.h>

#define BUFSIZE (2 * 1024 * FEC_BLOCKSIZE)

int main(int argc, char **argv)
{
    int fd;
    ssize_t count;
    struct fec_handle *f;
    uint8_t *buffer;

    buffer = (uint8_t *)malloc(BUFSIZE);

    if (!buffer) {
        perror("malloc");
        exit(1);
    }

    if (argc != 3) {
        fprintf(stderr, "usage: test_read input output\n");
        exit(1);
    }

    if (fec_open(&f, argv[1], O_RDONLY, FEC_FS_EXT4, FEC_DEFAULT_ROOTS) == -1) {
        perror("fec_open");
        exit(1);
    }

    fd = TEMP_FAILURE_RETRY(open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0640));

    if (fd == -1) {
        perror("open");
        exit(1);
    }

    do {
        count = fec_read(f, buffer, BUFSIZE);

        if (count == -1) {
            perror("fec_read");
            exit(1);
        } else if (count > 0) {
            if (TEMP_FAILURE_RETRY(write(fd, buffer, count)) != count) {
                perror("write");
                exit(1);
            }
        }
    } while (count > 0);

    fec_close(f);
    close(fd);

    free(buffer);

    exit(0);
}
