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

#include "event_attr.h"
#include "event_type.h"
#include "record.h"
#include "record_equal_test.h"

class RecordTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const EventType* type = FindEventTypeByName("cpu-cycles");
    ASSERT_TRUE(type != nullptr);
    event_attr = CreateDefaultPerfEventAttr(*type);
  }

  void CheckRecordMatchBinary(const Record& record) {
    const char* p = record.Binary();
    std::vector<std::unique_ptr<Record>> records =
        ReadRecordsFromBuffer(event_attr, p, record.size());
    ASSERT_EQ(1u, records.size());
    CheckRecordEqual(record, *records[0]);
  }

  perf_event_attr event_attr;
};

TEST_F(RecordTest, MmapRecordMatchBinary) {
  MmapRecord record(event_attr, true, 1, 2, 0x1000, 0x2000, 0x3000,
                    "MmapRecord", 0);
  CheckRecordMatchBinary(record);
}

TEST_F(RecordTest, CommRecordMatchBinary) {
  CommRecord record(event_attr, 1, 2, "CommRecord", 0);
  CheckRecordMatchBinary(record);
}

TEST_F(RecordTest, RecordCache_smoke) {
  event_attr.sample_id_all = 1;
  event_attr.sample_type |= PERF_SAMPLE_TIME;
  RecordCache cache(true, 2, 2);
  MmapRecord* r1 = new MmapRecord(event_attr, true, 1, 1, 0x100, 0x200, 0x300,
                                  "mmap_record1", 0, 3);
  MmapRecord* r2 = new MmapRecord(event_attr, true, 1, 1, 0x100, 0x200, 0x300,
                                  "mmap_record1", 0, 1);
  MmapRecord* r3 = new MmapRecord(event_attr, true, 1, 1, 0x100, 0x200, 0x300,
                                  "mmap_record1", 0, 4);
  MmapRecord* r4 = new MmapRecord(event_attr, true, 1, 1, 0x100, 0x200, 0x300,
                                  "mmap_record1", 0, 6);
  // Push r1.
  cache.Push(std::unique_ptr<Record>(r1));
  ASSERT_EQ(nullptr, cache.Pop());
  // Push r2.
  cache.Push(std::unique_ptr<Record>(r2));
  // Pop r2.
  std::unique_ptr<Record> popped_r = cache.Pop();
  ASSERT_TRUE(popped_r != nullptr);
  ASSERT_EQ(r2, popped_r.get());
  ASSERT_EQ(nullptr, cache.Pop());
  // Push r3.
  cache.Push(std::unique_ptr<Record>(r3));
  ASSERT_EQ(nullptr, cache.Pop());
  // Push r4.
  cache.Push(std::unique_ptr<Record>(r4));
  // Pop r1.
  popped_r = cache.Pop();
  ASSERT_TRUE(popped_r != nullptr);
  ASSERT_EQ(r1, popped_r.get());
  // Pop r3.
  popped_r = cache.Pop();
  ASSERT_TRUE(popped_r != nullptr);
  ASSERT_EQ(r3, popped_r.get());
  ASSERT_EQ(nullptr, cache.Pop());
  // Pop r4.
  std::vector<std::unique_ptr<Record>> last_records = cache.PopAll();
  ASSERT_EQ(1u, last_records.size());
  ASSERT_EQ(r4, last_records[0].get());
}

TEST_F(RecordTest, RecordCache_FIFO) {
  event_attr.sample_id_all = 1;
  event_attr.sample_type |= PERF_SAMPLE_TIME;
  RecordCache cache(true, 2, 2);
  std::vector<MmapRecord*> records;
  for (size_t i = 0; i < 10; ++i) {
    records.push_back(new MmapRecord(event_attr, true, 1, i, 0x100, 0x200,
                                     0x300, "mmap_record1", 0));
    cache.Push(std::unique_ptr<Record>(records.back()));
  }
  std::vector<std::unique_ptr<Record>> out_records = cache.PopAll();
  ASSERT_EQ(records.size(), out_records.size());
  for (size_t i = 0; i < records.size(); ++i) {
    ASSERT_EQ(records[i], out_records[i].get());
  }
}

TEST_F(RecordTest, RecordCache_PushRecordVector) {
  event_attr.sample_id_all = 1;
  event_attr.sample_type |= PERF_SAMPLE_TIME;
  RecordCache cache(true, 2, 2);
  MmapRecord* r1 = new MmapRecord(event_attr, true, 1, 1, 0x100, 0x200, 0x300,
                                  "mmap_record1", 0, 1);
  MmapRecord* r2 = new MmapRecord(event_attr, true, 1, 1, 0x100, 0x200, 0x300,
                                  "mmap_record1", 0, 3);
  std::vector<std::unique_ptr<Record>> records;
  records.push_back(std::unique_ptr<Record>(r1));
  records.push_back(std::unique_ptr<Record>(r2));
  cache.Push(std::move(records));
  std::unique_ptr<Record> popped_r = cache.Pop();
  ASSERT_TRUE(popped_r != nullptr);
  ASSERT_EQ(r1, popped_r.get());
  std::vector<std::unique_ptr<Record>> last_records = cache.PopAll();
  ASSERT_EQ(1u, last_records.size());
  ASSERT_EQ(r2, last_records[0].get());
}
