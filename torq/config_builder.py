#
# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import textwrap
from abc import ABC, abstractmethod
from validation_error import ValidationError


class ConfigBuilder(ABC):
  """
  Abstract base class representing a config builder.
  """

  @abstractmethod
  def build_config(self, command):
    raise NotImplementedError


class DefaultConfigBuilder(ConfigBuilder):
  """
  Class representing a default config builder. Users can use the default
  dur-ms and ftrace-events, or overwrite the dur-ms and exclude and include
  ftrace events.
  """

  def build_config(self, command):
    if command.dur_ms is None:
      raise Exception("Cannot create config because a valid dur_ms was not"
                      " passed into the ConfigBuilder constructor.")

    ftrace_events = ["power/suspend_resume", "mm_event/mm_event_record",
                     "kmem/rss_stat", "ion/ion_stat",
                     "dmabuf_heap/dma_heap_stat", "kmem/ion_heap_grow",
                     "kmem/ion_heap_shrink", "sched/sched_process_exit",
                     "sched/sched_process_free", "sched/sched_switch",
                     "sched/sched_wakeup", "sched/sched_wakeup_new",
                     "sched/sched_waking", "task/task_newtask",
                     "task/task_rename", "lowmemorykiller/lowmemory_kill",
                     "oom/oom_score_adj_update", "oom/mark_victim",
                     "sched/sched_blocked_reason", "workqueue/*", "vmscan/*",
                     "ftrace/print", "power/cpu_frequency", "power/cpu_idle",
                     "power/gpu_frequency", "power/wakeup_source_activate",
                     "power/wakeup_source_deactivate", "gpu_mem/gpu_mem_total"]

    if command.exclude_ftrace_event is not None:
      for event in command.exclude_ftrace_event:
        if event in ftrace_events:
          ftrace_events.remove(event)
        else:
          return None, ValidationError(("Cannot remove ftrace event %s from"
                                        " config because it is not one"
                                        " of the config's ftrace events."
                                        % event), None)

    if command.include_ftrace_event is not None:
      for event in command.include_ftrace_event:
        if event not in ftrace_events:
          ftrace_events.append(event)
        else:
          return None, ValidationError(("Cannot add ftrace event %s to config"
                                        " because it is already one of the"
                                        " config's ftrace events." % event),
                                       None)

    ftrace_events_string = ("ftrace_events: \"%s\""
                            % ("\"\n      "
                                "ftrace_events: \"".join(ftrace_events)))

    config = f'''\
<<EOF

buffers: {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers: {{
  size_kb: 260096
  fill_policy: RING_BUFFER
}}

data_sources: {{
  config {{
    name: "linux.process_stats"
    process_stats_config {{
      scan_all_processes_on_start: true
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.log"
    android_log_config {{
    }}
  }}
}}

data_sources {{
  config {{
    name: "android.packages_list"
  }}
}}

data_sources: {{
  config {{
    name: "linux.sys_stats"
    target_buffer: 1
    sys_stats_config {{
      stat_period_ms: 500
      stat_counters: STAT_CPU_TIMES
      stat_counters: STAT_FORK_COUNT
  
      meminfo_period_ms: 1000
      meminfo_counters: MEMINFO_ACTIVE_ANON
      meminfo_counters: MEMINFO_ACTIVE_FILE
      meminfo_counters: MEMINFO_INACTIVE_ANON
      meminfo_counters: MEMINFO_INACTIVE_FILE
      meminfo_counters: MEMINFO_KERNEL_STACK
      meminfo_counters: MEMINFO_MLOCKED
      meminfo_counters: MEMINFO_SHMEM
      meminfo_counters: MEMINFO_SLAB
      meminfo_counters: MEMINFO_SLAB_UNRECLAIMABLE
      meminfo_counters: MEMINFO_VMALLOC_USED
      meminfo_counters: MEMINFO_MEM_FREE
      meminfo_counters: MEMINFO_SWAP_FREE
  
      vmstat_period_ms: 1000
      vmstat_counters: VMSTAT_PGFAULT
      vmstat_counters: VMSTAT_PGMAJFAULT
      vmstat_counters: VMSTAT_PGFREE
      vmstat_counters: VMSTAT_PGPGIN
      vmstat_counters: VMSTAT_PGPGOUT
      vmstat_counters: VMSTAT_PSWPIN
      vmstat_counters: VMSTAT_PSWPOUT
      vmstat_counters: VMSTAT_PGSCAN_DIRECT
      vmstat_counters: VMSTAT_PGSTEAL_DIRECT
      vmstat_counters: VMSTAT_PGSCAN_KSWAPD
      vmstat_counters: VMSTAT_PGSTEAL_KSWAPD
      vmstat_counters: VMSTAT_WORKINGSET_REFAULT
  
      # Below field not available on < Android SC-V2 releases.
      cpufreq_period_ms: 500
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.surfaceflinger.frametimeline"
    target_buffer: 2
  }}
}}

data_sources: {{
  config {{
    name: "linux.ftrace"
    target_buffer: 2
    ftrace_config {{
      {ftrace_events_string}
      atrace_categories: "aidl"
      atrace_categories: "am"
      atrace_categories: "dalvik"
      atrace_categories: "binder_lock"
      atrace_categories: "binder_driver"
      atrace_categories: "bionic"
      atrace_categories: "camera"
      atrace_categories: "disk"
      atrace_categories: "freq"
      atrace_categories: "idle"
      atrace_categories: "gfx"
      atrace_categories: "hal"
      atrace_categories: "input"
      atrace_categories: "pm"
      atrace_categories: "power"
      atrace_categories: "res"
      atrace_categories: "rro"
      atrace_categories: "sched"
      atrace_categories: "sm"
      atrace_categories: "ss"
      atrace_categories: "thermal"
      atrace_categories: "video"
      atrace_categories: "view"
      atrace_categories: "wm"
      atrace_apps: "lmkd"
      atrace_apps: "system_server"
      atrace_apps: "com.android.systemui"
      atrace_apps: "com.google.android.gms"
      atrace_apps: "com.google.android.gms.persistent"
      atrace_apps: "android:ui"
      atrace_apps: "com.google.android.apps.maps"
      atrace_apps: "*"
      buffer_size_kb: 16384
      drain_period_ms: 150
      symbolize_ksyms: true
    }}
  }}
}}
duration_ms: {command.dur_ms}
write_into_file: true
file_write_period_ms: 5000
max_file_size_bytes: 100000000000
flush_period_ms: 5000
incremental_state_config {{
  clear_period_ms: 5000
}}

EOF
    '''
    return textwrap.dedent(config), None
