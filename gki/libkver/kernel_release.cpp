//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <inttypes.h>
#include <sys/utsname.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <kver/kernel_release.h>

namespace android::kver {

#define KERNEL_RELEASE_PRINT_FORMAT \
  "%" PRIu64 ".%" PRIu64 ".%" PRIu64 "-android%" PRIu64 "-%" PRIu64
#define KERNEL_RELEASE_SCAN_FORMAT KERNEL_RELEASE_PRINT_FORMAT "%n"

// Not taking a string_view because sscanf requires null-termination.
std::optional<KernelRelease> KernelRelease::Parse(const std::string& s, bool allow_suffix) {
  if (s.size() > std::numeric_limits<int>::max()) return std::nullopt;
  int nchars = -1;
  KernelRelease ret;
  auto scan_res = sscanf(s.c_str(), KERNEL_RELEASE_SCAN_FORMAT, &ret.kmi_version_.version_,
                         &ret.kmi_version_.patch_level_, &ret.sub_level_,
                         &ret.kmi_version_.release_, &ret.kmi_version_.gen_, &nchars);
  if (scan_res != 5) return std::nullopt;
  if (nchars < 0) return std::nullopt;
  // If !allow_suffix, ensure the whole string is consumed.
  if (!allow_suffix && nchars != s.size()) return std::nullopt;
  return ret;
}

std::string KernelRelease::string() const {
  return android::base::StringPrintf(KERNEL_RELEASE_PRINT_FORMAT, version(), patch_level(),
                                     sub_level(), android_release(), generation());
}

static std::string UnameRelease() {
  struct utsname buf;
  if (uname(&buf) != 0) {
    PLOG(ERROR) << "Unable to call uname()";
    return "";
  }
  return buf.release;
}

std::optional<KernelRelease> KernelRelease::FromUname() {
  return KernelRelease::Parse(UnameRelease(), true /* allow_suffix */);
}

std::tuple<uint64_t, uint64_t, uint64_t> KernelRelease::kernel_version_tuple() const {
  return std::make_tuple(version(), patch_level(), sub_level());
}

bool IsKernelUpdateValid(const std::string& new_release) {
  return IsKernelUpdateValid(UnameRelease(), new_release);
}

bool IsKernelUpdateValid(const std::string& old_release, const std::string& new_release) {
  // Check that uname() is successful and returns a non-empty kernel release string.
  if (old_release.empty()) {
    LOG(ERROR) << "Unable to get kernel release from uname()";
    return false;
  }

  // Check that the package either contain an empty version (indicating that the new build
  // does not use GKI), or a valid GKI kernel release.
  std::optional<KernelRelease> new_kernel_release;
  if (new_release.empty()) {
    LOG(INFO) << "New build does not contain GKI.";
  } else {
    new_kernel_release = KernelRelease::Parse(new_release);
    if (!new_kernel_release.has_value()) {
      LOG(ERROR) << "New kernel release is not valid GKI kernel release: " << new_release;
      return false;
    }
  }

  // Allow update from non-GKI to non-GKI for legacy devices, or non-GKI to GKI for retrofit
  // devices.
  auto old_kernel_release = KernelRelease::Parse(old_release, true /* allow_suffix */);
  if (!old_kernel_release.has_value()) {
    LOG(INFO) << "Current build does not contain GKI, permit update to kernel release\""
              << new_release << "\" anyways.";
    return true;
  }

  if (!new_kernel_release.has_value()) {
    LOG(ERROR) << "Cannot update from GKI \"" << old_kernel_release->string()
               << "\" to non-GKI build";
    return false;
  }

  // Check that KMI version does not downgrade; i.e. the tuple(w, x, z, k) does
  // not decrease.
  if (old_kernel_release->kmi_version().tuple() > new_kernel_release->kmi_version().tuple()) {
    LOG(ERROR) << "Cannot update from " << old_kernel_release->string() << " to "
               << new_kernel_release->string() << ": KMI version decreases.";
    return false;
  }

  // This ensures that Android release does not downgrade, e.g. you cannot go
  // from 5.10-android13-0 to 5.15-android12-0.
  if (old_kernel_release->android_release() > new_kernel_release->android_release()) {
    LOG(ERROR) << "Cannot update from " << old_kernel_release->string() << " to "
               << new_kernel_release->string() << ": Android release decreases.";
    return false;
  }

  // This ensures that w.x.y does not downgrade; e.g. you cannot go
  // from 5.4.43 to 5.4.42, but you can go from 5.4.43 to 5.10.5.
  if (old_kernel_release->kernel_version_tuple() > new_kernel_release->kernel_version_tuple()) {
    LOG(ERROR) << "Cannot update from " << old_kernel_release->string() << " to "
               << new_kernel_release->string() << ": Kernel version decreases.";
    return false;
  }
  
  LOG(INFO) << "Allow to update from " << old_kernel_release->string() << " to "
            << new_kernel_release->string();
  return true;
}

}  // namespace android::kver
