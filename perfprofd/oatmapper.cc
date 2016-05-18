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

// FIXME: hack -- this needs to be passed in from the config reader.
const char *cachedir_path = "/data/misc/perfprofd";

using wireless_android_play_playlog::OatFileInfo;
using wireless_android_play_playlog::LoadModule;

static std::string cachepath(const char *oatpath, const char *cachetype)
{
  assert(oatpath);
  assert(cachetype);
  assert(!strcmp(cachetype, "cksums") || !strcmp(cachetype, "oatmap"));
  std::stringstream ss;
  std::string cn(oatpath);
  std::replace(cn.begin(), cn.end(), '/', '_');
  ss << cachedir_path << "/" << cachetype << "_" << cn;
  return ss.str();
}

static bool write_oatinfo_to_file(const char *cksumcachepath,
                                  OatFileInfo &oatinfo)
{
  //
  // Serialize protobuf to array
  //
  int size = oatinfo.ByteSize();
  std::string data;
  data.resize(size);
  ::google::protobuf::uint8* dtarget =
        reinterpret_cast<::google::protobuf::uint8*>(string_as_array(&data));
  oatinfo.SerializeWithCachedSizesToArray(dtarget);

  //
  // Open file and write encoded data to it
  //
  FILE *fp = fopen(cksumcachepath, "w");
  if (!fp) {
    W_ALOGE("open for write failed for oatinfo cache file %s", cksumcachepath);
    return false;
  }
  size_t fsiz = size;
  if (fwrite(dtarget, fsiz, 1, fp) != 1) {
    fclose(fp);
    W_ALOGE("write failed on oatinfo cache file %s", cksumcachepath);
    return false;
  }
  fclose(fp);
  return true;
}

static bool read_oatinfo_from_file(const char *cksumcachepath,
                                   OatFileInfo &oatinfo)
{
  struct stat statb;
  if (stat(cksumcachepath, &statb) != 0) {
    W_ALOGW("failed to stat() oatinfo cache file %s", cksumcachepath);
    return false;
  }
  if (!statb.st_size) {
    W_ALOGD("zero-size oatinfo cache file %s", cksumcachepath);
    return false;
  }

  // read
  std::string encoded;
  encoded.resize(statb.st_size);
  if (!android::base::ReadFileToString(cksumcachepath, &encoded)) {
    W_ALOGW("ReadFileToString failed on oatinfo cache file %s", cksumcachepath);
    return false;
  }

  // decode
  W_ALOGD("collecting data from oatinfo cache file %s", cksumcachepath);
  oatinfo.ParseFromString(encoded);
  return true;
}

static bool harvest_oatinfo_from_oatmap(const char *oatmapcachepath,
                                        OatFileInfo &oatinfo)
{
  struct stat statb;
  if (stat(oatmapcachepath, &statb) != 0) {
    W_ALOGW("unable to stat oatmap cache file %s", oatmapcachepath);
    return false;
  }
  if (!statb.st_size) {
    W_ALOGD("zero-size oatmap cache file %s", oatmapcachepath);
    return false;
  }

  // read
  std::string encoded;
  encoded.resize(statb.st_size);
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
  return true;
}

static bool generate_oatinfo(const char *oatpath,
                             const char *oatmapcachepath,
                             OatFileInfo &oatinfo)
{
  struct stat statb;
  if (stat(oatpath, &statb) != 0) {
    W_ALOGW("unable to stat oat file %s", oatpath);
    return false;
  }
  if (!statb.st_size) {
    W_ALOGW("zero-size OAT file %s", oatpath);
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

  // read results from mapping file
  if (harvest_oatinfo_from_oatmap(oatmapcachepath, oatinfo)) {
    return true;
  }
  return false;
}

bool collect_oatfile_checksums(const char *oatpath,
                               const char *oatmapcachepath,
                               const char *cksumcachepath,
                               OatFileInfo &oatinfo)
{
  struct stat statb;
  if (stat(cksumcachepath, &statb) == 0) { // if file exists...
    // read and return
    W_ALOGD("cache hit on oatpath %s cksumcachepath %s",
            oatpath, cksumcachepath);
    if (read_oatinfo_from_file(cksumcachepath, oatinfo)) {
      return true;
    }
    // Q: unlink file if this happens?
    return false;
  }
  if (stat(oatmapcachepath, &statb) == 0) { // if file exists...
    W_ALOGD("cache hit on oatpath %s oatmapcachepath %s",
            oatpath, oatmapcachepath);
  } else if (!generate_oatinfo(oatpath, oatmapcachepath, oatinfo)) {
    return false;
  }

  // read results from mapping cache file
  if (harvest_oatinfo_from_oatmap(oatmapcachepath, oatinfo)) {
    write_oatinfo_to_file(cksumcachepath, oatinfo);
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

bool postprocess_oatfile(const char *loadmodulepath,
                         LoadModule &loadmodule)
{
  assert(loadmodulepath);

  // HACK: in the real implementation we'll want to use a real
  // elf reader to examine loadmodulepath to determine if it has
  // the right OAT characteristics, then cache this info so that
  // we don't need to recompute it. For the time being, use a
  // name pattern match (ouch).
  if (has_oat_suffix(loadmodulepath)) {
    W_ALOGD("examining potential oat file %s", loadmodulepath);
    std::string cksumscachepath = cachepath(loadmodulepath, "cksums");
    std::string oatmapcachepath = cachepath(loadmodulepath, "oatmap");
    OatFileInfo oatinfo;
    if (collect_oatfile_checksums(loadmodulepath,
                                  cksumscachepath.c_str(),
                                  oatmapcachepath.c_str(),
                                  oatinfo)) {
      auto new_oat_info = loadmodule.mutable_oat_info();
      (*new_oat_info) = oatinfo;
      return true;
    }
  }
  return false;
}
