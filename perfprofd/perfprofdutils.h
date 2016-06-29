/*
**
** Copyright 2015, The Android Open Source Project
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

#ifndef SYSTEM_EXTRAS_PERFPROFD_PERFPROFDUTILS_H_
#define SYSTEM_EXTRAS_PERFPROFD_PERFPROFDUTILS_H_

#include <sys/cdefs.h>
#include <string>

__BEGIN_DECLS

//
// These routines are separated out from the core perfprofd so
// as to be used as part of the unit test (see the README.txt
// alongside the unit test for more info).
//
extern void perfprofd_log_error(const char *fmt, ...);
extern void perfprofd_log_warning(const char *fmt, ...);
extern void perfprofd_log_info(const char *fmt, ...);
extern void perfprofd_sleep(int seconds);

#define W_ALOGE perfprofd_log_error
#define W_ALOGW perfprofd_log_warning
#define W_ALOGI perfprofd_log_info

inline char* string_as_array(std::string* str) {
  return str->empty() ? NULL : &*str->begin();
}

//
// Alignment helpers
//
inline bool IsAlignedPtr(unsigned char *ptr, unsigned pow2) {
  uint64_t val64 = reinterpret_cast<uint64_t>(ptr);
  return (val64 & (pow2-1) ? false : true);
}

inline bool IsWordAlignedPtr(unsigned char *ptr) {
  return IsAlignedPtr(ptr, 4);
}

#if DEBUGGING
#define DEBUGLOG(x) W_ALOGD x
#else
#define DEBUGLOG(x)
#endif

__END_DECLS

#endif // SYSTEM_EXTRAS_PERFPROFD_PERFPROFDUTILS_H_
