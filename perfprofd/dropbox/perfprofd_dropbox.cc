/*
**
** Copyright 2017, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "perfprofd_dropbox.h"

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/stringprintf.h>
#include <utils/String16.h>

#include <android/os/DropBoxManager.h>

namespace perfprofd {
namespace dropbox {

using DropBoxManager = ::android::os::DropBoxManager;
using Status = ::android::binder::Status;
using String16 = ::android::String16;

bool SubmitToDropbox(const std::string& filename) {
  ::android::sp<DropBoxManager> dropbox = new DropBoxManager();
  Status status = dropbox->addFile(String16("perfprofd"), filename, 0);
  return status.isOk();
}

}  // namespace dropbox
}  // namespace perfprofd
