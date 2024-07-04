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

#include "record_file.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>

#include "dso.h"
#include "event_attr.h"
#include "perf_event.h"
#include "record.h"
#include "system/extras/simpleperf/record_file.pb.h"
#include "utils.h"

namespace simpleperf {

using namespace PerfFileFormat;

std::unique_ptr<RecordFileWriter> RecordFileWriter::CreateInstance(const std::string& filename) {
  // Remove old perf.data to avoid file ownership problems.
  std::string err;
  if (!android::base::RemoveFileIfExists(filename, &err)) {
    LOG(ERROR) << "failed to remove file " << filename << ": " << err;
    return nullptr;
  }
  FILE* fp = fopen(filename.c_str(), "web+");
  if (fp == nullptr) {
    PLOG(ERROR) << "failed to open record file '" << filename << "'";
    return nullptr;
  }

  return std::unique_ptr<RecordFileWriter>(new RecordFileWriter(filename, fp, true));
}

RecordFileWriter::RecordFileWriter(const std::string& filename, FILE* fp, bool own_fp)
    : filename_(filename),
      record_fp_(fp),
      own_fp_(own_fp),
      attr_section_offset_(0),
      attr_section_size_(0),
      data_section_offset_(0),
      data_section_size_(0),
      feature_section_offset_(0),
      feature_count_(0) {}

RecordFileWriter::~RecordFileWriter() {
  if (record_fp_ != nullptr && own_fp_) {
    fclose(record_fp_);
    unlink(filename_.c_str());
  }
}

bool RecordFileWriter::WriteAttrSection(const EventAttrIds& attr_ids) {
  if (attr_ids.empty()) {
    return false;
  }

  // Skip file header part.
  if (fseek(record_fp_, sizeof(FileHeader), SEEK_SET) == -1) {
    return false;
  }

  // Write id section.
  uint64_t id_section_offset;
  if (!GetFilePos(&id_section_offset)) {
    return false;
  }
  for (auto& attr_id : attr_ids) {
    if (!Write(attr_id.ids.data(), attr_id.ids.size() * sizeof(uint64_t))) {
      return false;
    }
  }

  // Write attr section.
  uint64_t attr_section_offset;
  if (!GetFilePos(&attr_section_offset)) {
    return false;
  }
  for (auto& attr_id : attr_ids) {
    FileAttr file_attr;
    file_attr.attr = attr_id.attr;
    file_attr.ids.offset = id_section_offset;
    file_attr.ids.size = attr_id.ids.size() * sizeof(uint64_t);
    id_section_offset += file_attr.ids.size;
    if (!Write(&file_attr, sizeof(file_attr))) {
      return false;
    }
  }

  uint64_t data_section_offset;
  if (!GetFilePos(&data_section_offset)) {
    return false;
  }

  attr_section_offset_ = attr_section_offset;
  attr_section_size_ = data_section_offset - attr_section_offset;
  data_section_offset_ = data_section_offset;

  // Save event_attr for use when reading records.
  event_attr_ = attr_ids[0].attr;
  return true;
}

bool RecordFileWriter::WriteRecord(const Record& record) {
  // linux-tools-perf only accepts records with size <= 65535 bytes. To make
  // perf.data generated by simpleperf be able to be parsed by linux-tools-perf,
  // Split simpleperf custom records which are > 65535 into a bunch of
  // RECORD_SPLIT records, followed by a RECORD_SPLIT_END record.
  constexpr uint32_t RECORD_SIZE_LIMIT = 65535;
  if (record.size() <= RECORD_SIZE_LIMIT) {
    bool result = WriteData(record.Binary(), record.size());
    if (result && record.type() == PERF_RECORD_AUXTRACE) {
      auto auxtrace = static_cast<const AuxTraceRecord*>(&record);
      result = WriteData(auxtrace->location.addr, auxtrace->data->aux_size);
    }
    return result;
  }
  CHECK_GT(record.type(), SIMPLE_PERF_RECORD_TYPE_START);
  const char* p = record.Binary();
  uint32_t left_bytes = static_cast<uint32_t>(record.size());
  RecordHeader header;
  header.type = SIMPLE_PERF_RECORD_SPLIT;
  char header_buf[Record::header_size()];
  char* header_p;
  while (left_bytes > 0) {
    uint32_t bytes_to_write = std::min(RECORD_SIZE_LIMIT - Record::header_size(), left_bytes);
    header.size = bytes_to_write + Record::header_size();
    header_p = header_buf;
    header.MoveToBinaryFormat(header_p);
    if (!WriteData(header_buf, Record::header_size())) {
      return false;
    }
    if (!WriteData(p, bytes_to_write)) {
      return false;
    }
    p += bytes_to_write;
    left_bytes -= bytes_to_write;
  }
  header.type = SIMPLE_PERF_RECORD_SPLIT_END;
  header.size = Record::header_size();
  header_p = header_buf;
  header.MoveToBinaryFormat(header_p);
  return WriteData(header_buf, Record::header_size());
}

bool RecordFileWriter::WriteData(const void* buf, size_t len) {
  if (!Write(buf, len)) {
    return false;
  }
  data_section_size_ += len;
  return true;
}

bool RecordFileWriter::Write(const void* buf, size_t len) {
  if (len != 0u && fwrite(buf, len, 1, record_fp_) != 1) {
    PLOG(ERROR) << "failed to write to record file '" << filename_ << "'";
    return false;
  }
  return true;
}

bool RecordFileWriter::Read(void* buf, size_t len) {
  if (len != 0u && fread(buf, len, 1, record_fp_) != 1) {
    PLOG(ERROR) << "failed to read record file '" << filename_ << "'";
    return false;
  }
  return true;
}

bool RecordFileWriter::ReadDataSection(const std::function<void(const Record*)>& callback) {
  if (fseek(record_fp_, data_section_offset_, SEEK_SET) == -1) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  std::vector<char> record_buf(512);
  uint64_t read_pos = 0;
  while (read_pos < data_section_size_) {
    if (!Read(record_buf.data(), Record::header_size())) {
      return false;
    }
    RecordHeader header;
    if (!header.Parse(record_buf.data())) {
      return false;
    }
    if (record_buf.size() < header.size) {
      record_buf.resize(header.size);
    }
    if (!Read(record_buf.data() + Record::header_size(), header.size - Record::header_size())) {
      return false;
    }
    read_pos += header.size;
    std::unique_ptr<Record> r = ReadRecordFromBuffer(event_attr_, header.type, record_buf.data(),
                                                     record_buf.data() + header.size);
    CHECK(r);
    if (r->type() == PERF_RECORD_AUXTRACE) {
      auto auxtrace = static_cast<AuxTraceRecord*>(r.get());
      auxtrace->location.file_offset = data_section_offset_ + read_pos;
      if (fseek(record_fp_, auxtrace->data->aux_size, SEEK_CUR) != 0) {
        PLOG(ERROR) << "fseek() failed";
        return false;
      }
      read_pos += auxtrace->data->aux_size;
    }
    callback(r.get());
  }
  return true;
}

bool RecordFileWriter::GetFilePos(uint64_t* file_pos) {
  off_t offset = ftello(record_fp_);
  if (offset == -1) {
    PLOG(ERROR) << "ftello() failed";
    return false;
  }
  *file_pos = static_cast<uint64_t>(offset);
  return true;
}

bool RecordFileWriter::BeginWriteFeatures(size_t feature_count) {
  feature_section_offset_ = data_section_offset_ + data_section_size_;
  feature_count_ = feature_count;
  uint64_t feature_header_size = feature_count * sizeof(SectionDesc);

  // Reserve enough space in the record file for the feature header.
  std::vector<unsigned char> zero_data(feature_header_size);
  if (fseek(record_fp_, feature_section_offset_, SEEK_SET) == -1) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  return Write(zero_data.data(), zero_data.size());
}

bool RecordFileWriter::WriteBuildIdFeature(const std::vector<BuildIdRecord>& build_id_records) {
  if (!WriteFeatureBegin(FEAT_BUILD_ID)) {
    return false;
  }
  for (auto& record : build_id_records) {
    if (!Write(record.Binary(), record.size())) {
      return false;
    }
  }
  return WriteFeatureEnd(FEAT_BUILD_ID);
}

bool RecordFileWriter::WriteStringWithLength(const std::string& s) {
  uint32_t len = static_cast<uint32_t>(Align(s.size() + 1, 64));
  if (!Write(&len, sizeof(len))) {
    return false;
  }
  if (!Write(&s[0], s.size() + 1)) {
    return false;
  }
  size_t pad_size = Align(s.size() + 1, 64) - s.size() - 1;
  if (pad_size > 0u) {
    char align_buf[pad_size];
    memset(align_buf, '\0', pad_size);
    if (!Write(align_buf, pad_size)) {
      return false;
    }
  }
  return true;
}

bool RecordFileWriter::WriteFeatureString(int feature, const std::string& s) {
  if (!WriteFeatureBegin(feature)) {
    return false;
  }
  if (!WriteStringWithLength(s)) {
    return false;
  }
  return WriteFeatureEnd(feature);
}

bool RecordFileWriter::WriteCmdlineFeature(const std::vector<std::string>& cmdline) {
  if (!WriteFeatureBegin(FEAT_CMDLINE)) {
    return false;
  }
  uint32_t arg_count = cmdline.size();
  if (!Write(&arg_count, sizeof(arg_count))) {
    return false;
  }
  for (auto& arg : cmdline) {
    if (!WriteStringWithLength(arg)) {
      return false;
    }
  }
  return WriteFeatureEnd(FEAT_CMDLINE);
}

bool RecordFileWriter::WriteBranchStackFeature() {
  if (!WriteFeatureBegin(FEAT_BRANCH_STACK)) {
    return false;
  }
  return WriteFeatureEnd(FEAT_BRANCH_STACK);
}

bool RecordFileWriter::WriteAuxTraceFeature(const std::vector<uint64_t>& auxtrace_offset) {
  std::vector<uint64_t> data;
  for (auto offset : auxtrace_offset) {
    data.push_back(offset);
    data.push_back(AuxTraceRecord::Size());
  }
  return WriteFeature(FEAT_AUXTRACE, reinterpret_cast<char*>(data.data()),
                      data.size() * sizeof(uint64_t));
}

bool RecordFileWriter::WriteFileFeatures(const std::vector<Dso*>& dsos) {
  for (Dso* dso : dsos) {
    // Always want to dump dex file offsets for DSO_DEX_FILE type.
    if (!dso->HasDumpId() && dso->type() != DSO_DEX_FILE) {
      continue;
    }
    FileFeature file;
    file.path = dso->Path();
    file.type = dso->type();
    dso->GetMinExecutableVaddr(&file.min_vaddr, &file.file_offset_of_min_vaddr);

    // Dumping all symbols in hit files takes too much space, so only dump
    // needed symbols.
    const std::vector<Symbol>& symbols = dso->GetSymbols();
    for (const auto& sym : symbols) {
      if (sym.HasDumpId()) {
        file.symbol_ptrs.emplace_back(&sym);
      }
    }
    std::sort(file.symbol_ptrs.begin(), file.symbol_ptrs.end(), Symbol::CompareByAddr);

    if (const auto dex_file_offsets = dso->DexFileOffsets(); dex_file_offsets != nullptr) {
      file.dex_file_offsets = *dex_file_offsets;
    }
    if (!WriteFileFeature(file)) {
      return false;
    }
  }
  return true;
}

bool RecordFileWriter::WriteFileFeature(const FileFeature& file) {
  proto::FileFeature proto_file;
  proto_file.set_path(file.path);
  proto_file.set_type(static_cast<uint32_t>(file.type));
  proto_file.set_min_vaddr(file.min_vaddr);
  auto write_symbol = [&](const Symbol& symbol) {
    proto::FileFeature::Symbol* proto_symbol = proto_file.add_symbol();
    proto_symbol->set_vaddr(symbol.addr);
    proto_symbol->set_len(symbol.len);
    // Store demangled names for rust symbols. Because simpleperf on windows host doesn't know
    // how to demangle them.
    if (strncmp(symbol.Name(), "_R", 2) == 0) {
      proto_symbol->set_name(symbol.DemangledName());
    } else {
      proto_symbol->set_name(symbol.Name());
    }
  };
  for (const Symbol& symbol : file.symbols) {
    write_symbol(symbol);
  }
  for (const Symbol* symbol_ptr : file.symbol_ptrs) {
    write_symbol(*symbol_ptr);
  }
  if (file.type == DSO_DEX_FILE) {
    proto::FileFeature::DexFile* proto_dex_file = proto_file.mutable_dex_file();
    proto_dex_file->mutable_dex_file_offset()->Add(file.dex_file_offsets.begin(),
                                                   file.dex_file_offsets.end());
  } else if (file.type == DSO_ELF_FILE) {
    proto::FileFeature::ElfFile* proto_elf_file = proto_file.mutable_elf_file();
    proto_elf_file->set_file_offset_of_min_vaddr(file.file_offset_of_min_vaddr);
  } else if (file.type == DSO_KERNEL_MODULE) {
    proto::FileFeature::KernelModule* proto_kernel_module = proto_file.mutable_kernel_module();
    proto_kernel_module->set_memory_offset_of_min_vaddr(file.file_offset_of_min_vaddr);
  }
  std::string s;
  if (!proto_file.SerializeToString(&s)) {
    LOG(ERROR) << "SerializeToString() failed";
    return false;
  }
  uint32_t msg_size = s.size();
  return WriteFeatureBegin(FEAT_FILE2) && Write(&msg_size, sizeof(uint32_t)) &&
         Write(s.data(), s.size()) && WriteFeatureEnd(FEAT_FILE2);
}

bool RecordFileWriter::WriteMetaInfoFeature(
    const std::unordered_map<std::string, std::string>& info_map) {
  uint32_t size = 0u;
  for (auto& pair : info_map) {
    size += pair.first.size() + 1;
    size += pair.second.size() + 1;
  }
  std::vector<char> buf(size);
  char* p = buf.data();
  for (auto& pair : info_map) {
    MoveToBinaryFormat(pair.first.c_str(), pair.first.size() + 1, p);
    MoveToBinaryFormat(pair.second.c_str(), pair.second.size() + 1, p);
  }
  return WriteFeature(FEAT_META_INFO, buf.data(), buf.size());
}

bool RecordFileWriter::WriteDebugUnwindFeature(const DebugUnwindFeature& debug_unwind) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  proto::DebugUnwindFeature proto_debug_unwind;
  for (auto& file : debug_unwind) {
    auto proto_file = proto_debug_unwind.add_file();
    proto_file->set_path(file.path);
    proto_file->set_size(file.size);
  }
  std::string s;
  if (!proto_debug_unwind.SerializeToString(&s)) {
    LOG(ERROR) << "SerializeToString() failed";
    return false;
  }
  return WriteFeature(FEAT_DEBUG_UNWIND, s.data(), s.size());
}

bool RecordFileWriter::WriteInitMapFeature(const char* data, size_t size) {
  return WriteFeatureBegin(FEAT_INIT_MAP) && Write(data, size) && WriteFeatureEnd(FEAT_INIT_MAP);
}

bool RecordFileWriter::WriteFeature(int feature, const char* data, size_t size) {
  return WriteFeatureBegin(feature) && Write(data, size) && WriteFeatureEnd(feature);
}

bool RecordFileWriter::WriteFeatureBegin(int feature) {
  auto it = features_.find(feature);
  if (it == features_.end()) {
    CHECK_LT(features_.size(), feature_count_);
    auto& sec = features_[feature];
    if (!GetFilePos(&sec.offset)) {
      return false;
    }
    // Ensure each feature section starts at a 8-byte aligned location.
    // This is not needed for the current RecordFileReader implementation, but is helpful if we
    // switch to a mapped file reader. So it's nice to have. But it's nice to have.
    if (sec.offset & 7) {
      std::vector<char> zero_data(8 - (sec.offset & 7), '\0');
      if (!Write(zero_data.data(), zero_data.size())) {
        return false;
      }
      sec.offset += zero_data.size();
    }
    sec.size = 0;
  }
  return true;
}

bool RecordFileWriter::WriteFeatureEnd(int feature) {
  auto it = features_.find(feature);
  if (it == features_.end()) {
    return false;
  }
  uint64_t offset;
  if (!GetFilePos(&offset)) {
    return false;
  }
  it->second.size = offset - it->second.offset;
  return true;
}

bool RecordFileWriter::EndWriteFeatures() {
  // Used features (features_.size()) should be <= allocated feature space.
  CHECK_LE(features_.size(), feature_count_);
  if (fseek(record_fp_, feature_section_offset_, SEEK_SET) == -1) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  for (const auto& pair : features_) {
    if (!Write(&pair.second, sizeof(SectionDesc))) {
      return false;
    }
  }
  return true;
}

bool RecordFileWriter::WriteFileHeader() {
  FileHeader header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, PERF_MAGIC, sizeof(header.magic));
  header.header_size = sizeof(header);
  header.attr_size = sizeof(FileAttr);
  header.attrs.offset = attr_section_offset_;
  header.attrs.size = attr_section_size_;
  header.data.offset = data_section_offset_;
  header.data.size = data_section_size_;
  for (const auto& pair : features_) {
    int i = pair.first / 8;
    int j = pair.first % 8;
    header.features[i] |= (1 << j);
  }

  if (fseek(record_fp_, 0, SEEK_SET) == -1) {
    return false;
  }
  if (!Write(&header, sizeof(header))) {
    return false;
  }
  return true;
}

bool RecordFileWriter::Close() {
  CHECK(record_fp_ != nullptr);
  bool result = true;

  // Write file header. We gather enough information to write file header only after
  // writing data section and feature section.
  if (!WriteFileHeader()) {
    result = false;
  }

  if (own_fp_ && fclose(record_fp_) != 0) {
    PLOG(ERROR) << "failed to close record file '" << filename_ << "'";
    result = false;
  }
  record_fp_ = nullptr;
  return result;
}

}  // namespace simpleperf
