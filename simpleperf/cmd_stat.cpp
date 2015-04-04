#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>

#include "command.h"
#include "environment.h"
#include "event.h"
#include "event_attr.h"
#include "event_fd.h"
#include "workload.h"

static std::vector<std::string> default_measured_event_names {
  "cpu-cycles",
  "stalled-cycles-frontend",
  "stalled-cycles-backend",
  "instructions",
  "branch-instructions",
  "branch-misses",
  "task-clock",
  "context-switches",
  "page-faults",
};

class StatCommand : public Command {
 public:
  StatCommand()
    : Command("stat",
              "gather performance counter information",
              "Usage: simpleperf stat [options] [command [command-args]]\n"
              "    Gather performance counters information to run [command]. If [command]\n"
              "is not specified, sleep 1 is used instead.\n"
              "    -e event1,event2,...  Select the event list to count. Use `simpleperf list`\n"
              "to find possible event names.\n"
              "    -a collect system-wide information.\n"
              "    --verbose             Show result in verbose mode.\n"
              "    --help                Print this help info.\n"
              ) {
    help_option = false;
    verbose_option = false;
    all_cpus_option = false;
  }

  bool RunCommand(std::vector<std::string>& args) override;

 private:
  bool ParseOptions(const std::vector<std::string>& args, std::vector<std::string>& non_option_args);
  void AddDefaultMeasuredEvents();
  bool AddMeasuredEvents(std::vector<std::string>& event_names);
  bool OpenEventFilesForProcess(pid_t pid);
  bool OpenEventFilesForCpus(const std::vector<int>& cpus);
  bool StartCounting();
  bool ReadEventFiles();
  void ShowCounters(int64_t elapsed_ns);

  static void DefaultWorkLoadFn() {
    sleep(1);
  }

 private:
  struct EventElem {
    const Event* event;
    std::vector<std::unique_ptr<EventFd>> event_fds;
    std::vector<PerfCountStruct> event_counters;
    PerfCountStruct counter_sum;
  };

  std::vector<EventElem> measured_events;

  bool help_option;
  bool verbose_option;
  bool all_cpus_option;
};

bool StatCommand::RunCommand(std::vector<std::string>& args) {
  printf("stat command run\n");

  std::vector<std::string> non_option_args;
  if (!ParseOptions(args, non_option_args)) {
    fprintf(stderr, "StatCommand::ParseOptions() failed\n");
    fprintf(stderr, "%s\n", DetailedHelpInfo());
    return false;
  }
  if (help_option) {
    printf("%s\n", DetailedHelpInfo());
    return true;
  }

  if (measured_events.size() == 0) {
    AddDefaultMeasuredEvents();
  }

  std::unique_ptr<WorkLoad> work_load;
  if (non_option_args.size() == 0) {
    work_load = WorkLoad::CreateWorkLoadInCurrentProcess(DefaultWorkLoadFn);
  } else {
    work_load = WorkLoad::CreateWorkLoadInNewProcess(non_option_args);
  }

  if (all_cpus_option == true) {
    if (!OpenEventFilesForCpus(Environment::GetOnlineCpus())) {
      fprintf(stderr, "StatCommand::OpenEventFilesForCpus() failed: %s\n", strerror(errno));
      return false;
    }
  } else {
    if (!OpenEventFilesForProcess(work_load->GetWorkProcess())) {
      fprintf(stderr, "StatCommand::OpenEventFilesForProcess() failed: %s\n", strerror(errno));
      return false;
    }
  }

  // Counting has enable_on_exec flag. If work_load doesn't call exec(), we need to start counting manually.
  if (!work_load->UseExec()) {
    if (!StartCounting()) {
      fprintf(stderr, "StatCommand::StartCounting() failed: %s\n", strerror(errno));
      return false;
    }
  }

  int64_t start_time = Environment::NanoTime();

  if (!work_load->Start()) {
    fprintf(stderr, "StatCommand start workload failed\n");
    return false;
  }

  if (!work_load->WaitFinish()) {
    fprintf(stderr, "StatCommand wait workload finish failed\n");
    return false;
  }

  int64_t end_time = Environment::NanoTime();

  if (!ReadEventFiles()) {
    fprintf(stderr, "StatCommand::ReadEventFiles() failed\n");
    return false;
  }
  printf("Run command stat successfully!\n");
  ShowCounters(end_time - start_time);
  return true;
}

bool StatCommand::ParseOptions(const std::vector<std::string>& args,
                               std::vector<std::string>& non_option_args) {
  size_t i;
  for (i = 0; i < args.size() && args[i][0] == '-'; ++i) {
    if (args[i] == "-e") {
      if (i + 1 == args.size()) {
        return false;
      }
      std::string event_list_str = args[i + 1];
      std::vector<std::string> event_list;
      size_t pos = 0, npos;
      while ((npos = event_list_str.find(",", pos)) != event_list_str.npos) {
        event_list.push_back(event_list_str.substr(pos, npos - pos));
        pos = npos + 1;
      }
      if (pos != event_list_str.size()) {
        event_list.push_back(event_list_str.substr(pos));
      }
      if (!AddMeasuredEvents(event_list)) {
        return false;
      }
      ++i;
    } else if (args[i] == "-a") {
      all_cpus_option = true;
    } else if (args[i] == "--verbose") {
      verbose_option = true;
    } else if (args[i] == "--help") {
      help_option = true;
    }
  }

  non_option_args.clear();
  for (; i < args.size(); ++i) {
    non_option_args.push_back(args[i]);
  }
  return true;
}

void StatCommand::AddDefaultMeasuredEvents() {
  AddMeasuredEvents(default_measured_event_names);
}

bool StatCommand::AddMeasuredEvents(std::vector<std::string>& event_names) {
  bool all_supported = true;
  for (auto& name : event_names) {
    auto event = Event::FindEventByName(name);
    if (event->Supported()) {
      EventElem elem;
      elem.event = event;
      measured_events.push_back(std::move(elem));
    } else {
      all_supported = false;
    }
  }
  return all_supported;
}

bool StatCommand::OpenEventFilesForProcess(pid_t pid) {
  for (auto& elem : measured_events) {
    const Event* event = elem.event;
    EventAttr attr(event, false);
    attr.EnableOnExec();
    auto event_fd = EventFd::OpenEventFileForProcess(attr, pid);
    if (event_fd == nullptr) {
      for (auto& elem : measured_events) {
        elem.event_fds.clear();
      }
      return false;
    }
    auto& event_fds = elem.event_fds;
    event_fds.clear();
    event_fds.push_back(std::move(event_fd));
  }
  return true;
}

bool StatCommand::OpenEventFilesForCpus(const std::vector<int>& cpu_list) {
  for (auto& elem : measured_events) {
    const Event* event = elem.event;
    EventAttr attr(event, true);
    auto& event_fds = elem.event_fds;
    event_fds.clear();
    for (auto cpu : cpu_list) {
      auto event_fd = EventFd::OpenEventFileForCpu(attr, cpu);
      if (event_fd == nullptr) {
        for (auto& elem : measured_events) {
          elem.event_fds.clear();
        }
        return false;
      }
      event_fds.push_back(std::move(event_fd));
    }
  }
  return true;
}

bool StatCommand::StartCounting() {
  for (auto& elem : measured_events) {
    for (auto& event_fd : elem.event_fds) {
      if (!event_fd->EnableEvent()) {
        return false;
      }
    }
  }
  return true;
}

bool StatCommand::ReadEventFiles() {
  for (auto& elem : measured_events) {
    auto& event_fds = elem.event_fds;
    auto& event_counters = elem.event_counters;
    event_counters.clear();
    for (auto& event_fd : event_fds) {
      PerfCountStruct counter;
      if (!event_fd->ReadCounter(counter)) {
        return false;
      } else {
        event_counters.push_back(counter);
      }
    }
    PerfCountStruct counter_sum;
    for (auto& counter : event_counters) {
      counter_sum.count += counter.count;
      counter_sum.time_enabled += counter.time_enabled;
      counter_sum.time_running += counter.time_running;
    }
    elem.counter_sum = counter_sum;
  }
  return true;
}

static std::string FormatCount(int64_t count);

void StatCommand::ShowCounters(int64_t elapsed_ns) {
  printf("Performance counter statistics:\n\n");
  for (auto& elem : measured_events) {
    auto& event = elem.event;
    auto& counter = elem.counter_sum;

    bool scalled = false;
    int64_t scalled_count = counter.count;

    if (counter.time_running < counter.time_enabled) {
      scalled = true;
      if (counter.time_running == 0) {
        scalled_count = 0;
      } else {
        scalled_count = static_cast<int64_t>(static_cast<double>(counter.count) *
                                counter.time_enabled / counter.time_running);
      }
    }

    if (verbose_option == false) {
      printf("%30s%s  %s\n", FormatCount(scalled_count).c_str(),
                             scalled ? "(scalled)" : "",
                             event->Name());
    } else {
      printf("%30s%s (real_count %s, enabled_time %s, running_time %s)  %s\n",
              FormatCount(scalled_count).c_str(), scalled ? "(scalled)" : "",
              FormatCount(counter.count).c_str(), FormatCount(counter.time_enabled).c_str(),
              FormatCount(counter.time_running).c_str(), event->Name());
    }
  }

  printf("\n");
  printf("Total test time: %" PRId64 ".%" PRId64 " seconds.\n", elapsed_ns / 1000000000,
                                                                elapsed_ns % 1000000000);
}

static std::string FormatCount(int64_t count) {
  std::string result;
  if (count == 0) {
    result.push_back('0');
  }

  for (int level = 0; count != 0; ++level) {
    result.push_back('0' + count % 10);
    count /= 10;
    if (level % 3 == 2 && count != 0) {
      result.push_back(',');
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

StatCommand stat_cmd;
