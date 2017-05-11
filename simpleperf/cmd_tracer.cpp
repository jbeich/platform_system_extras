/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>

#include <csignal>
#include <memory>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "command.h"
#include "utils.h"

namespace {

struct Event {
  std::string system;
  std::string name;
  uint64_t id;

  Event(const std::string& system, const std::string& name, uint64_t id = UINT64_MAX)
      : system(system), name(name), id(id) {}
};

struct SavedTracingContext {
  std::string buffer_size_kb;
  std::string overwrite;
  std::string clock;
  std::string current_tracer;
  std::vector<std::pair<Event, std::string>> enable_states;
};

static volatile std::sig_atomic_t g_signal_flag = 0;

class TracerCommand : public Command {
 public:
  TracerCommand()
      : Command(
          "tracer", "provide tracing events information.\n",
          // clang-format off
"Usage: simpleperf tracer [options]\n"
"       Provide tracing events information in /sys/kernel/debug/tracing.\n"
"--list-events                     List all tracing events.\n"
"--dump-events event1,event2,...   Dump format file for events.\n"
"--trace-events event1,event2,...  Trace events until stopped by Ctrl-C.\n"
"--clock clock_name                Set trace clock. Default is perf.\n"
"-o file_name                      Write output to file_name instead of stdout.\n"
          // clang-format on
        ),
        list_events_(false),
        clock_name_("perf") {
    InitPaths();
  }

  bool Run(const std::vector<std::string>& args);

 private:
  void InitPaths();
  std::string GetEventFormatPath(const Event& event) const;
  std::string GetEventEnablePath(const Event& event) const;
  std::string GetEventIdPath(const Event& event) const;

  bool ParseOptions(const std::vector<std::string>& args);
  bool ParseEventList(const std::string& s, std::vector<Event>* events);
  bool ParseClockName(const std::string& s, std::string* clock_name);
  bool GetTraceClock(std::string* clock);
  std::vector<Event> GetAllEvents() const;
  bool StartTrace(SavedTracingContext* context);
  bool RegisterSignalHandlers();
  bool FinishTrace(const SavedTracingContext& context);
  bool DumpTrace(FILE* fp);


  std::string tracing_dir_;
  std::string tracing_event_dir_;
  std::string buffer_size_kb_path_;
  std::string overwrite_path_;
  std::string trace_clock_path_;
  std::string current_tracer_path_;
  std::string tracing_on_path_;
  std::string trace_pipe_path_;

  bool list_events_;
  std::vector<Event> dump_events_;
  std::vector<Event> trace_events_;
  std::string clock_name_;
  std::string output_filename_;
};

void TracerCommand::InitPaths() {
  tracing_dir_ = "/sys/kernel/debug/tracing/";
  tracing_event_dir_ = tracing_dir_ + "events/";
  buffer_size_kb_path_ = tracing_dir_ + "buffer_size_kb";
  overwrite_path_ = tracing_dir_ + "options/overwrite";
  trace_clock_path_ = tracing_dir_ + "trace_clock";
  current_tracer_path_ = tracing_dir_ + "current_tracer";
  tracing_on_path_ = tracing_dir_ + "tracing_on";
  trace_pipe_path_ = tracing_dir_ + "trace_pipe";
}

std::string TracerCommand::GetEventFormatPath(const Event& event) const {
  return tracing_event_dir_ + event.system + "/" + event.name + "/format";
}

std::string TracerCommand::GetEventEnablePath(const Event& event) const {
  return tracing_event_dir_ + event.system + "/" + event.name + "/enable";
}

std::string TracerCommand::GetEventIdPath(const Event& event) const {
  return tracing_event_dir_ + event.system + "/" + event.name + "/id";
}

bool TracerCommand::Run(const std::vector<std::string>& args) {
  if (!ParseOptions(args)) {
    return false;
  }
  FILE* fp = stdout;
  std::unique_ptr<FILE, decltype(&fclose)> scoped_fp(nullptr, fclose);
  if (!output_filename_.empty()) {
    scoped_fp.reset(fopen(output_filename_.c_str(), "w"));
    if (scoped_fp == nullptr) {
      PLOG(ERROR) << "failed to call fopen";
      return false;
    }
    fp = scoped_fp.get();
  }
  if (list_events_) {
    std::vector<Event> events = GetAllEvents();
    for (auto& event : events) {
      fprintf(fp, "%s:%s %" PRIu64 "\n", event.system.c_str(), event.name.c_str(), event.id);
    }
  }
  if (!dump_events_.empty()) {
    for (auto& event : dump_events_) {
      std::string format;
      if (!ReadFile(GetEventFormatPath(event), &format)) {
        return false;
      }
      fprintf(fp, "%s\n", format.c_str());
    }
  }
  if (!trace_events_.empty()) {
    SavedTracingContext context;
    if (!StartTrace(&context)) {
      FinishTrace(context);
      return false;
    }
    if (!DumpTrace(fp)) {
      FinishTrace(context);
      return false;
    }
    if (!FinishTrace(context)) {
      return false;
    }
  }
  return true;
}

bool TracerCommand::ParseOptions(const std::vector<std::string>& args) {
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--list-events") {
      list_events_ = true;
    } else if (args[i] == "--dump-events") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      if (!ParseEventList(args[i], &dump_events_)) {
        return false;
      }
    } else if (args[i] == "--trace-events") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      if (!ParseEventList(args[i], &trace_events_)) {
        return false;
      }
    } else if (args[i] == "--clock") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      if (!ParseClockName(args[i], &clock_name_)) {
        return false;
      }
    } else if (args[i] == "-o") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      output_filename_ = args[i];
    } else {
      ReportUnknownOption(args, i);
      return false;
    }
  }
  return true;
}

bool TracerCommand::ParseEventList(const std::string& s, std::vector<Event>* events) {
  std::vector<std::string> strs = android::base::Split(s, ",");
  for (auto& str : strs) {
    int sep = str.find(':');
    if (sep == -1) {
      LOG(ERROR) << "wrong_event: " << str;
      return false;
    }
    std::string system = str.substr(0, sep);
    std::string name = str.substr(sep + 1);
    Event event(system, name);
    if (!IsRegularFile(GetEventFormatPath(event))) {
      LOG(ERROR) << "wrong_event: " << str;
      return false;
    }
    events->push_back(event);
  }
  return true;
}

bool TracerCommand::ParseClockName(const std::string& s, std::string* clock_name) {
  std::string content;
  if (!ReadFile(trace_clock_path_, &content)) {
    return false;
  }
  std::vector<std::string> strs = android::base::Split(content, " ");
  for (const auto& str : strs) {
    if (str == s || (str.size() > 2 && str[0] == '[' && str.back() == ']' &&
        str.substr(1, str.size() - 2) == s)) {
      *clock_name = s;
      return true;
    }
  }
  return false;
}

bool TracerCommand::GetTraceClock(std::string* clock) {
  std::string content;
  if (!ReadFile(trace_clock_path_, &content)) {
    return false;
  }
  int start = content.find('[');
  int end = content.find(']');
  if (start != -1 && end != -1 && start + 1 < end) {
    *clock = content.substr(start + 1, end - start - 1);
    return true;
  }
  LOG(ERROR) << "invalid trace_clock: " << content;
  return false;
}

std::vector<Event> TracerCommand::GetAllEvents() const {
  std::vector<Event> events;
  std::vector<std::string> systems = GetSubDirs(tracing_event_dir_);
  for (const auto& system : systems) {
    std::string system_path = tracing_event_dir_ + system;
    std::vector<std::string> names = GetSubDirs(system_path);
    for (const auto& name : names) {
      Event event(system, name);
      if (!IsRegularFile(GetEventIdPath(event))) {
        continue;
      }
      std::string id_content;
      if (!ReadFile(GetEventIdPath(event), &id_content)) {
        continue;
      }
      if (!android::base::ParseUint(android::base::Trim(id_content), &event.id)) {
        continue;
      }
      events.push_back(event);
    }
  }
  return events;
}

bool TracerCommand::StartTrace(SavedTracingContext* context) {
  if (!RegisterSignalHandlers()) {
    return false;
  }
  if (!WriteFile(tracing_on_path_, "0")) {
    return false;
  }
  if (!ReadFile(buffer_size_kb_path_, &context->buffer_size_kb) ||
      !WriteFile(buffer_size_kb_path_, "2048")) {
    return false;
  }
  if (!ReadFile(overwrite_path_, &context->overwrite) ||
      !WriteFile(overwrite_path_, "1")) {
    return false;
  }
  if (!GetTraceClock(&context->clock) || !WriteFile(trace_clock_path_, clock_name_)) {
    return false;
  }
  if (!ReadFile(current_tracer_path_, &context->current_tracer) ||
      !WriteFile(current_tracer_path_, "nop")) {
    return false;
  }
  for (auto& event : trace_events_) {
    std::string enable_state;
    if (!ReadFile(GetEventEnablePath(event), &enable_state) ||
        !WriteFile(GetEventEnablePath(event), "1")) {
      return false;
    }
    context->enable_states.push_back(std::make_pair(event, enable_state));
  }
  return WriteFile(tracing_on_path_, "1");
}

static void SignalHandler(int) {
  g_signal_flag = 1;
}

bool TracerCommand::RegisterSignalHandlers() {
  g_signal_flag = 0;
  std::vector<int> signals = { SIGHUP, SIGINT, SIGQUIT, SIGTERM };
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SignalHandler;
  for (int signal : signals) {
    if (sigaction(signal, &sa, nullptr) != 0) {
      PLOG(ERROR) << "sigaction";
      return false;
    }
  }
  return true;
}

bool TracerCommand::FinishTrace(const SavedTracingContext& context) {
  if (!WriteFile(tracing_on_path_, "0")) {
    return false;
  }
  if (!WriteFile(buffer_size_kb_path_, context.buffer_size_kb)) {
    return false;
  }
  if (!WriteFile(overwrite_path_, context.overwrite)) {
    return false;
  }
  if (!WriteFile(trace_clock_path_, context.clock)) {
    return false;
  }
  if (!WriteFile(current_tracer_path_, context.current_tracer)) {
    return false;
  }
  for (auto& pair : context.enable_states) {
    if (!WriteFile(GetEventEnablePath(pair.first), pair.second)) {
      return false;
    }
  }
  return true;
}

bool TracerCommand::DumpTrace(FILE* fp) {
  FileHelper fh = FileHelper::OpenReadOnly(trace_pipe_path_);
  if (!fh) {
    PLOG(ERROR) << "open";
    return false;
  }
  char buf[4096];
  while (g_signal_flag != 1) {
    ssize_t n = read(fh.fd(), buf, sizeof(buf));
    if (n > 0) {
      if (fwrite(buf, n, 1, fp) != 1) {
        PLOG(ERROR) << "fwrite";
        return false;
      }
    }
  }
  return true;
}

}  // namespace

void RegisterTracerCommand() {
  RegisterCommand("tracer",
                  [] { return std::unique_ptr<Command>(new TracerCommand()); });
}

