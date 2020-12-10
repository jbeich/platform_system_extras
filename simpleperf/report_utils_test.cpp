/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <string>
#include <vector>

#include "record_file.h"
#include "report_utils.h"
#include "thread_tree.h"

using namespace simpleperf;

class CallChainReportBuilderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    thread_tree.SetThreadName(1, 1, "thread1");
    thread = thread_tree.FindThread(1);

    // Add symbols for fake files.
    FileFeature file;
    file.path = fake_interpreter_path;
    file.type = DSO_ELF_FILE;
    file.min_vaddr = file.file_offset_of_min_vaddr = 0;
    file.symbols = {
        Symbol("art_func1", 0x0, 0x100),
        Symbol("art_func2", 0x100, 0x100),
    };
    thread_tree.AddDsoInfo(file);

    file.path = fake_dex_file_path;
    file.type = DSO_DEX_FILE;
    file.min_vaddr = file.file_offset_of_min_vaddr = 0;
    file.symbols = {
        Symbol("java_method1", 0x0, 0x100),
        Symbol("java_method2", 0x100, 0x100),
    };
    thread_tree.AddDsoInfo(file);

    file.path = fake_jit_cache_path;
    file.type = DSO_ELF_FILE;
    file.min_vaddr = file.file_offset_of_min_vaddr = 0;
    file.symbols = {
        Symbol("java_method2", 0x3000, 0x100),
        Symbol("java_method3", 0x3100, 0x100),
    };
    thread_tree.AddDsoInfo(file);

    thread_tree.AddThreadMap(1, 1, 0x1000, 0x1000, 0x0, fake_interpreter_path);
    thread_tree.AddThreadMap(1, 1, 0x2000, 0x1000, 0x0, fake_dex_file_path);
    thread_tree.AddThreadMap(1, 1, 0x3000, 0x1000, 0x0, fake_jit_cache_path,
                             map_flags::PROT_JIT_SYMFILE_MAP);
  }

  ThreadTree thread_tree;
  const ThreadEntry* thread;
  const std::string fake_interpreter_path = "fake_dir/libart.so";
  const std::string fake_dex_file_path = "fake_dir/framework.jar";
  const std::string fake_jit_cache_path = "fake_jit_app_cache:0";

  const std::vector<uint64_t> fake_ips = {
      0x1000,  // art_func1
      0x1100,  // art_func2
      0x2000,  // java_method1 in dex file
      0x1000,  // art_func1
      0x1100,  // art_func2
      0x3000,  // java_method2 in jit cache
      0x1000,  // art_func1
      0x1100,  // art_func2
  };
};

TEST_F(CallChainReportBuilderTest, default_option) {
  // Default to remove art frames, and convert jit frames.
  CallChainReportBuilder builder(thread_tree);
  std::vector<CallChainReportEntry> entries = builder.Build(thread, fake_ips, 0);
  ASSERT_EQ(entries.size(), 2);
  ASSERT_EQ(entries[0].ip, 0x2000);
  ASSERT_STREQ(entries[0].symbol->Name(), "java_method1");
  ASSERT_EQ(entries[0].dso->Path(), fake_dex_file_path);
  ASSERT_EQ(entries[0].vaddr_in_file, 0);
  ASSERT_EQ(entries[1].ip, 0x3000);
  ASSERT_STREQ(entries[1].symbol->Name(), "java_method2");
  ASSERT_EQ(entries[1].dso->Path(), fake_dex_file_path);
  ASSERT_EQ(entries[1].vaddr_in_file, 0x100);
}

TEST_F(CallChainReportBuilderTest, not_convert_jit_frame) {
  CallChainReportBuilder builder(thread_tree);
  builder.SetConvertJITFrame(false);
  std::vector<CallChainReportEntry> entries = builder.Build(thread, fake_ips, 0);
  ASSERT_EQ(entries.size(), 2);
  ASSERT_EQ(entries[0].ip, 0x2000);
  ASSERT_STREQ(entries[0].symbol->Name(), "java_method1");
  ASSERT_EQ(entries[0].dso->Path(), fake_dex_file_path);
  ASSERT_EQ(entries[0].vaddr_in_file, 0);
  ASSERT_EQ(entries[1].ip, 0x3000);
  ASSERT_STREQ(entries[1].symbol->Name(), "java_method2");
  ASSERT_EQ(entries[1].dso->Path(), fake_jit_cache_path);
  ASSERT_EQ(entries[1].vaddr_in_file, 0x3000);
}

TEST_F(CallChainReportBuilderTest, not_remove_art_frame) {
  CallChainReportBuilder builder(thread_tree);
  builder.SetRemoveArtFrame(false);
  std::vector<CallChainReportEntry> entries = builder.Build(thread, fake_ips, 0);
  ASSERT_EQ(entries.size(), 8);
  for (size_t i : {0, 3, 6}) {
    ASSERT_EQ(entries[i].ip, 0x1000);
    ASSERT_STREQ(entries[i].symbol->Name(), "art_func1");
    ASSERT_EQ(entries[i].dso->Path(), fake_interpreter_path);
    ASSERT_EQ(entries[i].vaddr_in_file, 0);
    ASSERT_EQ(entries[i + 1].ip, 0x1100);
    ASSERT_STREQ(entries[i + 1].symbol->Name(), "art_func2");
    ASSERT_EQ(entries[i + 1].dso->Path(), fake_interpreter_path);
    ASSERT_EQ(entries[i + 1].vaddr_in_file, 0x100);
  }
  ASSERT_EQ(entries[2].ip, 0x2000);
  ASSERT_STREQ(entries[2].symbol->Name(), "java_method1");
  ASSERT_EQ(entries[2].dso->Path(), fake_dex_file_path);
  ASSERT_EQ(entries[2].vaddr_in_file, 0);
  ASSERT_EQ(entries[5].ip, 0x3000);
  ASSERT_STREQ(entries[5].symbol->Name(), "java_method2");
  ASSERT_EQ(entries[5].dso->Path(), fake_jit_cache_path);
  ASSERT_EQ(entries[5].vaddr_in_file, 0x3000);
}

TEST_F(CallChainReportBuilderTest, remove_jit_frame_called_by_dex_frame) {
  std::vector<uint64_t> fake_ips = {
      0x3000,  // java_method2 in jit cache
      0x1000,  // art_func1
      0x1100,  // art_func2
      0x2100,  // java_method2 in dex file
      0x1000,  // art_func1
  };
  CallChainReportBuilder builder(thread_tree);
  std::vector<CallChainReportEntry> entries = builder.Build(thread, fake_ips, 0);
  ASSERT_EQ(entries.size(), 1);
  ASSERT_EQ(entries[0].ip, 0x2100);
  ASSERT_STREQ(entries[0].symbol->Name(), "java_method2");
  ASSERT_EQ(entries[0].dso->Path(), fake_dex_file_path);
  ASSERT_EQ(entries[0].vaddr_in_file, 0x100);
}
