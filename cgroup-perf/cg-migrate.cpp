/*
**
** Copyright 2018, The Android Open Source Project
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

#include "cg-utils.h"

#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

using namespace std;
using namespace android::cgroupperf;

#define THREAD_SYNC_INIT 0
#define THREAD_SYNC_CHILD_READY 1

static const string procs_file = "/cgroup.procs";
static const string usage_example =
    "cg-migrate -s /dev/cpuset/cg1 -d /dev/cpuset/cg2 -i 1000 -c 100";

static void usage() {
    cout << "Migrate task from <src> cgroup"
        << " to <dest> cgroup and back <count> times" << endl
        << "Usage: cg-migrate [-s|-S] <src> [-d|-D] <dest> -i <iterations>"
        << " -c <child count> -r <run script>" << endl
        << "  src: source cgroup path to migrate from" << endl
        << "  dest: source cgroup path to migrate to" << endl
        << "  iteations: number of iterations to run" << endl
        << "  child count: number of child processes in the cgroup" << endl
        << "  run script: script to run after cgroups are created"
        << " and before test starts. cgroup is passed as a parameter" << endl
        << "Example: " << usage_example << endl;
}

static bool migrate(const string& procs_path, const string& pid_str,
                    double& min_duration_ns, double& max_duration_ns) {
    double duration_ns;
    enum file_write_res res;

    res = timed_file_write(procs_path, pid_str, duration_ns);
    switch (res) {
    case (success):
        if (min_duration_ns > duration_ns) {
           min_duration_ns = duration_ns;
        }
        if (max_duration_ns < duration_ns) {
           max_duration_ns = duration_ns;
        }
        break;
    case (open_err):
        cerr << "Failed to open " << procs_path << " for writing: "
            << strerror(errno) << endl;
        break;
    case (write_err):
        cerr << "Write to file " << procs_path << " failed: "
            << strerror(errno) << endl;
        break;
    }

    return (res == success);
}

static inline void err_on(bool cond, const string& msg) {
    if (cond) {
        cerr << msg << endl;
        abort();
    }
}

void* sleep_forever(void *arg __unused) {
    while (true) {
        sleep(10000);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    char pid_buf[32];

    int iterations = 0;
    int child_count = 1;
    string src, dest;
    string src_file, dest_file;
    bool create_src_dir = true;
    bool create_dest_dir = true;
    string script;
    cgroup_controller src_ctrl, dest_ctrl;
    thread_sync ts;

    int c;
    double min_duration_ns = DBL_MAX;
    double max_duration_ns = 0;


    while ((c = getopt(argc, argv, "s:S:d:D:i:c:r:h")) != -1) {
        switch (c) {
        case 's':
            src = optarg;
            break;
        case 'S':
            src = optarg;
            create_src_dir = false;
            break;
        case 'd':
            dest = optarg;
            break;
        case 'D':
            dest = optarg;
            create_dest_dir = false;
            break;
        case 'i':
            iterations = atoi(optarg);
            break;
        case 'c':
            child_count = atoi(optarg);
            break;
        case 'r':
            script = optarg;
            break;
        case 'h':
        default:
            usage();
            abort();
        }
    }

    err_on(src.size() == 0,
        "Source cgroup path parameter is not specified");
    err_on(dest.size() == 0,
        "Destination cgroup path parameter is not specified");

    if (iterations < 1) {
        cerr << "Invalid number of iterations: " << iterations << endl;
        abort();
    }
    if (child_count < 1) {
        cerr << "Invalid number of child processes (min 1): "
            << iterations << endl;
        abort();
    }

    if (create_src_dir) {
        if (access(src.c_str(), F_OK) == 0) {
            cerr << "Path " << src << " already exists" << endl;
            abort();
        }

        if (!cgroup_create(src, &src_ctrl)) {
            cerr << "Failed to create " << src << " cgroup" << endl;
            abort();
        }
    } else {
        if (!cgroup_is_valid(src, &src_ctrl)) {
            cerr << src << " is not a valid cgroup mount location" << endl;
            abort();
        }
    }

    if (create_dest_dir) {
        if (access(dest.c_str(), F_OK) == 0) {
            cerr << "Path " << dest << " already exists" << endl;
            abort();
        }

        if (!cgroup_create(dest, &dest_ctrl)) {
            cerr << "Failed to create " << dest << " cgroup" << endl;
            abort();
        }
    } else {
        if (!cgroup_is_valid(dest, &dest_ctrl)) {
            cerr << dest << " is not a valid cgroup mount location" << endl;
            abort();
        }
    }

    if (src_ctrl != dest_ctrl) {
        cerr << "Cgroup controllers for " << src << " and "
            << dest << "are different" << endl;
        abort();
    }

    src_file = src + procs_file;
    if (access(src_file.c_str(), R_OK | W_OK) < 0) {
        cerr << src_file << " access error: " << strerror(errno) << endl;
        abort();
    }

    dest_file = dest + procs_file;
    if (access(dest_file.c_str(), R_OK | W_OK) < 0) {
        cerr << dest_file << " access error: " << strerror(errno) << endl;
        abort();
    }

    if (!ts.init(THREAD_SYNC_INIT)) {
        cerr << "Failed to create shared object" << endl;
        abort();
    }

    /* Fork a child that will be migrated */
    pid_t pid = fork();
    if (!pid) {
        /* Child will spawn children and wait until killed */
        pthread_t thread;

        /* Account for the newly forked child process */
        child_count--;
        /* Spawn aditional child threads */
        while (child_count--) {
            if (pthread_create(&thread, NULL, sleep_forever, NULL)) {
                cerr << "Failed to create a thread" << endl;
                abort();
            }
        }

        /* Signal the parent we are ready */
        ts.signal_state(THREAD_SYNC_CHILD_READY);
        /* Child will wait until killed */
        sleep_forever(NULL);
        /* Shoud not get here */
        exit(0);
    }

    /* Wait for child to get ready */
    ts.wait_for_state(THREAD_SYNC_CHILD_READY);

    /* Parent will use child pid for migrations */
    snprintf(pid_buf, sizeof(pid_buf), "%u", pid);

    /* Run script */
    if (script.length()) {
        if (system((script + " " + src).c_str()) < 0 ||
            system((script + " " + dest).c_str()) < 0) {
            cerr << "Failed to execute script " << script << endl;
            abort();
        }
    }

    chrono::time_point<chrono::high_resolution_clock> test_start =
        chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        if (!migrate(dest_file, pid_buf, min_duration_ns, max_duration_ns)) {
           abort();
        }
        if (!migrate(src_file, pid_buf, min_duration_ns, max_duration_ns)) {
           abort();
        }
    }
    double duration_ns = chrono::duration_cast<chrono::nanoseconds>
        (chrono::high_resolution_clock::now() - test_start).count();

    cout << "Migration durations:" << std::fixed
        << std::setprecision(2) << endl
        << "\tavg:" << std::right << std::setw(20)
        << duration_ns / (iterations * 2) << " ns" << endl
        << "\tmin:" << std::right << std::setw(20)
        << min_duration_ns << " ns" << endl
        << "\tmax:" << std::right << std::setw(20)
        << max_duration_ns << " ns" << endl;

    if (pid > 0) {
        kill(pid, SIGKILL);
    }

    if (create_dest_dir && cgroup_remove(dest) < 0) {
        cerr << "Failed to remove " << dest
            << " error: " << strerror(errno) << endl;
        abort();
    }
    if (create_src_dir && cgroup_remove(src) < 0) {
        cerr << "Failed to remove " << src
            << " error: " << strerror(errno) << endl;
        abort();
    }

    return 0;
}
