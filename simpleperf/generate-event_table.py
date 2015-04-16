#!/usr/bin/env python
#
# Copyright (C) 2015 The Android Open Source Project
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

import shutil

# return string like: Event cpu_cycles_event{"cpu-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES};
def gen_event_definition_str(variable_name, event_name, type_name, config):
 return 'static Event %s{"%s", %s,\n  %s};' % (variable_name, event_name, type_name, config)

# return string like:
# std::vector<const Event*> hardware_events{
#                                           &cpu_cycles_event,
#                                           &instruction_events
#                                          };
def gen_event_array_definition_str(array_name, events):
  result = "static std::vector<const Event*> %s{\n" % array_name
  indent_space = " " * len(result[:-1])
  for event in events:
    result += "%s&%s,\n" % (indent_space, event)
  result += "%s};\n" % indent_space[:-1]
  return result

def gen_hardware_events():
  hardware_configs = ["cpu-cycles",
                      "instructions",
                      "cache-references",
                      "cache-misses",
                      "branch-instructions",
                      "branch-misses",
                      "bus-cycles",
                      "stalled-cycles-frontend",
                      "stalled-cycles-backend",
                      #["ref-cycles", "PERF_COUNT_HW_REF_CPU_CYCLES"], # Not defined in old perf_event.h.
                     ]
  hardware_events = []
  generated_str = ""
  for config in hardware_configs:
    if type(config) is list:
      variable_name = config[0].replace('-', '_') + "_event"
      event_name = config[0]
      config_name = config[1]
    else:
      variable_name = config.replace('-', '_') + "_event"
      event_name = config
      config_name = "PERF_COUNT_HW_" + config.replace('-', '_').upper()

    event_definition_str = gen_event_definition_str(variable_name, event_name,
                                                    "PERF_TYPE_HARDWARE", config_name)
    generated_str += event_definition_str + '\n'
    hardware_events.append(variable_name)

  generated_str += "\n" + gen_event_array_definition_str("hardware_events", hardware_events) + '\n'
  return generated_str

def gen_software_events():
  software_configs = ["cpu-clock",
                      "task-clock",
                      "page-faults",
                      "context-switches",
                      "cpu-migrations",
                      ["minor-faults", "PERF_COUNT_SW_PAGE_FAULTS_MIN"],
                      ["major-faults", "PERF_COUNT_SW_PAGE_FAULTS_MAJ"],
                      "alignment-faults",
                      "emulation-faults",
                      #"dummy",  # Not defined in old perf_event.h.
                     ]
  software_events = []
  generated_str = ""
  for config in software_configs:
    if type(config) is list:
      variable_name = config[0].replace('-', '_') + "_event"
      event_name = config[0]
      config_name = config[1]
    else:
      variable_name = config.replace('-', '_') + "_event"
      event_name = config
      config_name = "PERF_COUNT_SW_" + config.replace('-', '_').upper()

    event_definition_str = gen_event_definition_str(variable_name, event_name,
                                                    "PERF_TYPE_SOFTWARE", config_name)
    generated_str += event_definition_str + '\n'
    software_events.append(variable_name)

  generated_str += "\n" + gen_event_array_definition_str("software_events", software_events) + '\n'
  return generated_str

def gen_hw_cache_events():
  hw_cache_types = [["L1-dcache", "PERF_COUNT_HW_CACHE_L1D"],
                    ["L1-icache", "PERF_COUNT_HW_CACHE_L1I"],
                    ["LLC", "PERF_COUNT_HW_CACHE_LL"],
                    ["dTLB", "PERF_COUNT_HW_CACHE_DTLB"],
                    ["iTLB", "PERF_COUNT_HW_CACHE_ITLB"],
                    ["branch", "PERF_COUNT_HW_CACHE_BPU"],
                    ["node", "PERF_COUNT_HW_CACHE_NODE"],
                   ]
  hw_cache_ops =   [["loades", "load", "PERF_COUNT_HW_CACHE_OP_READ"],
                    ["stores", "store", "PERF_COUNT_HW_CACHE_OP_WRITE"],
                    ["prefetches", "prefetch", "PERF_COUNT_HW_CACHE_OP_PREFETCH"],
                   ]
  hw_cache_op_results = [["accesses", "PERF_COUNT_HW_CACHE_RESULT_ACCESS"],
                         ["misses", "PERF_COUNT_HW_CACHE_RESULT_MISS"],
                        ]
  hw_cache_events = []
  generated_str = ""
  for (type_name, type_config) in hw_cache_types:
    for (op_name_access, op_name_miss, op_config) in hw_cache_ops:
      for (result_name, result_config) in hw_cache_op_results:
        if result_name == "accesses":
          variable_name = type_name.replace('-', '_').lower() + '_' + \
                          op_name_access.replace('-', '_').lower() + "_event"
          event_name = type_name + '-' + op_name_access
        else:
          variable_name = type_name.replace('-', '_').lower() + '_' + \
                          op_name_miss.replace('-', '_').lower() + '_' + \
                          result_name.replace('-', '_').lower() + "_event"
          event_name = type_name + '-' + op_name_miss + '-' + result_name
        config_name = "((%s) | (%s << 8) | (%s << 16))" % (type_config, op_config, result_config)
        event_definition_str = gen_event_definition_str(variable_name, event_name,
                                                        "PERF_TYPE_HW_CACHE", config_name)
        generated_str += event_definition_str + '\n'
        hw_cache_events.append(variable_name)

  generated_str += '\n' + gen_event_array_definition_str("hwcache_events", hw_cache_events) + '\n'
  return generated_str

def gen_events():
  generated_str = "// This file is auto-generated by generate-event_table.py.\n\n"
  generated_str += gen_hardware_events() + '\n'
  generated_str += gen_software_events() + '\n'
  generated_str += gen_hw_cache_events() + '\n'
  return generated_str

generated_str = gen_events()
fh = open('event_table.h', 'w')
fh.write(generated_str)
fh.close()
