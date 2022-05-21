/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/fanotify.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include<pthread.h>

struct file_handle {
    __u32 handle_bytes;
    int handle_type;
    /* file identifier */
    unsigned char f_handle[];
};

struct flag {
    uint64_t mask;
    const char * name;
};

#define MASK_COUNT 7
static struct flag const MASKS[MASK_COUNT] = {
    {FAN_OPEN, "OPEN"},
    {FAN_OPEN_EXEC, "OPEN_EXEC"},
    {FAN_ACCESS, "ACCESS"},
    {FAN_MODIFY, "MODIFY"},
    {FAN_CLOSE, "CLOSE"},
    {FAN_CLOSE_WRITE, "CLOSE_WRITE"},
    {FAN_CLOSE_NOWRITE, "CLOSE_NOWRITE"},
};

#define NAME_BUF_SIZE 32
#define MASK_LENGTH 32
#define BUFFER_SIZE 4096

char * print_flags(
    char * output, char * output_end,
    struct flag const * flags, unsigned flag_count, uint64_t mask
) {
    unsigned j;
    /* If the mask is empty then we don't print anything. */
    if (!mask) return output;
    for (j = 0; j < flag_count; ++j) {
        if ((flags[j].mask & mask) == flags[j].mask ) {
            /* Print the name of the bits */
            size_t length = strlen(flags[j].name);
            if (output_end - output < length) length = output_end - output;
            memcpy(output, flags[j].name, length);
            output += length;
            if (output != output_end) { *(output++) = '|'; }
            /* Remove the bits from the mask. */
            mask &= ~flags[j].mask;
        }
    }
    if (mask) {
        /* The mask contained some bits we don't know about. Print it as hex */
        output += snprintf(
            output, output_end - output, "0x%llx", (long long) mask
        );
    } else {
        /* We have written a trailing '|' character since the mask is set and
         * we known what all the bits mean. So we can safely move output one
         * character back to remove the trailing '|' */
        --output;
    }
    return output;
}

void getNameByPid(pid_t pid, char *task_name) {
    char proc_pid_path[NAME_BUF_SIZE];
    char buf[NAME_BUF_SIZE];

    sprintf(proc_pid_path, "/proc/%d/status", pid);
    FILE *fp = fopen(proc_pid_path, "r");
    if (NULL != fp) {
        if (fgets(buf, NAME_BUF_SIZE - 1, fp) == NULL) {
            fclose(fp);
        }
        fclose(fp);
        sscanf(buf, "%*s %s", task_name);
    }
}

void *monitor_dirent(void * argv) {
    int fan, count;
    int mount_fd, event_fd;
    char buf[BUFFER_SIZE];
    char OUTPUT_BUFFER[BUFFER_SIZE];
    char fdpath[32];
    char path[PATH_MAX + 1];
    struct file_handle *file_handle;
	char *data_dir = "/data";
    ssize_t buflen, linklen;
    struct fanotify_event_metadata *metadata;
    struct fanotify_event_info_fid *fid;
    char file_name[NAME_BUF_SIZE];
    char commname[NAME_BUF_SIZE];
    char *output;
    mount_fd = open(data_dir, O_DIRECTORY | O_RDONLY);
    if (mount_fd == -1) { perror("mount fd error"); exit(EXIT_FAILURE); }

    fan = syscall(__NR_fanotify_init, FAN_CLASS_NOTIF |
				FAN_UNLIMITED_QUEUE | FAN_UNLIMITED_MARKS |
				FAN_REPORT_DIR_FID | FAN_REPORT_NAME /*| FAN_REPORT_TID*/,
				O_RDWR);
    if (fan == -1) { perror("fanotify_init"); exit(EXIT_FAILURE); }

	int flag = FAN_MARK_ADD | FAN_MARK_FILESYSTEM;
	int mask = FAN_CREATE | FAN_DELETE | FAN_DELETE_SELF | FAN_EVENT_ON_CHILD | FAN_ONDIR;
    int ret = syscall(__NR_fanotify_mark, fan, flag, mask, AT_FDCWD, data_dir);
    if(ret == -1) { perror("fanotify_mark"); exit(EXIT_FAILURE); }

	struct timeval tv;
	struct tm *p;

    while(1) {
        buflen = read(fan, buf, sizeof(buf));
        if (buflen < 0) { perror("read error"); exit(EXIT_FAILURE); }
        metadata = (struct fanotify_event_metadata*)&buf;

        for (; FAN_EVENT_OK(metadata, buflen); metadata = FAN_EVENT_NEXT(metadata, buflen)) {
            fid = (struct fanotify_event_info_fid *) (metadata + 1);
            file_handle = (struct file_handle *) fid->handle;

            output = OUTPUT_BUFFER;

			/* Ensure that the event info is of the correct type */
            if (fid->hdr.info_type != FAN_EVENT_INFO_TYPE_DFID_NAME) {
                fprintf(stderr, "Received unexpected event info type.\n");
                exit(EXIT_FAILURE);
            }

            event_fd = syscall(__NR_open_by_handle_at, mount_fd, file_handle, O_RDONLY);
            if (event_fd == -1) {
                if (errno == ESTALE) {
                    printf("File handle is no longer valid. "
                           "File has been deleted\n");
                    continue;
                } else {
                    perror("open_by_handle_at");
                    exit(EXIT_FAILURE);
                }
            }

            sprintf(fdpath, "/proc/self/fd/%d", event_fd);
            linklen = readlink(fdpath, path, sizeof(path) - 1);
            if (linklen == -1) {
                perror("readlink error");
            }
            path[linklen] = '\0';
            memset(commname, 0, NAME_BUF_SIZE);
            getNameByPid(metadata->pid, commname);
			gettimeofday(&tv, NULL);
			p = localtime(&tv.tv_sec);

            count = snprintf(output, BUFFER_SIZE, "%02d-%02d %02d:%02d:%02d.%03ld  (%6d)%-15s  %-32s %s/%s\n",
							 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec / 1000,
							 metadata->pid, commname,
							 (metadata->mask == FAN_CREATE) ? "CREATE" : "DELETE",
							 path, (char *)file_handle + sizeof(struct file_handle) + file_handle->handle_bytes);

            write(1, output, count);
            /* Close the file descriptor of the event. */
            close(event_fd);
        }
    }
}


void *monitor_file(void * argv) {
	char PROC_SELF_FD[NAME_BUF_SIZE] = "/proc/self/fd/";
    char INPUT_BUFFER[BUFFER_SIZE];
    char OUTPUT_BUFFER[BUFFER_SIZE];
    int fanfd, i, result, cwdfd, length;
    char task_name[NAME_BUF_SIZE];
	struct timeval tv;
	struct tm *p;
	char *temp;
    #define PATH_COUNT 6
    char *directory_path[PATH_COUNT] = {"/system", "/vendor", "/product", "/odm", "/oem", "/data"};

	fanfd = syscall(__NR_fanotify_init, FAN_CLASS_NOTIF | FAN_UNLIMITED_QUEUE | FAN_UNLIMITED_MARKS, O_RDONLY);
	if (fanfd < 0) {
        perror("fanotify_init");
        /* The most likely reason to fail here is that we don't have
         * the CAP_SYS_ADMIN cabability needed by fanotify_init */
        if (errno == EPERM) {
            write(2, OUTPUT_BUFFER, snprintf(
                OUTPUT_BUFFER, sizeof(OUTPUT_BUFFER),
                "fanotify needs to be run as root\n"
            ));
        }
        exit(EXIT_FAILURE);
    }
    /* In theory fanotify_mark should be able to take AT_FDCWD for the dirfd.
     * However it seems to complain if we pass AT_FDCWD to it. So instead we
     * open the current working directory and pass the resulting fd. */
    cwdfd = openat(AT_FDCWD, ".", O_RDONLY | O_DIRECTORY);
    if (cwdfd < 0) { perror("open error"); exit(EXIT_FAILURE); }

    uint64_t mask = FAN_ACCESS | FAN_MODIFY | FAN_OPEN | FAN_CLOSE;
    unsigned int flags = FAN_MARK_ADD | FAN_MARK_FILESYSTEM;
    for (i = 0; i < PATH_COUNT; i++) {
         result =  syscall(__NR_fanotify_mark, fanfd, flags, mask, cwdfd, directory_path[i]);
         if (result < 0) { perror("fanotify_mark"); exit(EXIT_FAILURE); }
    }

    if (result < 0) { perror("fanotify_mark"); exit(EXIT_FAILURE); }
    for (;;) {
        ssize_t count = read(fanfd, INPUT_BUFFER, sizeof(INPUT_BUFFER));
        if (count < 0) { perror("read error"); exit(EXIT_FAILURE); }
        char * input = INPUT_BUFFER;
        char * input_end = input + count;
        struct fanotify_event_metadata * event;
        while (input != input_end) {
			gettimeofday(&tv, NULL);
			p = localtime(&tv.tv_sec);

            char * output = OUTPUT_BUFFER;
            /* Leave space at the end of the output buffer for a '\n' */
            char * output_end = output + sizeof(OUTPUT_BUFFER) - 1;
            /* Check that we have enough input read an event structure. */
            if (input_end - input < sizeof(struct fanotify_event_metadata)) {
                perror("Invalid fanotify_event_meta"); exit(EXIT_FAILURE);
            }
            event = (struct fanotify_event_metadata *) input;
            /* Check that we have all of the event structure and that it's
             * a version that we understand */
            if (input_end - input < event->event_len ||
                event->vers != FANOTIFY_METADATA_VERSION) {
                perror("Invalid fanotify_event_meta"); exit(EXIT_FAILURE);
            }
            //1900+p->tm_year
			output += snprintf(output, output_end - output, "%02d-%02d %02d:%02d:%02d.%03ld  ",
						1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec / 1000);

            output += snprintf(output, output_end - output, "(%6d)", event->pid);
            getNameByPid(event->pid, task_name);
            output += snprintf(output, output_end - output, "%-15s  ", task_name);

			temp = output;
			/* Print the event mask. Each bit will be separated by '|'
             * characters. */
            output = print_flags(output, output_end, MASKS, MASK_COUNT, event->mask);
			length = MASK_LENGTH - (output - temp);
			while(length-- >= 0)
				output += snprintf(output, output_end - output, " ");

            snprintf(PROC_SELF_FD, sizeof(PROC_SELF_FD), "/proc/self/fd/%d", event->fd);
            count = readlink(PROC_SELF_FD, output, output_end - output);
            if (count < 0) { perror("readlink error"); exit(EXIT_FAILURE); }
            output += count;
            /* Add a newline to the end. This is always safe because we left
             * ourselves a byte of space when picking output_end */
            *(output++) = '\n';
            write(1, OUTPUT_BUFFER, output - OUTPUT_BUFFER);

            /* Close the file descriptor of the event. */
            close(event->fd);
            /* Advance to next event in the input buffer */
            input += event->event_len;
        }
    }
}

int main(int argc, char **argv) {
	int ret;
	pthread_t thread_fd;

    setpriority(PRIO_PROCESS, 0, -20);
	ret = pthread_create(&thread_fd, 0, monitor_file, NULL);
	if (ret) { perror("path pthread init failed\n"); exit(EXIT_FAILURE); }
    monitor_dirent(NULL);
    pthread_join(thread_fd, NULL);

	return ret;
}
