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

#ifndef SIMPLE_PERF_TRACING_H_
#define SIMPLE_PERF_TRACING_H_

#include <memory>
#include <string>
#include <vector>

#include <android-base/logging.h>

#include "event_type.h"
#include "utils.h"

struct TracingValue {
  enum {
    TRACING_VALUE_UNKNOWN,
    TRACING_VALUE_UNSIGNED,
    TRACING_VALUE_SIGNED,
    TRACING_VALUE_STRING,
  } type;

  union {
    uint64_t unsigned_value;
    int64_t signed_value;
  };
  std::string string_value;

  std::string toString() const;
};

struct TracingField {
  std::string name;
  size_t offset;
  size_t elem_size;
  size_t elem_count;
  bool is_signed;

  bool ExtractValue(const char* data, size_t data_size, TracingValue* value) const;
};

struct TracingFieldPlace {
  uint32_t offset;
  uint32_t size;

  uint64_t ReadFromData(const char* raw_data) {
    return ConvertBytesToValue(raw_data + offset, size);
  }
  void WriteToData(char* raw_data, uint64_t value) {
    memcpy(raw_data + offset, &value, size);
  }
};

struct TracingFormat {
  std::string system_name;
  std::string name;
  uint64_t id;
  std::vector<TracingField> fields;

  void GetField(const std::string& name, TracingFieldPlace& place) const {
    const TracingField& field = GetField(name);
    place.offset = field.offset;
    place.size = field.elem_size;
  }

 private:
  const TracingField& GetField(const std::string& name) const {
    for (const auto& field : fields) {
      if (field.name == name) {
        return field;
      }
    }
    LOG(FATAL) << "Couldn't find field " << name << "in TracingFormat of "
               << this->name;
    return fields[0];
  }
};

class TracingFile;

class Tracing {
 public:
  explicit Tracing(const std::vector<char>& data);
  ~Tracing();
  void Dump(size_t indent);
  const TracingFormat* GetTracingFormatHavingId(uint64_t trace_event_id);
  std::string GetTracingEventNameHavingId(uint64_t trace_event_id);
  const std::string& GetKallsyms() const;
  uint32_t GetPageSize() const;

 private:
  TracingFile* tracing_file_;
  std::vector<TracingFormat> tracing_formats_;
};

class ScopedTracing {
 public:
  explicit ScopedTracing(Tracing* tracing) : saved_tracing_(current_tracing_.release()) {
    current_tracing_.reset(tracing);
  }

  ~ScopedTracing() {
    current_tracing_.reset(saved_tracing_.release());
  }

  static Tracing* GetCurrentTracing() {
    return current_tracing_.get();
  }

 private:
  std::unique_ptr<Tracing> saved_tracing_;
  static std::unique_ptr<Tracing> current_tracing_;
};


#if defined(__linux__)

class Tracer {
 public:
  static std::unique_ptr<Tracer> CreateInstance();  // Return nullptr if not available.
  virtual ~Tracer() {}

  virtual bool GetAllEvents(std::vector<std::pair<std::string, uint64_t>>* event_with_ids) = 0;
  virtual bool GetEventFormats(const std::vector<std::string>& events,
                               std::vector<std::string>* formats) = 0;
  virtual bool StartTracing(const std::vector<std::string>& events, const std::string& clock,
                            const std::string& output_filename) = 0;
  virtual bool StopTracing() = 0;

 protected:
  Tracer() {}

};

bool GetTracingData(const std::vector<const EventType*>& event_types, std::vector<char>* data);


#endif  // defined(__linux__)

#endif  // SIMPLE_PERF_TRACING_H_
