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

#ifndef SYSTEM_EXTRAS_CGROUP_PERF_CG_UTILS_H_
#define SYSTEM_EXTRAS_CGROUP_PERF_CG_UTILS_H_

#include <string>

namespace android {
namespace cgroupperf {

enum file_write_res {
    success,
    open_err,
    write_err,
};

enum cgroup_controller {
    controller_none = 0x0,
    contoller_pids = 0x1,
    contoller_cpuset = 0x2,
    contoller_cpuctl = 0x4,
    contoller_cpuacct = 0x8,
    contoller_devices = 0x10,
    contoller_freezer = 0x20,
    contoller_memory = 0x40,
    contoller_net_cls = 0x80,
    contoller_net_prio = 0x100,
    contoller_schedtune = 0x200,
    contoller_memory_v2 = 0x400,
    contoller_cpuctl_v2 = 0x800,
    contoller_cpuset_v2 = 0x1000,
};

enum file_write_res write_file(const std::string& path,
                               const std::string& value);
enum file_write_res timed_file_write(const std::string& path,
         const std::string& value, double& duration_ns);
bool cgroup_is_valid(const std::string& path,
         enum cgroup_controller *controller);
bool cgroup_create(const std::string& path,
         enum cgroup_controller *controller);
bool cgroup_remove(const std::string& path);
bool cgroup_is_valid(const std::string& path,
         enum cgroup_controller *controller);
bool cgroup_controller_present(enum cgroup_controller mask,
         enum cgroup_controller controller);

/* Thread synchronization primitives */
class thread_sync {
private:
    struct state_sync {
        pthread_mutex_t mutex;
        pthread_cond_t condition;
        int state;
    } *sync_obj;

public:
    thread_sync() : sync_obj(NULL) {}
    virtual ~thread_sync();

    bool init(int state);
    void signal_state(int state);
    void wait_for_state(int state);
};

}  // namespace cgroupperf
}  // namespace android

#endif

