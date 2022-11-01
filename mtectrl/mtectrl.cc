/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <getopt.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <bootloader_message/bootloader_message.h>

#include <functional>
#include <iostream>

void AddItem(std::string* s, const char* item) {
  if (!s->empty()) *s += ",";
  *s += item;
}

bool CheckAndUnset(uint32_t& mode, uint32_t mask) {
  bool is_set = mode & mask;
  mode &= ~mask;
  return is_set;
}

bool UpdateProp(const char* prop_name, const misc_memtag_message& m) {
  uint32_t mode = m.memtag_mode;
  std::string prop_str;
  if (CheckAndUnset(mode, MISC_MEMTAG_MODE_MEMTAG)) AddItem(&prop_str, "memtag");
  if (CheckAndUnset(mode, MISC_MEMTAG_MODE_MEMTAG_ONCE)) AddItem(&prop_str, "memtag-once");
  if (CheckAndUnset(mode, MISC_MEMTAG_MODE_MEMTAG_KERNEL)) AddItem(&prop_str, "memtag-kernel");
  if (CheckAndUnset(mode, MISC_MEMTAG_MODE_MEMTAG_KERNEL_ONCE))
    AddItem(&prop_str, "memtag-kernel-once");
  if (CheckAndUnset(mode, MISC_MEMTAG_MODE_MEMTAG_OFF)) AddItem(&prop_str, "memtag-off");
  if (android::base::GetProperty(prop_name, "") != prop_str)
    android::base::SetProperty(prop_name, prop_str);
  if (mode) {
    LOG(ERROR) << "MTE mode in misc message contained unknown bits: " << mode
               << ". Ignoring and setting " << prop_name << " to " << prop_str;
  }
  return mode == 0;
}

void PrintUsage(const char* progname) {
  std::cerr << "Usage: " << progname
            << " [-s PROPERTY_NAME] none|memtag|memtag-once|memtag-kernel|memtag-kernel-once[,.."
               ".] [default|force_on|force_off]\n"
            << "       " << progname << " -s PROPERTY_NAME\n";
}

int StringToMode(const char* value) {
  int memtag_mode = 0;
  for (const auto& field : android::base::Split(value, ",")) {
    if (field == "memtag") {
      memtag_mode |= MISC_MEMTAG_MODE_MEMTAG;
    } else if (field == "memtag-once") {
      memtag_mode |= MISC_MEMTAG_MODE_MEMTAG_ONCE;
    } else if (field == "memtag-kernel") {
      memtag_mode |= MISC_MEMTAG_MODE_MEMTAG_KERNEL;
    } else if (field == "memtag-kernel-once") {
      memtag_mode |= MISC_MEMTAG_MODE_MEMTAG_KERNEL_ONCE;
    } else if (field == "memtag-off") {
      memtag_mode |= MISC_MEMTAG_MODE_MEMTAG_OFF;
    } else if (field != "none") {
      LOG(ERROR) << "Unknown value for mode: " << field;
      return -1;
    }
  }
  return memtag_mode;
}

bool HandleOverride(const std::string& override_value, misc_memtag_message* m) {
  if (override_value == "force_off") {
    // If the force_off override is active, only allow MEMTAG_MODE_MEMTAG_ONCE.
    m->memtag_mode |= MISC_MEMTAG_MODE_MEMTAG_OFF;
    m->memtag_mode &= ~MISC_MEMTAG_MODE_MEMTAG;
  } else if (override_value == "force_on") {
    m->memtag_mode |= MISC_MEMTAG_MODE_MEMTAG;
    m->memtag_mode &= ~MISC_MEMTAG_MODE_MEMTAG_OFF;
  } else if (!override_value.empty() && override_value != "default") {
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  const char* set_prop = nullptr;
  int opt;
  std::function<bool(misc_memtag_message*, std::string*)> read_memtag_message =
      ReadMiscMemtagMessage;
  std::function<bool(const misc_memtag_message&, std::string*)> write_memtag_message =
      WriteMiscMemtagMessage;

  android::base::unique_fd fake_partition_fd;
  while ((opt = getopt(argc, argv, "s:t:")) != -1) {
    switch (opt) {
      case 's':
        set_prop = optarg;
        break;
      case 't': {
        // Use different fake misc partition for testing.
        const char* filename = optarg;
        fake_partition_fd.reset(open(filename, O_RDWR | O_CLOEXEC));
        int raw_fd = fake_partition_fd.get();
        CHECK_NE(raw_fd, -1);
        CHECK_NE(ftruncate(raw_fd, sizeof(misc_memtag_message)), -1);
        read_memtag_message = [raw_fd](misc_memtag_message* m, std::string*) {
          CHECK(android::base::ReadFully(raw_fd, m, sizeof(*m)));
          return true;
        };
        write_memtag_message = [raw_fd](const misc_memtag_message& m, std::string*) {
          CHECK(android::base::WriteFully(raw_fd, &m, sizeof(m)));
          return true;
        };
        break;
      }
      default:
        PrintUsage(argv[0]);
        return 1;
    }
  }

  const char* value = optind < argc ? argv[optind++] : nullptr;
  const char* override_value = optind < argc ? argv[optind++] : nullptr;

  if (optind != argc) {  // Unknown argument.
    PrintUsage(argv[0]);
    return 1;
  }

  if (!value && set_prop) {
    std::string err;
    misc_memtag_message m = {};
    if (!read_memtag_message(&m, &err)) {
      LOG(ERROR) << "Failed to read memtag message: " << err;
      return 1;
    }
    if (m.magic != MISC_MEMTAG_MAGIC_HEADER || m.version != MISC_MEMTAG_MESSAGE_VERSION) {
      // This should not fail by construction.
      CHECK(UpdateProp(set_prop, {}));
      // This is an expected case, as the partition gets initialized to all zero.
      return 0;
    }
    // UpdateProp failing is an unexpected case, as a message with a valid
    // header should not have an invalid memtag_mode.
    return UpdateProp(set_prop, m) ? 0 : 1;
  }

  if (!value) {
    PrintUsage(argv[0]);
    return 1;
  }

  misc_memtag_message m = {.version = MISC_MEMTAG_MESSAGE_VERSION,
                           .magic = MISC_MEMTAG_MAGIC_HEADER};
  int memtag_mode = StringToMode(value);
  bool valid_value = memtag_mode != -1;
  m.memtag_mode = valid_value ? memtag_mode : 0;

  bool valid_override = true;
  if (override_value) valid_override = HandleOverride(override_value, &m);

  if (!valid_value && !valid_override) {
    return 1;
  }
  std::string err;
  if (!write_memtag_message(m, &err)) {
    LOG(ERROR) << "Failed to apply mode: " << value << ", override: " << override_value << err;
    return 1;
  } else {
    const char* parse_error = "";
    const char* verb = "Applied";
    if (!valid_value) {
      parse_error = " (invalid mode)";
      verb = "Partially applied";
    } else if (!valid_override) {
      // else if because we bail out if both are false above.
      parse_error = " (invalid override)";
      verb = "Partially applied";
    }
    LOG(INFO) << verb << " mode: " << value << ", "
              << "override: " << (override_value ? override_value : "") << parse_error;
    // Because all the bits in memtag_mode were set above, this should never fail.
    if (set_prop) CHECK(UpdateProp(set_prop, m));
    return !valid_value || !valid_override;
  }
}
