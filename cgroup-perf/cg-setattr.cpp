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
#include <string>
#include <unistd.h>

using namespace std;
using namespace android::cgroupperf;

#define THREAD_SYNC_INIT 0
#define THREAD_SYNC_CHILD_READY 1

static const string procs_file = "/cgroup.procs";
static const string usage_example =
    "cg-setattr -f /dev/cpuset/cg1/cpus -o 0-1 -n 0-7 -i 1000 -c 100";

static void usage() {
    cout << "Set cgroup attribute" << endl
        << "Usage: cg-setattr [-f|-F] <file path> -o <orig value>"
        << " -n <new value> -i <iterations> -r <run script>" << endl
        << "  file path: cgroup attribute file path" << endl
        << "  orig value: original value of the attribute" << endl
        << "  new value: new value of the attribute" << endl
        << "  iterations: number of iterations to run" << endl
        << "  child count: number of child processes in the cgroup" << endl
        << "  run script: script to run after cgroups are created"
        << " and before test starts. cgroup is passed as a parameter" << endl
        << "Example: " << usage_example << endl;
}

static bool set_attr(const string& path, const string& value,
                    double& min_duration_ns, double& max_duration_ns) {
    enum file_write_res res;
    double duration_ns;

    res = timed_file_write(path, value, duration_ns);
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
        cerr << "Failed to open " << path << " for writing: "
            << strerror(errno) << endl;
        break;
    case (write_err):
        cerr << "Write to file " << path << " failed: "
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
    int iterations = 0;
    int child_count = 1;
    string dir, path, orig_value, new_value;
    string script;
    int c;
    double min_duration_ns = DBL_MAX;
    double max_duration_ns = 0;
    bool create_dir = true;
    pid_t pid = 0;
    thread_sync ts;
    enum file_write_res res;
    char pid_buf[32];

    while ((c = getopt(argc, argv, "f:F:o:n:i:c:r:h")) != -1) {
        switch (c) {
        case 'f':
            path = optarg;
            break;
        case 'F':
            path = optarg;
            create_dir = false;
            break;
        case 'o':
            orig_value = optarg;
            break;
        case 'n':
            new_value = optarg;
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

    err_on(path.size() == 0,
        "File path parameter is not specified");
    err_on(orig_value.size() == 0,
        "Original value parameter is not specified");
    err_on(new_value.size() == 0,
        "New value parameter is not specified");
    if (iterations < 1) {
        cerr << "Invalid number of iterations: " << iterations << endl;
        abort();
    }
    if (child_count < 1) {
        cerr << "Invalid number of child processes (min 1): "
            << iterations << endl;
        abort();
    }

    dir = path.substr(0, path.find_last_of('/'));

    if (create_dir) {
        if (access(dir.c_str(), F_OK) == 0) {
            cerr << "Path " << dir << " already exists" << endl;
            abort();
        }

        if (!cgroup_create(dir, NULL)) {
            cerr << "Failed to create " << dir << " cgroup" << endl;
            abort();
        }
    }

    if (!ts.init(THREAD_SYNC_INIT)) {
        cerr << "Failed to create shared object" << endl;
        abort();
    }

    /* Fork a child that will be migrated */
    pid = fork();
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

    /* Add child into newly created cgroup */
    snprintf(pid_buf, sizeof(pid_buf), "%u", pid);
    res = write_file(dir + procs_file, pid_buf);
    switch (res) {
    case (success):
        cout << "Added child process " << pid
            << " into created cgroup" << endl;
        break;
    case (open_err):
        cerr << "Failed to open " << dir << procs_file << " for writing: "
            << strerror(errno) << endl;
        abort();
        break;
    case (write_err):
        cerr << "Write to file " << dir << procs_file << " failed: "
            << strerror(errno) << endl;
        abort();
        break;
    }

    if (access(path.c_str(), R_OK | W_OK) < 0) {
        cerr << path << " access error: " << strerror(errno) << endl;
        abort();
    }

    /* Run script */
    if (script.length()) {
        if (system((script + " " + dir).c_str()) < 0) {
            cerr << "Failed to execute script " << script << endl;
            abort();
        }
    }

    chrono::time_point<chrono::high_resolution_clock> test_start =
        chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        if (!set_attr(path, new_value, min_duration_ns, max_duration_ns)) {
           abort();
        }
        if (!set_attr(path, orig_value, min_duration_ns, max_duration_ns)) {
           abort();
        }
    }
    double duration_ns = chrono::duration_cast<chrono::nanoseconds>
        (chrono::high_resolution_clock::now() - test_start).count();

    cout << "Attribute set durations:"
        << std::fixed << std::setprecision(2) << endl
        << "\tavg:" << std::right << std::setw(20)
        << duration_ns / (iterations * 2) << " ns" << endl
        << "\tmin:" << std::right << std::setw(20)
        << min_duration_ns << " ns" << endl
        << "\tmax:" << std::right << std::setw(20)
        << max_duration_ns << " ns" << endl;

    if (pid > 0) {
        kill(pid, SIGKILL);
    }

    if (create_dir) {
        if (cgroup_remove(dir) < 0) {
            cerr << "Failed to remove " << dir
                << " error: " << strerror(errno) << endl;
            abort();
        }
    }

    return 0;
}
