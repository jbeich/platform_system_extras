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
#include "kallsyms.h"

#include <inttypes.h>

#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

namespace simpleperf {

namespace {

const char kKallsymsPath[] = "/proc/kallsyms";
const char kPtrRestrictPath[] = "/proc/sys/kernel/kptr_restrict";
const char kLowerPtrRestrictAndroidProp[] = "security.lower_kptr_restrict";

// Tries to read the kernel symbol file and ensure that at least some symbol
// addresses are non-null.
bool CanReadKernelSymbolAddresses() {
  std::string kallsyms;
  // TODO(tweek): Only read the beginning of the file.
  if (!android::base::ReadFileToString(kKallsymsPath, &kallsyms)) {
    LOG(DEBUG) << "failed to read " << kKallsymsPath;
    return false;
  }
  bool non_null_addr = false;
  auto symbol_callback = [&](const KernelSymbol& symbol) {
    if (symbol.addr != 0u) {
      non_null_addr = true;
      return true;
    }
    return false;
  };
  ProcessKernelSymbols(kallsyms, symbol_callback);
  return non_null_addr;
}
// Define a scope in which access to kallsyms is possible.
// This is based on the Perfetto implementation.
class ScopedKptrUnrestrict {
 public:
  ScopedKptrUnrestrict(bool use_android_property);  // Lowers kptr_restrict if necessary.
  ~ScopedKptrUnrestrict();                          // Restores the initial kptr_restrict.

 private:
  static void WriteKptrRestrict(const std::string&);

  std::string initial_value_;
  bool use_android_property_;
  bool restore_on_dtor_ = true;
};

ScopedKptrUnrestrict::ScopedKptrUnrestrict(bool use_android_property)
    : use_android_property_(use_android_property) {
  if (CanReadKernelSymbolAddresses()) {
    // Everything seems to work (e.g., we are running as root and kptr_restrict
    // is < 2). Don't touching anything.
    restore_on_dtor_ = false;
    return;
  }

  if (use_android_property_) {
    android::base::SetProperty(kLowerPtrRestrictAndroidProp, "1");
    // Init takes some time to react to the property change.
    // Unfortunately, we cannot read kptr_restrict because of SELinux. Instead,
    // we detect this by reading the initial lines of kallsyms and checking
    // that they are non-zero. This loop waits for at most 250ms (50 * 5ms).
    for (int attempt = 1; attempt <= 50; ++attempt) {
      usleep(5000);
      if (CanReadKernelSymbolAddresses()) return;
    }
    LOG(ERROR) << "kallsyms addresses are still masked after setting "
               << kLowerPtrRestrictAndroidProp;
    return;
  }

  // Otherwise, read the kptr_restrict value and lower it if needed.
  bool read_res = android::base::ReadFileToString(kPtrRestrictPath, &initial_value_);
  if (!read_res) {
    LOG(ERROR) << "Failed to read " << kPtrRestrictPath;
    return;
  }

  // Progressively lower kptr_restrict until we can read kallsyms.
  for (int value = atoi(initial_value_.c_str()); value > 0; --value) {
    WriteKptrRestrict(std::to_string(value));
    if (CanReadKernelSymbolAddresses()) return;
  }
}

ScopedKptrUnrestrict::~ScopedKptrUnrestrict() {
  if (!restore_on_dtor_) return;
  if (use_android_property_) {
    android::base::SetProperty(kLowerPtrRestrictAndroidProp, "0");
  } else if (!initial_value_.empty()) {
    WriteKptrRestrict(initial_value_);
  }
}

void ScopedKptrUnrestrict::WriteKptrRestrict(const std::string& value) {
  if (!android::base::WriteStringToFile(value, kPtrRestrictPath)) {
    LOG(ERROR) << "Failed to set " << kPtrRestrictPath << " to " << value;
  }
}

}  // namespace

bool ProcessKernelSymbols(std::string& symbol_data,
                          const std::function<bool(const KernelSymbol&)>& callback) {
  char* p = &symbol_data[0];
  char* data_end = p + symbol_data.size();
  while (p < data_end) {
    char* line_end = strchr(p, '\n');
    if (line_end != nullptr) {
      *line_end = '\0';
    }
    size_t line_size = (line_end != nullptr) ? (line_end - p) : (data_end - p);
    // Parse line like: ffffffffa005c4e4 d __warned.41698       [libsas]
    char name[line_size];
    char module[line_size];
    strcpy(module, "");

    KernelSymbol symbol;
    int ret = sscanf(p, "%" PRIx64 " %c %s%s", &symbol.addr, &symbol.type, name, module);
    if (line_end != nullptr) {
      *line_end = '\n';
      p = line_end + 1;
    } else {
      p = data_end;
    }
    if (ret >= 3) {
      symbol.name = name;
      size_t module_len = strlen(module);
      if (module_len > 2 && module[0] == '[' && module[module_len - 1] == ']') {
        module[module_len - 1] = '\0';
        symbol.module = &module[1];
      } else {
        symbol.module = nullptr;
      }

      if (callback(symbol)) {
        return true;
      }
    }
  }
  return false;
}

bool LoadKernelSymbols(std::string* kallsyms, bool use_android_property /* = false */) {
  ScopedKptrUnrestrict kptr_unrestrict(use_android_property);
  return android::base::ReadFileToString(kKallsymsPath, kallsyms);
}

}  // namespace simpleperf
