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

#include <chrono>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cg-utils.h"

using namespace std;

namespace android {
namespace cgroupperf {

enum file_write_res write_file(const string& path,
                               const string& value) {
    int fd;

    if ((fd = open(path.c_str(), O_WRONLY)) < 0) {
        return open_err;
    }

    if (write(fd, value.c_str(), value.size()) < 0) {
        return write_err;
    }

    close(fd);
    return success;
}

enum file_write_res timed_file_write(const string& path,
                                     const string& value,
                                     double& duration_ns) {
    enum file_write_res res;
    chrono::time_point<chrono::high_resolution_clock> start;

    start = chrono::high_resolution_clock::now();
    res = write_file(path, value);
    duration_ns = chrono::duration_cast<chrono::nanoseconds>
        (chrono::high_resolution_clock::now() - start).count();

    return res;
}

static enum cgroup_controller cgroup_get_controller(const string& path) {
    enum cgroup_controller mask = controller_none;
    if (access((path + "/pids").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_pids);
    if (access((path + "/cpus").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_cpuset);
    if (access((path + "/cpu.shares").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_cpuctl);
    if (access((path + "/cpuacct.usage").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_cpuacct);
    if (access((path + "/devices.allow").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_devices);
    if (access((path + "/freezer.state").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_freezer);
    if (access((path + "/memory.limit_in_bytes").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_memory);
    if (access((path + "/net_cls.classid").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_net_cls);
    if (access((path + "/net_prio.prioidx").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_net_prio);
    if (access((path + "/schedtune.boost").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_schedtune);
    if (access((path + "/memory.current").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_memory_v2);
    if (access((path + "/cpu.weight").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_cpuctl_v2);
    if (access((path + "/cpuset.cpus").c_str(), F_OK) == 0)
        mask = (cgroup_controller)(mask | contoller_cpuset_v2);

    return mask;
}

static bool prepare_cgroup(const string& path, enum cgroup_controller gc) {
    if (cgroup_controller_present(gc, contoller_cpuset)) {
        /* cpus and mems files have to be populated */
        if (write_file((path + "/cpus"), "0-1") != success ||
            write_file((path + "/mems"), "0") != success) {
            return false;
        }
    }
    if (cgroup_controller_present(gc, contoller_cpuset_v2)) {
        /* cpuset.cpus and cpuset.mems files have to be populated */
        if (write_file((path + "/cpuset.cpus"), "0-1") != success ||
            write_file((path + "/cpuset.mems"), "0") != success) {
            return false;
        }
    }
    return true;
}

bool cgroup_is_valid(const string& path, enum cgroup_controller *controller) {
    enum cgroup_controller gc;

    if (access((path + "/cgroup.procs").c_str(), F_OK) < 0) {
        return false;
    }
    gc = cgroup_get_controller(path);
    if (gc == controller_none) {
        return false;
    }
    if (controller) {
        *controller = gc;
    }
    return true;
}

bool cgroup_create(const string& path, enum cgroup_controller *controller) {
    enum cgroup_controller gc;

    if (mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
              S_IROTH | S_IWOTH) < 0) {
        return false;
    }
    gc = cgroup_get_controller(path);
    if (gc == controller_none) {
        return false;
    }
    if (controller) {
        *controller = gc;
    }
    return prepare_cgroup(path, gc);
}

bool cgroup_remove(const string& path) {
    return (system(("rm -rf " + path + " > /dev/null 2>&1").c_str()) == 0);
}

bool cgroup_controller_present(enum cgroup_controller mask,
                               enum cgroup_controller controller) {
    return ((mask & controller) != 0);
}

bool thread_sync::init(int state) {
    sync_obj = (struct state_sync*)mmap(NULL, sizeof(struct state_sync),
                PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (sync_obj == MAP_FAILED) {
        return false;
    }

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sync_obj->mutex, &mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&sync_obj->condition, &cattr);

    sync_obj->state = state;

    return true;
}

thread_sync::~thread_sync() {
    if (sync_obj) {
        pthread_cond_destroy(&sync_obj->condition);
        pthread_mutex_destroy(&sync_obj->mutex);
        munmap(sync_obj, sizeof(struct state_sync));
    }
}

void thread_sync::signal_state(int state) {
    pthread_mutex_lock(&sync_obj->mutex);
    sync_obj->state = state;
    pthread_cond_signal(&sync_obj->condition);
    pthread_mutex_unlock(&sync_obj->mutex);
}

void thread_sync::wait_for_state(int state) {
    pthread_mutex_lock(&sync_obj->mutex);
    while (sync_obj->state != state) {
        pthread_cond_wait(&sync_obj->condition, &sync_obj->mutex);
    }
    pthread_mutex_unlock(&sync_obj->mutex);
}


} // namespace cgroupperf
} // namespace android

