/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <map>
#include <set>

#include "command.h"
#include "environment.h"
#include "event_attr.h"
#include "record.h"
#include "record_file_format.h"

class ReadRecordCommand : public Command {
 public:
  ReadRecordCommand()
      : Command("readrecord",
                "read record file and print it out",
                "Usage: simpleperf readrecord [record_file]\n"
                "    Read record file dumped by record command\n"
                "perf.data is used as filename by default\n") {
    option_filename = "perf.data";
    fp = nullptr;
  }

  ~ReadRecordCommand() {
    if (fp != nullptr) {
      fclose(fp);
    }
  }

  bool RunCommand(std::vector<std::string>& args) override;

 private:
  bool ParseOptions(const std::vector<std::string>& args, std::vector<std::string>& non_option_args);
  bool ReadHeader();
  bool ReadAttrs();
  bool ReadData();
  bool ReadFeatures();
  void PrintFeatureBitmap();
  void PrintAttr(int attr_index, const file_attr& file_attr, const EventAttr& attr,
                 const std::vector<uint64_t>& ids);
  bool CheckSampleHit();

 private:
  std::string option_filename;
  FILE* fp;
  file_header header;
  std::vector<int> features;
  std::vector<file_attr> file_attrs;
  std::vector<EventAttr> attrs;

  std::vector<std::unique_ptr<Record>> records;
  std::vector<std::unique_ptr<Record>> build_id_records;  // Collected in build_id feature section.
};

bool ReadRecordCommand::RunCommand(std::vector<std::string>& args) {
  std::vector<std::string> non_option_args;
  if (!ParseOptions(args, non_option_args)) {
    return false;
  }

  fp = fopen(option_filename.c_str(), "rb");
  if (fp == nullptr) {
    fprintf(stderr, "fopen %s failed: %s\n", option_filename.c_str(), strerror(errno));
    return false;
  }

  if (!ReadHeader()) {
    return false;
  }

  if (!ReadAttrs()) {
    return false;
  }

  if (!ReadData()) {
    return false;
  }

  if (!ReadFeatures()) {
    return false;
  }

  CheckSampleHit();

  return true;
}

bool ReadRecordCommand::ParseOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>& non_option_args) {
  if (args.size() != 0) {
    option_filename = args[0];
  }
  non_option_args.clear();
  return true;
}

bool ReadRecordCommand::ReadHeader() {
  if (fread(&header, sizeof(header), 1, fp) != 1) {
    perror("fread");
    return false;
  }
  printf("magic: ");
  for (int i = 0; i < 8; ++i) {
    printf("%c", header.magic[i]);
  }
  printf("\n");
  printf("header_size: %" PRId64 "\n", header.header_size);
  if (header.header_size != sizeof(header)) {
    printf("  Our expected header_size is %zu\n", sizeof(header));
  }
  printf("attr_size: %" PRId64 "\n", header.attr_size);
  if (header.attr_size != sizeof(file_attr)) {
    printf("  Our expected attr_size is %zu\n", sizeof(file_attr));
  }
  printf("attrs[file section]: offset %" PRId64 ", size %" PRId64 "\n",
         header.attrs.offset, header.attrs.size);
  printf("data[file_section]: offset %" PRId64 ", size %" PRId64 "\n",
         header.data.offset, header.data.size);
  printf("event_types[file_section]: offset %" PRId64 ", size %" PRId64 "\n",
         header.event_types.offset, header.event_types.size);

  features.clear();
  for (size_t i = 0; i < FEAT_MAX_NUM; ++i) {
    size_t j = i / 8;
    size_t k = i % 8;
    if ((header.adds_features[j] & (1 << k)) != 0) {
      features.push_back(i);
    }
  }

  PrintFeatureBitmap();

  return true;
}

static const char* feature_names[FEAT_MAX_NUM];

__attribute__((constructor)) static void init_feature_names() {
  for (size_t i = 0; i < FEAT_MAX_NUM; ++i) {
    feature_names[i] = "unknown";
  }

  feature_names[FEAT_TRACING_DATA] = "tracing_data";
  feature_names[FEAT_BUILD_ID] = "build_id";
  feature_names[FEAT_HOSTNAME] = "hostname";
  feature_names[FEAT_OSRELEASE] = "osrelease";
  feature_names[FEAT_VERSION] = "version";
  feature_names[FEAT_ARCH] = "arch";
  feature_names[FEAT_NRCPUS] = "nrcpus";
  feature_names[FEAT_CPUDESC] = "cpudesc";
  feature_names[FEAT_CPUID] = "cpuid";
  feature_names[FEAT_TOTAL_MEM] = "total_mem";
  feature_names[FEAT_CMDLINE] = "cmdline";
  feature_names[FEAT_EVENT_DESC] = "event_desc";
  feature_names[FEAT_CPU_TOPOLOGY] = "cpu_topology";
  feature_names[FEAT_NUMA_TOPOLOGY] = "numa_topology";
  feature_names[FEAT_BRANCH_STACK] = "branch_stack";
  feature_names[FEAT_PMU_MAPPINGS] = "pmu_mapping";
  feature_names[FEAT_GROUP_DESC] = "group_desc";
}

void ReadRecordCommand::PrintFeatureBitmap() {
  for (auto feature : features) {
    printf("additional feature: %s\n", feature_names[feature]);
  }
}

bool ReadRecordCommand::ReadAttrs() {
  if (header.attr_size != sizeof(file_attr)) {
    fprintf(stderr, "header.attr_size %" PRId64 " doesn't match expected size %zu\n",
            header.attr_size, sizeof(file_attr));
    return false;
  }
  if (fseek(fp, header.attrs.offset, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }

  if (header.attrs.size % header.attr_size != 0) {
    fprintf(stderr, "Not integer number of attrs.\n");
    return false;
  }

  size_t attr_count = header.attrs.size / header.attr_size;
  file_attrs.resize(attr_count);

  if (fread(file_attrs.data(), header.attrs.size, 1, fp) != 1) {
    fprintf(stderr, "fread failed: %s\n", strerror(errno));
    return false;
  }

  attrs.clear();
  for (size_t i = 0; i < attr_count; ++i) {
    attrs.push_back(EventAttr(&file_attrs[i].attr));
  }

  std::vector<std::vector<uint64_t>> ids_for_attrs(attr_count);
  for (size_t i = 0; i < attr_count; ++i) {
    file_section section = file_attrs[i].ids;
    if (section.size == 0) {
      continue;
    }
    if (fseek(fp, section.offset, SEEK_SET) != 0) {
      perror("fseek");
      return false;
    }
    ids_for_attrs[i].resize(section.size / sizeof(uint64_t));
    int ret = fread(ids_for_attrs[i].data(), section.size, 1, fp);
    if (ret != 1) {
      fprintf(stderr, "fread failed: ret = %d, errno = %s\n", ret, strerror(ferror(fp)));
      return false;
    }
  }

  for (size_t i = 0; i < attr_count; ++i) {
    PrintAttr(i + 1, file_attrs[i], attrs[i], ids_for_attrs[i]);
  }

  return true;
}

void ReadRecordCommand::PrintAttr(int attr_index, const file_attr& file_attr,
                                  const EventAttr& attr,
                                  const std::vector<uint64_t>& ids) {
  printf("file_attr %d:\n", attr_index);
  attr.Print(2);
  printf("  ids[file_section]: offset %" PRId64 ", size %" PRId64 "\n",
         file_attr.ids.offset, file_attr.ids.size);
  if (ids.size() != 0) {
    printf("  ids:");
    for (auto id : ids) {
      printf(" %" PRId64, id);
    }
    printf("\n");
  }
}

bool ReadRecordCommand::ReadData() {
  long data_offset = header.data.offset;
  size_t data_size = header.data.size;

  if (data_size == 0) {
    printf("no data\n");
    return true;
  }

  if (fseek(fp, data_offset, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }

  records.clear();

  size_t last_data_size = data_size;
  while (last_data_size != 0) {
    if (last_data_size < sizeof(perf_event_header)) {
      fprintf(stderr, "last_data_size(%zu) is less than the size of perf_event_header\n", last_data_size);
      return false;
    }
    perf_event_header record_header;
    if (fread(&record_header, sizeof(record_header), 1, fp) != 1) {
      perror("fread");
      return false;
    }
    size_t record_size = record_header.size;
    char* buf = new char[record_size];
    memcpy(buf, &record_header, sizeof(record_header));

    if (record_size > sizeof(record_header)) {
      if (fread(buf + sizeof(record_header), record_size - sizeof(record_header), 1, fp) != 1) {
        perror("fread");
        delete [] buf;
        return false;
      }
    }
    auto record = BuildRecordOnBuffer(buf, record_size, &attrs[0]);
    record->Print();
    last_data_size -= record_size;

    records.push_back(std::move(record));
  }
  return true;
}

bool ReadRecordCommand::ReadFeatures() {
  long feature_offset = header.data.offset + header.data.size;
  if (fseek(fp, feature_offset, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }

  std::vector<file_section> sections(features.size());
  if (fread(&sections[0], sections.size() * sizeof(file_section), 1, fp) != 1) {
    perror("fread");
    return false;
  }

  build_id_records.clear();

  for (size_t i = 0; i < sections.size(); ++i) {
    printf("feature %s(%d): section offset 0x%" PRIx64 ", size 0x%" PRIx64 "\n",
           feature_names[features[i]], features[i], sections[i].offset, sections[i].size);
    size_t buf_size = sections[i].size;
    std::vector<char> buf(buf_size);
    if (fseek(fp, sections[i].offset, SEEK_SET) != 0) {
      perror("fseek");
      return false;
    }
    if (fread(buf.data(), buf_size, 1, fp) != 1) {
      perror("fread");
      return false;
    }

    if (features[i] == FEAT_BUILD_ID) {
      const char* p = buf.data();
      const char* end_p = buf.data() + buf.size();
      while (p != end_p) {
        auto record = BuildRecordBuildId(p, end_p - p, &p);
        if (record == nullptr) {
          fprintf(stderr, "Identify broken build id record.\n");
          break;
        }
        record->Print();
        build_id_records.push_back(std::move(record));
      }
    }
  }
  return true;
}

struct HitMmap {
  uint64_t addr;
  uint64_t len;
  const char* filename;
  bool hit;
};

bool ReadRecordCommand::CheckSampleHit() {
  // Check if we only dump build_id for dsos that are hit in sample records.
  bool result = true;

  // 1. Build a mmap array for kernel and each process, mark sample hit flag.
  std::vector<HitMmap> kernel_mmap;
  std::map<pid_t, std::vector<HitMmap>> process_mmaps;

  for (auto& record : records) {
    if (record->Type() == PERF_RECORD_MMAP) {
      RecordMmap* mmap_record = static_cast<RecordMmap*>(record.get());
      if (mmap_record->InKernel()) {
        HitMmap hitmmap{mmap_record->Addr(), mmap_record->Len(), mmap_record->Filename(), false};
        kernel_mmap.push_back(hitmmap);
      } else {
        pid_t pid = mmap_record->Pid();
        auto it = process_mmaps.find(pid);
        if (it == process_mmaps.end()) {
          process_mmaps[pid] = std::vector<HitMmap>();
        }
        auto& process_mmap = process_mmaps[pid];
        HitMmap hitmmap{mmap_record->Addr(), mmap_record->Len(), mmap_record->Filename(), false};
        process_mmap.push_back(hitmmap);
      }
    } else if (record->Type() == PERF_RECORD_SAMPLE) {
      RecordSample* sample_record = static_cast<RecordSample*>(record.get());
      if (sample_record->InKernel()) {
        // Loop from back to front, because new HitMmap is inserted at the end of the vector, and
        // we always want to match the newer one.
        for (auto it = kernel_mmap.rbegin(); it != kernel_mmap.rend(); ++it) {
          auto& hit_mmap = *it;
          if (sample_record->Ip() >= hit_mmap.addr &&
              sample_record->Ip() <= hit_mmap.addr + hit_mmap.len) {
            hit_mmap.hit = true;
            break;
          }
        }
      } else {
        auto it = process_mmaps.find(sample_record->Pid());
        if (it == process_mmaps.end()) {
          continue;
        }
        auto& process_mmap = it->second;
        for (auto it = process_mmap.rbegin(); it != process_mmap.rend(); ++it) {
          auto& hit_mmap = *it;
          if (sample_record->Ip() >= hit_mmap.addr &&
              sample_record->Ip() <= hit_mmap.addr + hit_mmap.len) {
            hit_mmap.hit = true;
            break;
          }
        }
      }
    }
  }

  // 2. build hit_mmap_set.
  std::set<std::string> hit_mmap_set;
  for (auto& hit_mmap : kernel_mmap) {
    if (hit_mmap.hit) {
      if (strcmp(hit_mmap.filename, DEFAULT_KERNEL_MMAP_NAME) == 0) {
        hit_mmap_set.insert(DEFAULT_KERNEL_FILENAME_FOR_BUILD_ID);
      } else {
        hit_mmap_set.insert(hit_mmap.filename);
      }
    }
  }
  for (auto it = process_mmaps.begin(); it != process_mmaps.end(); ++it) {
    for (auto& hit_mmap : it->second) {
      if (hit_mmap.hit && strcmp(hit_mmap.filename, DEFAULT_EXEC_NAME_FOR_THREAD_MMAP) != 0) {
        hit_mmap_set.insert(hit_mmap.filename);
      }
    }
  }

  // 2. check if build_id table matches hit_mmap_set.
  for (auto& record : build_id_records) {
    RecordBuildId* buildid_record = static_cast<RecordBuildId*>(record.get());
    std::string s = buildid_record->Filename();
    auto it = hit_mmap_set.find(s);
    if (it == hit_mmap_set.end()) {
      printf("extra build_id_record: %s\n", s.c_str());
      result = false;
    } else {
      hit_mmap_set.erase(it);
    }
  }
  for (auto it = hit_mmap_set.begin(); it != hit_mmap_set.end(); ++it) {
    printf("extra mmap hit record: %s\n", (*it).c_str());
    result = false;
  }
  return result;
}

ReadRecordCommand readrecord_cmd;
