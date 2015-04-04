#include "record_file.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "command.h"
#include "event.h"
#include "event_attr.h"
#include "event_fd.h"
#include "record.h"

enum {
  FEAT_RESERVED = 0,
  FEAT_FIRST_FEATURE = 1,
  FEAT_TARCING_DATA = 1,
	FEAT_BUILD_ID,
	FEAT_HOSTNAME,
	FEAT_OSRELEASE,
	FEAT_VERSION,
	FEAT_ARCH,
	FEAT_NRCPUS,
	FEAT_CPUDESC,
	FEAT_CPUID,
	FEAT_TOTAL_MEM,
	FEAT_CMDLINE,
	FEAT_EVENT_DESC,
	FEAT_CPU_TOPOLOGY,
	FEAT_NUMA_TOPOLOGY,
	FEAT_BRANCH_STACK,
	FEAT_PMU_MAPPINGS,
	FEAT_GROUP_DESC,
	FEAT_LAST_FEATURE,
	FEAT_MAX_NUM	= 256,
};

struct file_section {
  uint64_t offset;
  uint64_t size;
};

const char* PERF_MAGIC = "PERFILE2";

struct file_header {
  char magic[8];
  uint64_t header_size;
  uint64_t attr_size;
  file_section attrs;
  file_section data;
  file_section event_types;
  uint64_t adds_features[FEAT_MAX_NUM / 64];
};

struct file_attr {
  perf_event_attr attr;
  file_section ids;
};

std::unique_ptr<RecordFile> RecordFile::CreateFile(const std::string& filename) {
  FILE* fp = fopen(filename.c_str(), "wb");
  if (fp == nullptr) {
    return std::unique_ptr<RecordFile>(nullptr);
  }
  return std::unique_ptr<RecordFile>(new RecordFile(filename, fp));
}

RecordFile::RecordFile(const std::string& filename, FILE* fp)
  : filename(filename), record_fp(fp), data_offset(0), data_size(0) {
}

RecordFile::~RecordFile() {
  if (record_fp != nullptr) {
    Close();
  }
}

bool RecordFile::WriteHeader(std::vector<std::unique_ptr<EventFd>>& event_fds) {
  fseek(record_fp, sizeof(file_header), SEEK_SET);

  std::vector<uint64_t> event_ids;

  for (auto& event_fd : event_fds) {
    uint64_t event_id;
    if (!event_fd->GetEventId(event_id)) {
      return false;
    }
    event_ids.push_back(event_id);
  }

  long ids_offset = ftell(record_fp);
  size_t ids_size = event_ids.size() * sizeof(uint64_t);
  if (!WriteOutput(event_ids.data(), ids_size)) {
    return false;
  }

  std::vector<file_attr> file_attrs;

  uint64_t id_offset = ids_offset;
  for (auto& event_fd : event_fds) {
    file_attr attr;
    attr.attr = *(event_fd->GetAttr().Attr());
    attr.ids.offset = id_offset;
    id_offset += sizeof(uint64_t);
    attr.ids.size = sizeof(uint64_t);
    file_attrs.push_back(attr);
  }

  long attrs_offset = ftell(record_fp);
  size_t attrs_size = file_attrs.size() * sizeof(file_attr);
  if (!WriteOutput(file_attrs.data(), attrs_size)) {
    return false;
  }

  data_offset = ftell(record_fp);

  file_header header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, PERF_MAGIC, sizeof(header.magic));
  header.header_size = sizeof(header);
  header.attr_size = sizeof(file_attr);
  header.attrs.offset = attrs_offset;
  header.attrs.size = attrs_size;
  header.data.offset = data_offset;
  header.data.size = data_size;

  if (fseek(record_fp, 0, SEEK_SET) != 0) {
    perror("fseek");
    return false;
  }
  if (!WriteOutput(&header, sizeof(header))) {
    return false;
  }
  fseek(record_fp, data_offset, SEEK_SET);

  return true;
}

bool RecordFile::WriteData(const void* buf, size_t len) {
  if (!WriteOutput(buf, len)) {
    return false;
  }
  data_size += len;
  return true;
}

bool RecordFile::WriteOutput(const void* buf, size_t len) {
  if (fwrite(buf, len, 1, record_fp) != 1) {
    fprintf(stderr, "RecordFile::WriteData for file %s fails: %s\n", filename.c_str(),
            strerror(errno));
    return false;
  }
  return true;
}

bool RecordFile::Close() {
  if (record_fp == nullptr) {
    return true;
  }

  if (fclose(record_fp) != 0) {
    fprintf(stderr, "RecordFile::Close for file %s fails: %s\n", filename.c_str(),
            strerror(errno));
    return false;
  }
  record_fp = nullptr;
  return true;
}

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
  void PrintAttr(int attr_index, const file_attr& file_attr, const EventAttr& attr, const std::vector<uint64_t>& ids);

 private:
  std::string option_filename;
  FILE* fp;
  file_header header;
  std::vector<file_attr> file_attrs;
  std::vector<EventAttr> attrs;
};

bool ReadRecordCommand::RunCommand(std::vector<std::string>& args) {
  std::vector<std::string> non_option_args;
  if (!ParseOptions(args, non_option_args)) {
    return false;
  }

  fp = fopen(option_filename.c_str(), "rb");
  if (fp == nullptr) {
    fprintf(stderr, "fopen %s fails: %s\n", option_filename.c_str(), strerror(errno));
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
  for (size_t i = 0; i < sizeof(header.adds_features) / sizeof(header.adds_features[0]); ++i) {
    printf("adds_features[%zu] = 0x%" PRIx64 "\n", i, header.adds_features[i]);
  }

  return true;
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
    fprintf(stderr, "fread fails: %s\n", strerror(errno));
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
      fprintf(stderr, "fread fails: ret = %d, errno = %s\n", ret, strerror(ferror(fp)));
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
      }
    }
    auto record = BuildRecordOnBuffer(buf, record_size, attrs[0]);
    record->Print();
  }
/*
  std::vector<unsigned char> data(data_size);
  if (fread(data.data(), data_size, 1, fp) != 1) {
    perror("fread");
    return false;
  }
  printf("data:\n");
  for (size_t i = 0; i < data_size; ++i) {
    printf("%02X ", data[i]);
    if (i % 16 == 15) {
      printf("\n");
    }
  }
  printf("\n");
*/
  return true;
}

ReadRecordCommand readrecord_cmd;
