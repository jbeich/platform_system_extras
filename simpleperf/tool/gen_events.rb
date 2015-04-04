#!/usr/bin/ruby

# return string like: Event cpu_cycles_event{"cpu-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES};
def gen_event_definition_str(variable_name, event_name, type, config)
  "Event #{variable_name}{\"#{event_name}\", #{type},\n  #{config}};"
end

# return string like:
# "std::vector<Event*> hardware_events{&cpu_cycles_event,
#                                    &instructions_event};"
#
def gen_event_array_definition_str(array_name, events)
  str = "std::vector<const Event*> #{array_name}{\n"
  indent_spaces = str[0..-2].gsub(/./, " ")
  events.each {|event| str += indent_spaces + "&" + event + ",\n" }
  str += indent_spaces[0..-2] + "};\n"
end

def gen_hardware_events()
  hardware_configs = ["cpu-cycles",
                      "instructions",
                      "cache-references",
                      "cache-misses",
                      "branch-instructions",
                      "branch-misses",
                      "bus-cycles",
                      "stalled-cycles-frontend",
                      "stalled-cycles-backend",
                      #["ref-cycles", "PERF_COUNT_HW_REF_CPU_CYCLES"],
                     ]
  hardware_events = Array.new
  generated_str = ""
  hardware_configs.collect do |config|
    if config.kind_of?(Array)
      variable_name = config[0].gsub(/-/, "_") + "_event"
      event_name = config[0]
      config_name = config[1]
    else
      variable_name = config.gsub(/-/, "_") + "_event"
      event_name = config
      config_name = "PERF_COUNT_HW_" + config.gsub(/-/, "_").upcase
    end
    event_definition_str = gen_event_definition_str(variable_name, event_name,
                                                    "PERF_TYPE_HARDWARE", config_name)
    generated_str += event_definition_str + "\n"
    hardware_events.push(variable_name)
  end
  generated_str += "\n" + gen_event_array_definition_str("hardware_events", hardware_events) + "\n";
end

def gen_software_events()
  software_configs = ["cpu-clock",
                      "task-clock",
                      "page-faults",
                      "context-switches",
                      "cpu-migrations",
                      ["minor-faults", "PERF_COUNT_SW_PAGE_FAULTS_MIN"],
                      ["major-faults", "PERF_COUNT_SW_PAGE_FAULTS_MAJ"],
                      "alignment-faults",
                      "emulation-faults",
                      #"dummy",
                     ]
  software_events = Array.new
  generated_str = ""
  software_configs.collect do |config|
    if config.kind_of?(Array)
      variable_name = config[0].gsub(/-/, "_") + "_event"
      event_name = config[0]
      config_name = config[1]
    else
      variable_name = config.gsub(/-/, "_") + "_event"
      event_name = config
      config_name = "PERF_COUNT_SW_" + config.gsub(/-/, "_").upcase
    end
    event_definition_str = gen_event_definition_str(variable_name, event_name,
                                                    "PERF_TYPE_SOFTWARE", config_name)
    generated_str += event_definition_str + "\n"
    software_events.push(variable_name)
  end
  generated_str += "\n" + gen_event_array_definition_str("software_events", software_events) + "\n"
end

def gen_hw_cache_events()
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

  hw_cache_events = Array.new
  generated_str = ""
  hw_cache_types.each do |type_name, type_config|
    hw_cache_ops.each do |op_name_access, op_name_miss, op_config|
      hw_cache_op_results.each do |result_name, result_config|
        if result_name == "accesses"
          variable_name = type_name.gsub(/-/, "_").downcase + "_" +
                          op_name_access.gsub(/-/, "_").downcase + "_event"
          event_name = type_name + "-" + op_name_access
        else
          variable_name = type_name.gsub(/-/, "_").downcase + "_" +
                          op_name_miss.gsub(/-/, "_").downcase + "_" +
                          result_name.gsub(/-/, "_").downcase + "_event"
          event_name = type_name + "-" + op_name_miss + "-" + result_name
        end
        config_name = "((#{type_config}) | (#{op_config} << 8) | (#{result_config} << 16))"
        event_definition_str = gen_event_definition_str(variable_name, event_name,
                                                        "PERF_TYPE_HW_CACHE", config_name)
        generated_str += event_definition_str + "\n"
        hw_cache_events.push(variable_name)
      end
    end
  end
  generated_str += "\n" + gen_event_array_definition_str("hwcache_events", hw_cache_events) + "\n"
end

def gen_events()
  generated_str = gen_hardware_events() + "\n"
  generated_str += gen_software_events() + "\n"
  generated_str += gen_hw_cache_events() + "\n"
end

# regenerate the content below substitute_pattern in file_path.
def gen_events_for_file(file_path, substitute_pattern)
  tmp_file_path = file_path + ".sub_tmp"
  tmp_file = File.open(tmp_file_path, "w")
  File.open(file_path, "r") do |file|
    file.each do |line|
      tmp_file.puts(line)
      if line =~ substitute_pattern
        break
      end
    end
  end
  tmp_file.puts()
  tmp_file.puts(gen_events())
  tmp_file.close
  `mv #{tmp_file_path} #{file_path}`
end

gen_events_for_file("../event.cpp", /gen_events\.rb/)
