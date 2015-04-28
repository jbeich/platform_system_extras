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

#include <inttypes.h>
#include <base/file.h>

#include "environment.h"

TEST(environment, GetOnlineCpusFromString) {
  ASSERT_EQ(GetOnlineCpusFromString(""), std::vector<int>());
  ASSERT_EQ(GetOnlineCpusFromString("0-2"), std::vector<int>({0, 1, 2}));
  ASSERT_EQ(GetOnlineCpusFromString("0,2-3"), std::vector<int>({0, 2, 3}));
}

struct ProcessKernelSymbolArg {
  int count;
  std::string name;
  uint64_t addr;
  char type;
  std::string module;
};

bool ProcessKernelSymbolCallback(const KernelSymbol& symbol, void* callback_arg) {
  ProcessKernelSymbolArg* arg = reinterpret_cast<ProcessKernelSymbolArg*>(callback_arg);
  ++arg->count;
  if (arg->name == symbol.name) {
    arg->addr = symbol.addr;
    arg->type = symbol.type;
    arg->module = (symbol.module == nullptr ? "" : symbol.module);
    return true;
  }
  return false;
}

TEST(environment, ProcessKernelSymbols) {
  std::string data =
      "ffffffffa005c4e4 d __warned.41698   [libsas]\n"
      "aaaaaaaaaaaaaaaa T _text\n"
      "cccccccccccccccc c ccccc\n";

  const char* tempfile = "tempfile_process_kernel_symbols";
  ASSERT_TRUE(android::base::WriteStringToFile(data, tempfile));

  ProcessKernelSymbolArg arg;
  arg.count = 0;
  arg.name = "_text";
  ASSERT_TRUE(ProcessKernelSymbols(tempfile, ProcessKernelSymbolCallback, &arg));
  ASSERT_EQ(2, arg.count);
  ASSERT_EQ(0xaaaaaaaaaaaaaaaaULL, arg.addr);
  ASSERT_EQ('T', arg.type);
  ASSERT_EQ("", arg.module);

  arg.count = 0;
  arg.name = "_not_exist";
  ASSERT_FALSE(ProcessKernelSymbols(tempfile, ProcessKernelSymbolCallback, &arg));
  ASSERT_EQ(3, arg.count);

  ASSERT_EQ(0, unlink(tempfile));
}

struct ProcessModuleArg {
  int count;
  std::string name;
  uint64_t addr;
};

bool ProcessModuleCallback(uint64_t module_addr, const char* module, void* callback_arg) {
  ProcessModuleArg* arg = reinterpret_cast<ProcessModuleArg*>(callback_arg);
  ++arg->count;
  if (arg->name == module) {
    arg->addr = module_addr;
    return true;
  }
  return false;
}

TEST(environment, ProcessModules) {
  std::string data =
      "nf_defrag_ipv6 34768 1 nf_conntrack_ipv6, Live 0xffffffffa0fe5000\n"
      "a 0x01\n"
      "b 0x02\n";

  const char* tempfile = "tempfile_process_modules";
  ASSERT_TRUE(android::base::WriteStringToFile(data, tempfile));

  ProcessModuleArg arg;
  arg.count = 0;
  arg.name = "nf_defrag_ipv6";
  ASSERT_TRUE(ProcessModules(tempfile, ProcessModuleCallback, &arg));
  ASSERT_EQ(3, arg.count);
  ASSERT_EQ(0xffffffffa0fe5000ULL, arg.addr);

  arg.count = 0;
  arg.name = "a";
  ASSERT_TRUE(ProcessModules(tempfile, ProcessModuleCallback, &arg));
  ASSERT_EQ(3, arg.count);
  ASSERT_EQ(0x1ULL, arg.addr);

  arg.count = 0;
  arg.name = "_not_exist";
  ASSERT_FALSE(ProcessModules(tempfile, ProcessModuleCallback, &arg));
  ASSERT_EQ(3, arg.count);

  ASSERT_EQ(0, unlink(tempfile));
}
