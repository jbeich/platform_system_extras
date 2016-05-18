/*
**
** Copyright 2016, The Android Open Source Project
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sstream>

#include <android-base/file.h>

#include "oatmapper.h"
#include "oatmap.pb.h"
#include "perfprofdutils.h"
#include "perfprofdcore.h"
#include "alarmhelper.h"

using wireless_android_play_playlog::OatFileInfo;
using wireless_android_play_playlog::LoadModule;

class OatMapperImpl {
 public:
  OatMapperImpl(const char *cachedir)
      : cachedir_(cachedir) { }

  void postprocess_encoded_profile(AndroidPerfProfile &prof);

  bool collect_oatfile_checksums(const char *oatpath,
                                 OatFileInfo &oatinfo);

  uint64_t encode_addr(const char *loadmodulepath,
                       OatAddressEncoding encoding,
                       uint64_t addr);

  std::string cachepath(const char *oatpath);

 private:
  bool postprocess_oatfile(LoadModule &loadmodule);
  bool harvest_oatinfo_from_oatmap(const char *oatmapcachepath,
                                   OatFileInfo &oatinfo);
  bool generate_oatmap(const char *oatpath,
                       const char *oatmapcachepath,
                       OatFileInfo &oatinfo);

 private:
  const char *cachedir_;
};

std::string OatMapperImpl::cachepath(const char *oatpath)
{
  assert(oatpath);
  std::stringstream ss;
  std::string cn(oatpath);
  std::replace(cn.begin(), cn.end(), '/', '_');
  ss << cachedir_ << "/" << "oatmap" << "_" << cn;
  return ss.str();
}

static off_t get_file_size(const char *path)
{
  struct stat statb;
  assert(path);
  return (stat(path, &statb) == 0 ? statb.st_size : static_cast<off_t>(0));
}

bool OatMapperImpl::harvest_oatinfo_from_oatmap(const char *oatmapcachepath,
                                                OatFileInfo &oatinfo)
{
  off_t omf_size = get_file_size(oatmapcachepath);
  if (!omf_size) {
    W_ALOGW("nonexistent or zero-length oatmap cache file %s", oatmapcachepath);
    return false;
  }

  // read
  std::string encoded;
  encoded.resize(omf_size);
  if (!android::base::ReadFileToString(oatmapcachepath, &encoded)) {
    W_ALOGW("read failed on oatmap cache file %s", oatmapcachepath);
    return false;
  }

  // decode
  oatmap::OatFile oatfile;
  oatfile.ParseFromString(encoded);

  // unpack the portions we want into oatinfo
  W_ALOGD("unpacking oatmap cache file %s", oatmapcachepath);
  oatinfo.set_adler32_checksum(oatfile.adler32_checksum());
  W_ALOGD("  adler32 is %x", oatfile.adler32_checksum());
  for (const auto &dexfile : oatfile.dexfiles()) {
    W_ALOGD("  dex sha: %s", dexfile.sha1signature().c_str());
    oatinfo.add_dex_sha1_signatures(dexfile.sha1signature());
  }

  // success
  return true;
}

bool OatMapperImpl::generate_oatmap(const char *oatpath,
                                    const char *oatmapcachepath,
                                    OatFileInfo &oatinfo)
{
  off_t oatf_size = get_file_size(oatpath);

  if (!oatf_size) {
    W_ALOGW("nonexistent or unreadable OAT file %s", oatpath);
    return false;
  }

  pid_t pid = fork();
  if (pid == -1) {
    W_ALOGE("fork() failed (%s)", strerror(errno));
    return false;
  } else if (pid == 0) {
    // child
    unsigned slot = 0;
    const char *argv[10];

    memset(&argv[0], 0, sizeof(const char *) * 10);
    argv[slot++] = "/system/bin/oatdump";

    std::string a1s("--emitmap=");
    a1s += oatpath;
    argv[slot++] = a1s.c_str();

    std::string a2s("--output=");
    a2s += oatmapcachepath;
    argv[slot++] = a2s.c_str();

    argv[slot++] = nullptr;
    W_ALOGD("about to exec: %s %s %s", argv[0], argv[1], argv[2]);
    execvp(argv[0], (char * const *)argv);
    W_ALOGE("execvp() failed (%s)", strerror(errno));
    return false;
  }

  // parent
  AlarmHelper helper(10, pid);

  // reap child (no zombies please)
  int st = 0;
  TEMP_FAILURE_RETRY(waitpid(pid, &st, 0));

  return true;
}

bool OatMapperImpl::collect_oatfile_checksums(const char *oatpath,
                                              OatFileInfo &oatinfo)
{
  std::string omcp = cachepath(oatpath);
  const char *oatmapcachepath = omcp.c_str();

  W_ALOGD("collect_oatfile_checksums(%s,%s)",
          oatpath, oatmapcachepath);

  off_t omf_size = get_file_size(oatmapcachepath);
  W_ALOGD("cache %s on oatpath %s oatmapcachepath %s",
          omf_size ? "hit" : "miss", oatpath, oatmapcachepath);
  if (omf_size) {
    if (harvest_oatinfo_from_oatmap(oatmapcachepath, oatinfo)) {
      return true;
    }
  }

  if (!generate_oatmap(oatpath, oatmapcachepath, oatinfo)) {
    return false;
  }

  if (harvest_oatinfo_from_oatmap(oatmapcachepath, oatinfo)) {
    return true;
  }

  return false;
}

static bool has_oat_suffix(const char *loadmodulepath)
{
  static const char *oat_suffixes[] = { ".oat", ".odex", ".dex" };
  std::string lm(loadmodulepath);
  for (auto suffix : oat_suffixes) {
    std::string suf(suffix);
    if (lm.size() > suf.size() &&
        lm.compare(lm.size() - suf.size(), suf.size(), suf) == 0) {
      return true;
    }
  }
  return false;
}

void OatMapperImpl::postprocess_encoded_profile(AndroidPerfProfile &prof)
{
  assert(false);

  // Examine each load module
  for (int i = 0; i < prof.load_modules_size(); ++i) {
    auto *lm = prof.mutable_load_modules(i);
    postprocess_oatfile(*lm);
  }
}

bool OatMapperImpl::postprocess_oatfile(LoadModule &loadmodule)
{
  const char *loadmodulepath = loadmodule.name().c_str();
  assert(loadmodulepath);

  // HACK: in the real implementation we'll want to use a real
  // elf reader to examine loadmodulepath to determine if it has
  // the right OAT characteristics, then cache this info so that
  // we don't need to recompute it. For the time being, use a
  // name pattern match (ouch).
  if (has_oat_suffix(loadmodulepath)) {
    W_ALOGD("examining potential oat file %s", loadmodulepath);
    std::string oatmapcachepath = cachepath(loadmodulepath);
    OatFileInfo oatinfo;
    if (collect_oatfile_checksums(loadmodulepath,
                                  oatinfo)) {
      auto new_oat_info = loadmodule.mutable_oat_info();
      (*new_oat_info) = oatinfo;
      return true;
    }
  }
  return false;
}

uint64_t OatMapperImpl::encode_addr(const char *loadmodulepath,
                                    OatAddressEncoding encoding,
                                    uint64_t addr)
{
  (void) encoding;
  assert(false);
  return 0;
}

//======================================================================

OatMapper::OatMapper(const char *cachedir)
    : impl_(new OatMapperImpl(cachedir))
{
}

OatMapper::~OatMapper()
{
}

void OatMapper::postprocess_encoded_profile(AndroidPerfProfile &prof)
{
  impl_->postprocess_encoded_profile(prof);
}

bool OatMapper::collect_oatfile_checksums(const char *oatpath,
                                          OatFileInfo &oatinfo)
{
  return impl_->collect_oatfile_checksums(oatpath, oatinfo);
}

uint64_t OatMapper::encode_addr(const char *loadmodulepath,
                                OatAddressEncoding encoding,
                                uint64_t addr)
{
  return impl_->encode_addr(loadmodulepath, encoding, addr);
}

std::string OatMapper::cachepath(const char *oatfilepath)
{
  return impl_->cachepath(oatfilepath);
}
