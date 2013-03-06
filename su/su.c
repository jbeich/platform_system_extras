/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#define LOG_TAG "su"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include <unistd.h>
#include <time.h>

#include <pwd.h>

#include <private/android_filesystem_config.h>

#define NR_SUPP_GIDS 32

gid_t parse_gid(char *s)
{
    struct passwd *pw = getpwnam(s);

    return pw? pw->pw_gid: (gid_t)atoi(s);
}

uid_t parse_uid(char *s)
{
    struct passwd *pw = getpwnam(s);

    return pw? pw->pw_uid: (gid_t)atoi(s);
}

/*
 * SU can be given a specific command to exec. UID _must_ be
 * specified for this (ie argc => 3).
 *
 * Usage:
 * su [user].<primary group>[,<supp group 1>,...<supp group n>] [command] [args]
 */
int main(int argc, char **argv)
{
    uid_t uid = 0;
    gid_t gid = 0;
    uid_t myuid = 0;

    gid_t supp_gids[NR_SUPP_GIDS];
    char *user = NULL;
    char *group = NULL;
    int num_gids = 0;
    int i=0;

    /* Until we have something better, only root and the shell can use su. */
    myuid = getuid();
    if (myuid != AID_ROOT && myuid != AID_SHELL) {
        fprintf(stderr,"su: uid %d not allowed to su\n", myuid);
        return 1;
    }

    if(argc < 2) {
        uid = gid = 0;
    } else {
        user = strtok(argv[1], ".");

        if(user != NULL) {
            uid = parse_uid(user);

            if((group = strtok(NULL, ",")) != NULL) {
                gid = parse_gid(group);

                memset(supp_gids, 0, sizeof(supp_gids));
                while((group = strtok(NULL, ",")) && (num_gids < NR_SUPP_GIDS)) {
                    supp_gids[num_gids] = parse_gid(group);
                    num_gids++;
                }
            } else {
                gid = uid;
            }
        }
    }

    /* set primary gid */
    if(setgid(gid)) {
        fprintf(stderr,"su: permission denied setting primary group\n");
        return 1;
    }

    /* set supplemental gids, if needed */
    if(num_gids > 0) {
        if(setgroups(num_gids, supp_gids)) {
            fprintf(stderr,"su: permission denied setting supplemental groups\n");
            return 1;
        }
    }

    if(setuid(uid)) {
        fprintf(stderr,"su: permission denied setting uid\n");
        return 1;
    }

    /* User specified command for exec. */
    if (argc == 3 ) {
        if (execlp(argv[2], argv[2], NULL) < 0) {
            fprintf(stderr, "su: exec failed for %s Error:%s\n", argv[2],
                    strerror(errno));
            return -errno;
        }
    } else if (argc > 3) {
        /* Copy the rest of the args from main. */
        char *exec_args[argc - 1];
        memset(exec_args, 0, sizeof(exec_args));
        memcpy(exec_args, &argv[2], sizeof(exec_args));
        if (execvp(argv[2], exec_args) < 0) {
            fprintf(stderr, "su: exec failed for %s Error:%s\n", argv[2],
                    strerror(errno));
            return -errno;
        }
    }

    /* Default exec shell. */
    execlp("/system/bin/sh", "sh", NULL);

    fprintf(stderr, "su: exec failed\n");
    return 1;
}
