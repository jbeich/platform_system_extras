/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "tracing.h"

#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/quick_exit.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/test_utils.h>

#include "command.h"
#include "perf_event.h"
#include "utils.h"
#include "workload.h"

#if defined(__linux__)
#include <sys/wait.h>
#endif

std::unique_ptr<Tracing> ScopedTracing::current_tracing_;

const char TRACING_INFO_MAGIC[10] = {23,  8,   68,  't', 'r',
                                     'a', 'c', 'i', 'n', 'g'};

std::string TracingValue::toString() const {
  switch (type) {
    case TRACING_VALUE_UNSIGNED:
      return std::to_string(unsigned_value);
    case TRACING_VALUE_SIGNED:
      return std::to_string(signed_value);
    case TRACING_VALUE_STRING:
      return string_value;
    default:
      return "unknown type";
  }
}

bool TracingField::ExtractValue(const char* data, size_t data_size, TracingValue* value) const {
  if (data_size < offset + elem_size * elem_count) {
    return false;
  }
  value->type = TracingValue::TRACING_VALUE_UNKNOWN;
  if (elem_count == 1) {
    if (is_signed) {
      value->type = TracingValue::TRACING_VALUE_SIGNED;
      value->signed_value = static_cast<int64_t>(ConvertBytesToValue(data + offset, elem_size));
    } else {
      value->type = TracingValue::TRACING_VALUE_UNSIGNED;
      value->unsigned_value = ConvertBytesToValue(data + offset, elem_size);
    }
  } else if (is_signed && elem_size == 1u) {
    char s[elem_count + 1];
    s[elem_count] = '\0';
    strncpy(s, data + offset, elem_count);
    value->type = TracingValue::TRACING_VALUE_STRING;
    value->string_value = s;
  }
  return true;
}

template <class T>
void AppendData(std::vector<char>& data, const T& s) {
  const char* p = reinterpret_cast<const char*>(&s);
  data.insert(data.end(), p, p + sizeof(T));
}

static void AppendData(std::vector<char>& data, const char* s) {
  data.insert(data.end(), s, s + strlen(s) + 1);
}

template <>
void AppendData(std::vector<char>& data, const std::string& s) {
  data.insert(data.end(), s.c_str(), s.c_str() + s.size() + 1);
}

template <>
void MoveFromBinaryFormat(std::string& data, const char*& p) {
  data.clear();
  while (*p != '\0') {
    data.push_back(*p++);
  }
  p++;
}

static void AppendFile(std::vector<char>& data, const std::string& file,
                       uint32_t file_size_bytes = 8) {
  if (file_size_bytes == 8) {
    uint64_t file_size = file.size();
    AppendData(data, file_size);
  } else if (file_size_bytes == 4) {
    uint32_t file_size = file.size();
    AppendData(data, file_size);
  }
  data.insert(data.end(), file.begin(), file.end());
}

static void DetachFile(const char*& p, std::string& file,
                       uint32_t file_size_bytes = 8) {
  uint64_t file_size = ConvertBytesToValue(p, file_size_bytes);
  p += file_size_bytes;
  file.clear();
  file.insert(file.end(), p, p + file_size);
  p += file_size;
}

struct TraceType {
  std::string system;
  std::string name;
};

class TracingFile {
 public:
  TracingFile();
  void AddEventFormat(const std::string& event, const std::string& format);
  std::vector<char> BinaryFormat() const;
  void LoadFromBinary(const std::vector<char>& data);
  void Dump(size_t indent) const;
  std::vector<TracingFormat> LoadTracingFormatsFromEventFiles() const;
  const std::string& GetKallsymsFile() const { return kallsyms_file; }
  uint32_t GetPageSize() const { return page_size; }

 private:
  char magic[10];
  std::string version;
  char endian;
  uint8_t size_of_long;
  uint32_t page_size;
  // header_page_file, header_event_file, ftrace_format_files,
  // kallsyms_file, printk_formats_file are only kept to be compatible with linux-tools-perf.
  std::string header_page_file;
  std::string header_event_file;

  std::vector<std::string> ftrace_format_files;
  // pair of system, format_file_data.
  std::vector<std::pair<std::string, std::string>> event_format_files;

  std::string kallsyms_file;
  std::string printk_formats_file;
};

TracingFile::TracingFile() {
  memcpy(magic, TRACING_INFO_MAGIC, sizeof(TRACING_INFO_MAGIC));
  version = "0.5";
  endian = 0;
  size_of_long = static_cast<int>(sizeof(long));
  page_size = static_cast<uint32_t>(::GetPageSize());
}

void TracingFile::AddEventFormat(const std::string& event, const std::string& format) {
  event_format_files.push_back(std::make_pair(event.substr(0, event.find(':')), format));
}

std::vector<char> TracingFile::BinaryFormat() const {
  std::vector<char> ret;
  ret.insert(ret.end(), magic, magic + sizeof(magic));
  AppendData(ret, version);
  ret.push_back(endian);
  AppendData(ret, size_of_long);
  AppendData(ret, page_size);
  AppendData(ret, "header_page");
  AppendFile(ret, header_page_file);
  AppendData(ret, "header_event");
  AppendFile(ret, header_event_file);
  int count = static_cast<int>(ftrace_format_files.size());
  AppendData(ret, count);
  for (const auto& format : ftrace_format_files) {
    AppendFile(ret, format);
  }
  count = static_cast<int>(event_format_files.size());
  AppendData(ret, count);
  for (const auto& pair : event_format_files) {
    AppendData(ret, pair.first);
    AppendData(ret, 1);
    AppendFile(ret, pair.second);
  }
  AppendFile(ret, kallsyms_file, 4);
  AppendFile(ret, printk_formats_file, 4);
  return ret;
}

void TracingFile::LoadFromBinary(const std::vector<char>& data) {
  const char* p = data.data();
  const char* end = data.data() + data.size();
  CHECK(memcmp(p, magic, sizeof(magic)) == 0);
  p += sizeof(magic);
  MoveFromBinaryFormat(version, p);
  MoveFromBinaryFormat(endian, p);
  MoveFromBinaryFormat(size_of_long, p);
  MoveFromBinaryFormat(page_size, p);
  std::string filename;
  MoveFromBinaryFormat(filename, p);
  CHECK_EQ(filename, "header_page");
  DetachFile(p, header_page_file);
  MoveFromBinaryFormat(filename, p);
  CHECK_EQ(filename, "header_event");
  DetachFile(p, header_event_file);
  uint32_t count;
  MoveFromBinaryFormat(count, p);
  ftrace_format_files.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    DetachFile(p, ftrace_format_files[i]);
  }
  MoveFromBinaryFormat(count, p);
  event_format_files.clear();
  for (uint32_t i = 0; i < count; ++i) {
    std::string system;
    MoveFromBinaryFormat(system, p);
    uint32_t count_in_system;
    MoveFromBinaryFormat(count_in_system, p);
    for (uint32_t i = 0; i < count_in_system; ++i) {
      std::string format;
      DetachFile(p, format);
      event_format_files.push_back(std::make_pair(system, std::move(format)));
    }
  }
  DetachFile(p, kallsyms_file, 4);
  DetachFile(p, printk_formats_file, 4);
  CHECK_EQ(p, end);
}

void TracingFile::Dump(size_t indent) const {
  PrintIndented(indent, "tracing data:\n");
  PrintIndented(indent + 1, "magic: ");
  for (size_t i = 0; i < 3u; ++i) {
    printf("0x%x ", magic[i]);
  }
  for (size_t i = 3; i < sizeof(magic); ++i) {
    printf("%c", magic[i]);
  }
  printf("\n");
  PrintIndented(indent + 1, "version: %s\n", version.c_str());
  PrintIndented(indent + 1, "endian: %d\n", endian);
  PrintIndented(indent + 1, "header_page:\n%s\n\n", header_page_file.c_str());
  PrintIndented(indent + 1, "header_event:\n%s\n\n", header_event_file.c_str());
  for (size_t i = 0; i < ftrace_format_files.size(); ++i) {
    PrintIndented(indent + 1, "ftrace format file %zu/%zu:\n%s\n\n", i + 1,
                  ftrace_format_files.size(), ftrace_format_files[i].c_str());
  }
  for (size_t i = 0; i < event_format_files.size(); ++i) {
    PrintIndented(indent + 1, "event format file %zu/%zu %s:\n%s\n\n", i + 1,
                  event_format_files.size(),
                  event_format_files[i].first.c_str(),
                  event_format_files[i].second.c_str());
  }
  PrintIndented(indent + 1, "kallsyms:\n%s\n\n", kallsyms_file.c_str());
  PrintIndented(indent + 1, "printk_formats:\n%s\n\n",
                printk_formats_file.c_str());
}

enum class FormatParsingState {
  READ_NAME,
  READ_ID,
  READ_FIELDS,
  READ_PRINTFMT,
};

// Parse lines like: field:char comm[16]; offset:8; size:16;  signed:1;
static TracingField ParseTracingField(const std::string& s) {
  TracingField field;
  size_t start = 0;
  std::string name;
  std::string value;
  for (size_t i = 0; i < s.size(); ++i) {
    if (!isspace(s[i]) && (i == 0 || isspace(s[i - 1]))) {
      start = i;
    } else if (s[i] == ':') {
      name = s.substr(start, i - start);
      start = i + 1;
    } else if (s[i] == ';') {
      value = s.substr(start, i - start);
      if (name == "field") {
        size_t pos = value.find_first_of('[');
        if (pos == std::string::npos) {
          field.name = value;
          field.elem_count = 1;
        } else {
          field.name = value.substr(0, pos);
          field.elem_count =
              static_cast<size_t>(strtoull(&value[pos + 1], nullptr, 10));
        }
      } else if (name == "offset") {
        field.offset =
            static_cast<size_t>(strtoull(value.c_str(), nullptr, 10));
      } else if (name == "size") {
        size_t size = static_cast<size_t>(strtoull(value.c_str(), nullptr, 10));
        CHECK_EQ(size % field.elem_count, 0u);
        field.elem_size = size / field.elem_count;
      } else if (name == "signed") {
        int is_signed = static_cast<int>(strtoull(value.c_str(), nullptr, 10));
        field.is_signed = (is_signed == 1);
      }
    }
  }
  return field;
}

std::vector<TracingFormat> TracingFile::LoadTracingFormatsFromEventFiles()
    const {
  std::vector<TracingFormat> formats;
  for (const auto& pair : event_format_files) {
    TracingFormat format;
    format.system_name = pair.first;
    std::vector<std::string> strs = android::base::Split(pair.second, "\n");
    FormatParsingState state = FormatParsingState::READ_NAME;
    for (const auto& s : strs) {
      if (state == FormatParsingState::READ_NAME) {
        size_t pos = s.find("name:");
        if (pos != std::string::npos) {
          format.name = android::base::Trim(s.substr(pos + strlen("name:")));
          state = FormatParsingState::READ_ID;
        }
      } else if (state == FormatParsingState::READ_ID) {
        size_t pos = s.find("ID:");
        if (pos != std::string::npos) {
          format.id =
              strtoull(s.substr(pos + strlen("ID:")).c_str(), nullptr, 10);
          state = FormatParsingState::READ_FIELDS;
        }
      } else if (state == FormatParsingState::READ_FIELDS) {
        size_t pos = s.find("field:");
        if (pos != std::string::npos) {
          TracingField field = ParseTracingField(s);
          format.fields.push_back(field);
        }
      }
    }
    formats.push_back(format);
  }
  return formats;
}

Tracing::Tracing(const std::vector<char>& data) {
  tracing_file_ = new TracingFile;
  tracing_file_->LoadFromBinary(data);
}

Tracing::~Tracing() { delete tracing_file_; }

void Tracing::Dump(size_t indent) { tracing_file_->Dump(indent); }

const TracingFormat* Tracing::GetTracingFormatHavingId(uint64_t trace_event_id) {
  if (tracing_formats_.empty()) {
    tracing_formats_ = tracing_file_->LoadTracingFormatsFromEventFiles();
  }
  for (const auto& format : tracing_formats_) {
    if (format.id == trace_event_id) {
      return &format;
    }
  }
  LOG(FATAL) << "no tracing format for id " << trace_event_id;
  return nullptr;
}

std::string Tracing::GetTracingEventNameHavingId(uint64_t trace_event_id) {
  if (tracing_formats_.empty()) {
    tracing_formats_ = tracing_file_->LoadTracingFormatsFromEventFiles();
  }
  for (const auto& format : tracing_formats_) {
    if (format.id == trace_event_id) {
      return android::base::StringPrintf("%s:%s", format.system_name.c_str(),
                                         format.name.c_str());
    }
  }
  return "";
}

const std::string& Tracing::GetKallsyms() const {
  return tracing_file_->GetKallsymsFile();
}

uint32_t Tracing::GetPageSize() const { return tracing_file_->GetPageSize(); }

#if defined(__linux__)

class SimpleperfTracer : public Tracer {
 public:
  bool GetAllEvents(std::vector<std::pair<std::string, uint64_t>>* event_with_ids) override;
  bool GetEventFormats(const std::vector<std::string>& events,
                       std::vector<std::string>* formats) override;
  bool StartTracing(const std::vector<std::string>& events, const std::string& clock,
                    const std::string& output_filename) override;
  bool StopTracing() override;

 private:
  std::unique_ptr<Workload> CreateTracerCmdWorkload(const std::vector<std::string>& args);

  std::unique_ptr<Workload> tracing_workload_;
};

bool SimpleperfTracer::GetAllEvents(std::vector<std::pair<std::string, uint64_t>>* event_with_ids) {
  TemporaryFile tmpfile;
  std::unique_ptr<Workload> child_process =
      CreateTracerCmdWorkload({"--list-events", "-o", tmpfile.path});
  int status;
  if (child_process == nullptr || !child_process->Join(0, &status)) {
    return false;
  }
  if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
    LOG(ERROR) << "failed to run tracer to list events, status = " << status;
    return false;
  }
  std::string content;
  if (!ReadFile(tmpfile.path, &content)) {
    return false;
  }
  std::vector<std::string> lines = android::base::Split(content, "\n");
  event_with_ids->clear();
  for (auto& line : lines) {
    size_t sep = line.find(' ');
    if (sep != std::string::npos && sep > 0 && sep + 1 < line.size()) {
      uint64_t id;
      if (android::base::ParseUint(line.substr(sep + 1), &id)) {
        event_with_ids->push_back(std::make_pair(line.substr(0, sep), id));
      }
    }
  }
  return true;
}

bool SimpleperfTracer::GetEventFormats(const std::vector<std::string>& events,
                                       std::vector<std::string>* formats) {
  TemporaryFile tmpfile;
  std::string event_arg = android::base::Join(events, ',');
  std::unique_ptr<Workload> child_process =
      CreateTracerCmdWorkload({"--dump-events", event_arg, "-o", tmpfile.path});
  int status;
  if (child_process == nullptr || !child_process->Join(&status)) {
    return false;
  }
  if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
    LOG(ERROR) << "failed to run tracer to dump events, status = " << status;
    return false;
  }
  std::string content;
  if (!ReadFile(tmpfile.path, &content)) {
    return false;
  }
  size_t i = 0;
  size_t content_pos = 0;
  std::vector<size_t> pos_v;
  for (; i < events.size() && content_pos < content.size(); ++i) {
    std::string s = "name: " + events[i].substr(events[i].find(':') + 1);
    size_t pos = content.find(s, content_pos);
    if (pos == std::string::npos || (!pos_v.empty() && pos_v.back() >= pos)) {
      break;
    }
    pos_v.push_back(pos);
  }
  if (i != events.size()) {
    LOG(ERROR) << "wrong event format output";
    return false;
  }
  pos_v.push_back(content.size());
  formats->clear();
  for (size_t i = 0; i < pos_v.size(); ++i) {
    formats->push_back(content.substr(pos_v[i], pos_v[i+1] - pos_v[i]));
  }
  return true;
}

bool SimpleperfTracer::StartTracing(const std::vector<std::string>& events,
                                    const std::string& clock,
                                    const std::string& output_filename) {
  if (tracing_workload_ != nullptr) {
    LOG(ERROR) << "a tracing workload already exists.";
    return false;
  }
  std::string event_arg = android::base::Join(events, ',');
  tracing_workload_ = CreateTracerCmdWorkload({"--trace-events", event_arg, "-o", output_filename});
  return tracing_workload_ != nullptr;
}

bool SimpleperfTracer::StopTracing() {
  if (tracing_workload_ == nullptr) {
    LOG(ERROR) << "tracing workload doesn't exist.";
    return false;
  }
  tracing_workload_->SendSignal(SIGINT);
  int status;
  if (!tracing_workload_->Join(SIGINT, &status)) {
    return false;
  }
  tracing_workload_ = nullptr;
  if (!(WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)) {
    LOG(ERROR) << "tracing workload exit with unexpected status " << status;
    return false;
  }
  return true;
}

std::unique_ptr<Workload> SimpleperfTracer::CreateTracerCmdWorkload(
    const std::vector<std::string>& args) {
  auto child_function = [&]() {
    std::unique_ptr<Command> tracer_cmd = CreateCommandInstance("tracer");
    CHECK(tracer_cmd != nullptr);
    android::base::quick_exit(tracer_cmd->Run(args) ? 0 : 1);
  };
  std::unique_ptr<Workload> result = Workload::CreateWorkload(child_function);
  if (result == nullptr || !result->Start()) {
    return nullptr;
  }
  return result;
}

std::unique_ptr<Tracer> Tracer::CreateInstance() {
  if (IsRoot()) {
    return std::unique_ptr<Tracer>(new SimpleperfTracer());
  }
  return nullptr;
}

bool GetTracingData(const std::vector<const EventType*>& event_types,
                    std::vector<char>* data) {
  std::unique_ptr<Tracer> tracer = Tracer::CreateInstance();
  if (tracer == nullptr) {
    LOG(ERROR) << "No tracer is available";
    return false;
  }
  std::vector<std::string> events;
  for (auto& type : event_types) {
    events.push_back(type->name);
  }
  std::vector<std::string> formats;
  if (!tracer->GetEventFormats(events, &formats)) {
    return false;
  }
  TracingFile tracing_file;
  for (size_t i = 0; i < events.size(); ++i) {
    tracing_file.AddEventFormat(events[i], formats[i]);
  }
  *data = tracing_file.BinaryFormat();
  return true;
}

#endif   // defined(__linux__)
