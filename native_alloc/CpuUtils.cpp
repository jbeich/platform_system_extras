/*
** Copyright 2014 The Android Open Source Project
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

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#include "CpuUtils.h"

void RaisePriority() {
  if (setpriority(PRIO_PROCESS, 0, -20)) {
    fprintf(stderr, "setpriority failed: %s\n", strerror(errno));
    abort();
  }
}

void LockToCpu(int cpu_to_lock) {
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  if (sched_getaffinity(0, sizeof(cpuset), &cpuset) != 0) {
    fprintf(stderr, "sched_getaffinity failed: %s\n", strerror(errno));
    abort();
  }

  if (cpu_to_lock < 0) {
    // Lock to the last active core we find.
    for (int i = 0; i < CPU_SETSIZE; i++) {
      if (CPU_ISSET(i, &cpuset)) {
        cpu_to_lock = i;
      }
    }
  } else if (!CPU_ISSET(cpu_to_lock, &cpuset)) {
    fprintf(stderr, "Cpu %d does not exist.\n", cpu_to_lock);
    abort();
  }

  if (cpu_to_lock < 0) {
    fprintf(stderr, "No cpus to lock.\n");
    abort();
  }

  CPU_ZERO(&cpuset);
  CPU_SET(cpu_to_lock, &cpuset);
  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
    fprintf(stderr, "sched_getaffinity failed: %s\n", strerror(errno));
    abort();
  }
}
