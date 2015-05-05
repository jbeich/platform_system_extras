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

#include <gtest/gtest.h>

#include "environment.h"
#include "event_attr.h"
#include "event_type.h"
#include "record.h"

class RecordTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const EventType* event_type = EventTypeFactory::FindEventTypeByName("cpu-cycles");
    ASSERT_TRUE(event_type != nullptr);
    event_attr = EventAttr::CreateDefaultAttrToMonitorEvent(*event_type);
  }

  template <class RecordType>
  void CheckRecordMatchBinary(const RecordType& record);

  EventAttr event_attr;
};

void RecordEqual(const MmapRecord& r1, const MmapRecord& r2) {
  ASSERT_EQ(0, memcmp(&r1.header, &r2.header, sizeof(r1.header)));
  ASSERT_EQ(0, memcmp(&r2.sample_id, &r2.sample_id, sizeof(r1.sample_id)));
  ASSERT_EQ(r1.pid, r2.pid);
  ASSERT_EQ(r1.tid, r2.tid);
  ASSERT_EQ(r1.addr, r2.addr);
  ASSERT_EQ(r1.len, r2.len);
  ASSERT_EQ(r1.pgoff, r2.pgoff);
  ASSERT_EQ(r1.filename, r2.filename);
}

static void RecordEqual(const CommRecord& r1, const CommRecord& r2) {
  ASSERT_EQ(0, memcmp(&r1.header, &r2.header, sizeof(r1.header)));
  ASSERT_EQ(0, memcmp(&r2.sample_id, &r2.sample_id, sizeof(r1.sample_id)));
  ASSERT_EQ(r1.pid, r2.pid);
  ASSERT_EQ(r1.tid, r2.tid);
  ASSERT_EQ(r1.comm, r2.comm);
}

void RecordEqual(const BuildIdRecord& r1, const BuildIdRecord& r2) {
  ASSERT_EQ(0, memcmp(&r1.header, &r2.header, sizeof(r1.header)));
  ASSERT_EQ(0, memcmp(&r2.sample_id, &r2.sample_id, sizeof(r1.sample_id)));
  ASSERT_EQ(r1.pid, r2.pid);
  ASSERT_EQ(r1.build_id, r2.build_id);
  ASSERT_EQ(r1.filename, r2.filename);
}

template <class RecordType>
void RecordTest::CheckRecordMatchBinary(const RecordType& record) {
  std::vector<char> binary = record.BinaryFormat();
  std::unique_ptr<const Record> record_p =
      ReadRecordFromBuffer(event_attr, reinterpret_cast<const perf_event_header*>(binary.data()));
  ASSERT_TRUE(record_p != nullptr);
  ASSERT_EQ(record.header.type, record_p->header.type);
  RecordEqual(record, *reinterpret_cast<const RecordType*>(record_p.get()));
}

TEST_F(RecordTest, CreateKernelMmapRecord) {
  KernelMmap kernel_mmap;  // Leave it uninitialized as we don't care of its content.
  MmapRecord record = CreateKernelMmapRecord(kernel_mmap, event_attr);
  CheckRecordMatchBinary(record);
}

TEST_F(RecordTest, CreateModuleMmapRecord) {
  ModuleMmap module_mmap;
  MmapRecord record = CreateModuleMmapRecord(module_mmap, event_attr);
  CheckRecordMatchBinary(record);
}

TEST_F(RecordTest, CreateThreadCommRecord) {
  ThreadComm thread;
  CommRecord record = CreateThreadCommRecord(thread, event_attr);
  CheckRecordMatchBinary(record);
}

TEST_F(RecordTest, CreateThreadMmapRecord) {
  ThreadComm thread;
  ThreadMmap thread_mmap;
  MmapRecord record = CreateThreadMmapRecord(thread, thread_mmap, event_attr);
  CheckRecordMatchBinary(record);
}

TEST_F(RecordTest, CreateBuildIdRecordForFeatureSection) {
  BuildId build_id;
  BuildIdRecord record = CreateBuildIdRecordForFeatureSection(1, build_id, "init", true);
  CheckRecordMatchBinary(record);
}
