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

#include "oatmapper.h"

//#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sstream>
#include <set>
#include <vector>

#include <android-base/file.h>

// Uncomment while debugging
// #define DEBUGGING 1

#include "oatreader.h"
#include "oatdexvisitor.h"
#include "oatmap.pb.h"
#include "genoatmap.h"
#include "perfprofdutils.h"
#include "perfprofdcore.h"
#include "alarmhelper.h"

using wireless_android_play_playlog::OatFileInfo;
using wireless_android_play_playlog::LoadModule;

struct MethodTextMapEntry {
  explicit MethodTextMapEntry(uint64_t ip) { method_start_ = ip; }
  MethodTextMapEntry(uint64_t ip, uint32_t siz, uint32_t val)
      : method_start_(ip)
      , method_size_(siz)
      , encoded_value_(val) {}
  uint64_t method_start_;
  uint32_t method_size_;
  uint32_t encoded_value_;
};

static bool CompareMethodTextMapEntry(const MethodTextMapEntry &left,
                                      const MethodTextMapEntry &right) {
  return left.method_start_ < right.method_start_;
}

class AddressRemapper {
 public:
  AddressRemapper(const oatmap::MapOatFile &oatfilemap,
                  uint64_t start_addr);

  // Look up an IP address, returning TRUE if it falls within a
  // range corresponding to a given OAT method, FALSE if not.
  // For TRUE returns, "encoded" is filled in with the encoded addr.
  bool lookup_ip(uint64_t ip, uint64_t &encoded);

  // debugging
  std::string toString() const;

 private:
  std::vector<MethodTextMapEntry> lookup_vec_;
  typedef std::vector<MethodTextMapEntry>::iterator LookupVecIterator;
};

AddressRemapper::AddressRemapper(const oatmap::MapOatFile &oatfilemap,
                                 uint64_t start_addr)
{
  // Count methods
  uint64_t method_count = 0;
  for (auto &df : oatfilemap.dexfiles()) {
    for (auto &dc : df.classes()) {
      for (auto &dm : dc.methods()) {
        (void)dm;
        method_count += 1;
      }
    }
  }
  assert(method_count < UINT32_MAX);
  lookup_vec_.reserve(method_count);

  // Populate lookup vector
  uint32_t mc32 = 1;
  uint64_t prev_mstart = 0;
  for (auto &df : oatfilemap.dexfiles()) {
    for (auto &dc : df.classes()) {
      for (auto &dm : dc.methods()) {

        // enforce sorted order
        assert(dm.mstart() >= prev_mstart);
        assert(dm.msize());
        assert(false);
        prev_mstart = dm.mstart();

        lookup_vec_.emplace_back(start_addr + dm.mstart(),
                                 dm.msize(),
                                 mc32++);
      }
    }
  }

  DEBUGLOG(("AddressRemapper: start_addr=0x%" PRIx64 ", %u methods",
            start_addr, mc32-1));
  DEBUGLOG(("%s", toString().c_str()));
}

std::string AddressRemapper::toString() const
{
  std::stringstream ss;
  ss << "AddressRemapper (" << lookup_vec_.size() << " methods):\n";
  ss << "AddressRemapper:\n";
  unsigned idx = 0;
  for (const auto &entry : lookup_vec_) {
    ss << "  " << std::dec << idx << ":"
       << " start=" << std::hex << entry.method_start_
       << " size=" << std::dec << entry.method_size_
       << " val=" << entry.encoded_value_
       << "\n";
    idx++;
  }
  return ss.str();
}

bool AddressRemapper::lookup_ip(uint64_t ip, uint64_t &encoded)
{
  MethodTextMapEntry x(ip);
  DEBUGLOG(("testing 0x%" PRIx64 "", ip));
  LookupVecIterator lb = std::lower_bound(lookup_vec_.begin(),
                                          lookup_vec_.end(),
                                          x, CompareMethodTextMapEntry);

  DEBUGLOG(("lookup 0x%" PRIx64 " found: st=0x%" PRIx64 " siz=%u",
            ip, lb->method_start_, lb->method_size_));

  if (ip == lb->method_start_) {
    DEBUGLOG(("match for st=0x%" PRIx64 " siz=%u enc=%d",
              lb->method_start_, lb->method_size_, lb->encoded_value_));
    encoded = lb->encoded_value_;
    return true;
  }
  if (lb != lookup_vec_.begin()) {
    lb--;
    if (ip >= lb->method_start_ &&
        (ip - lb->method_start_) < lb->method_size_) {
      DEBUGLOG(("match for st=0x%" PRIx64 " siz=%u enc=%d",
                lb->method_start_, lb->method_size_, lb->encoded_value_));
      encoded = lb->encoded_value_;
      return true;
    }
  }
  DEBUGLOG(("no match"));
  return false;
}

class OatMapperImpl {
 public:
  OatMapperImpl(const char *cachedir, OatMapGenFlavor mapgenflav)
      : cachedir_(cachedir), mapgenflav_(mapgenflav) { }

  void postprocess_encoded_profile(AndroidPerfProfile &prof);

  bool collect_oatfile_checksums(const char *oatpath,
                                 uint64_t start_addr,
                                 OatFileInfo &oatinfo);

  uint64_t encode_addr(const char *loadmodulepath,
                       OatAddressEncoding encoding,
                       uint64_t addr);

  std::string cachepath(const char *oatpath);

 private:
  bool examine_potential_oatfile(LoadModule &loadmodule);
  bool read_oatmap_from_cachefile(const char *oatmapcachepath,
                                  oatmap::MapOatFile &mapoatfile);
  void create_maptable_for_oatfile(const char *oatpath,
                                   uint64_t start_addr,
                                   oatmap::MapOatFile &mapoatfile);
  void harvest_oatinfo_from_oatmap(const oatmap::MapOatFile &mapoatfile,
                                   OatFileInfo &oatinfo);
  bool generate_oatmap_external(const char *oatpath,
                                const char *oatmapcachepath);
  bool emit_oatmap_to_file(const char *cachepath,
                           oatmap::MapOatFile &mapoatfile);

 private:
  const char *cachedir_;
  std::map<std::string, std::unique_ptr<AddressRemapper>> mappers_;
  typedef std::map<std::string, std::unique_ptr<AddressRemapper>>::iterator MapTableIterator;
  OatMapGenFlavor mapgenflav_;
};

void OatMapperImpl::create_maptable_for_oatfile(const char *oatpath,
                                                uint64_t start_addr,
                                                oatmap::MapOatFile &mapoatfile)
{
  // Already in cache?
  std::string lm(oatpath);
  if (mappers_.find(lm) != mappers_.end())
    return;

  DEBUGLOG(("building AddressRemapper for %s", oatpath));

  // Add to map cache
  mappers_.insert(std::make_pair(lm, std::unique_ptr<AddressRemapper>(new AddressRemapper(mapoatfile, start_addr))));
}

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

bool OatMapperImpl::read_oatmap_from_cachefile(const char *oatmapcachepath,
                                               oatmap::MapOatFile &mapoatfile)
{
  off_t omf_size = get_file_size(oatmapcachepath);
  if (!omf_size) {
    DEBUGLOG(("nonexistent or zero-length oatmap cache file %s",
              oatmapcachepath));
    return false;
  }

  // read
  std::string encoded;
  encoded.resize(omf_size);
  if (!android::base::ReadFileToString(oatmapcachepath, &encoded)) {
    DEBUGLOG(("read failed on oatmap cache file %s", oatmapcachepath));
    return false;
  }

  // decode
  mapoatfile.ParseFromString(encoded);

  DEBUGLOG(("read cachefile %s", oatmapcachepath));

  return true;
}

void OatMapperImpl::harvest_oatinfo_from_oatmap(const oatmap::MapOatFile &mapoatfile,
                                                OatFileInfo &oatinfo)
{
  // unpack the portions we want into oatinfo
  oatinfo.set_adler32_checksum(mapoatfile.adler32_checksum());
  DEBUGLOG(("  adler32 is %x", mapoatfile.adler32_checksum()));
  for (const auto &dexfile : mapoatfile.dexfiles()) {
    DEBUGLOG(("  dex sha: %s", dexfile.sha1signature().c_str()));
    oatinfo.add_dex_sha1_signatures(dexfile.sha1signature());
  }
}

bool OatMapperImpl::generate_oatmap_external(const char *oatpath,
                                             const char *oatmapcachepath)
{
  off_t oatf_size = get_file_size(oatpath);

  if (!oatf_size) {
    DEBUGLOG(("nonexistent or unreadable OAT file %s", oatpath));
    return false;
  }

  pid_t pid = fork();
  if (pid == -1) {
    DEBUGLOG(("fork() failed (%s)", strerror(errno)));
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
    DEBUGLOG(("about to exec: %s %s %s", argv[0], argv[1], argv[2]));
    execvp(argv[0], (char * const *)argv);
    DEBUGLOG(("execvp() failed (%s)", strerror(errno)));
    return false;
  }

  // parent
  AlarmHelper helper(10, pid);

  // reap child (no zombies please)
  int st = 0;
  TEMP_FAILURE_RETRY(waitpid(pid, &st, 0));

  return true;
}

bool OatMapperImpl::emit_oatmap_to_file(const char *cachepath,
                                        oatmap::MapOatFile &mapoatfile) {
  FILE *fp = fopen(cachepath, "w");
  if (!fp)
    return false;
  int size = mapoatfile.ByteSize();
  std::string data;
  data.resize(size);
  ::google::protobuf::uint8 *dtarget =
        reinterpret_cast<::google::protobuf::uint8 *>(string_as_array(&data));
  mapoatfile.SerializeWithCachedSizesToArray(dtarget);
  size_t fsiz = size;
  DEBUGLOG(("writing %d bytes to %s", size, cachepath));
  if (fwrite(dtarget, fsiz, 1, fp) != 1) {
    fclose(fp);
    return false;
  }
  return true;
}

bool OatMapperImpl::collect_oatfile_checksums(const char *oatpath,
                                              uint64_t start_addr,
                                              OatFileInfo &oatinfo)
{
  std::string omcp = cachepath(oatpath);
  const char *oatmapcachepath = omcp.c_str();
  DEBUGLOG(("collect_oatfile_checksums(%s,%s)",
            oatpath, oatmapcachepath));

  //
  // Try reading from the cache first
  //
  oatmap::MapOatFile mapoatfile;
  off_t omf_size = get_file_size(oatmapcachepath);
  DEBUGLOG(("cache %s on oatpath %s oatmapcachepath %s",
            omf_size ? "hit" : "miss", oatpath, oatmapcachepath));
  if (omf_size) {
    if (read_oatmap_from_cachefile(oatmapcachepath, mapoatfile)) {
      create_maptable_for_oatfile(oatpath, start_addr, mapoatfile);
      harvest_oatinfo_from_oatmap(mapoatfile, oatinfo);
      return true;
    }
  }

  //
  // Cache lookup failed. Generate oatmap from scratch.
  //
  if (mapgenflav_ == OATMAPGEN_OATDUMP) {
    // Testing path
    if (!generate_oatmap_external(oatpath, oatmapcachepath)) {
      // generation failed
      return false;
    }
    if (!read_oatmap_from_cachefile(oatmapcachepath, mapoatfile)) {
      return false;
    }
  } else {
    // regular path
    DEBUGLOG(("invoking genmap_for_oat(%s)", oatpath));
    if (!genmap_for_oat(oatpath, mapoatfile)) {
      // generation failed
      return false;
    }
    // update cache
    if (!emit_oatmap_to_file(oatmapcachepath, mapoatfile)) {
      return false;
    }
  }

  // Success.
  create_maptable_for_oatfile(oatpath, start_addr, mapoatfile);
  harvest_oatinfo_from_oatmap(mapoatfile, oatinfo);

  return true;
}

#if 0
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
#endif

void OatMapperImpl::postprocess_encoded_profile(AndroidPerfProfile &prof)
{
  assert(false);

  //
  // Examine each of the load modules to determine if it is an OAT
  // file (as a side efect, this will update the LoadModule with sha1
  // checksum info).
  //
  std::set<int> oat_ids;
  for (int i = 0; i < prof.load_modules_size(); ++i) {
    auto *lm = prof.mutable_load_modules(i);
    if (examine_potential_oatfile(*lm)) {
      oat_ids.insert(i);
    }
  }
  if (! oat_ids.size())
    return;

  //
  // Walk the encoded profile and rewrite the sampled addresses,
  // converting raw IP values into virtual DEX locations.
  //
  // NB: may want to revisit how this loop works at some point
  // depending on what the strategy is for keeping OAT mapping files
  // in memory-- if we were to limit the number of mapping files
  // resident (so as to lower total memory high-water mark) then it
  // might make sense to try to build up a work list of addresses to
  // process in each module. For now, however, just use the simplest
  // ordering.
  //
  for (int i = 0; i < prof.programs_size(); ++i) {
    auto &prog = *prof.mutable_programs(i);
    for (int j = 0; j < prog.modules_size(); ++j) {
      auto &lms = *prog.mutable_modules(j);
      int load_module_id = lms.load_module_id();
      for (int k = 0; k < lms.address_samples_size(); ++k) {
        auto *as = lms.mutable_address_samples(k);
        bool callchain = (as->address_size() > 1);
        for (int f = 0; f < as->address_size(); ++f) {
          int frame_load_module_id =
              (callchain ? as->load_module_id(f) : load_module_id);
          if (oat_ids.find(frame_load_module_id) != oat_ids.end()) {
            auto &flm = prof.load_modules(frame_load_module_id);
            uint64_t encoded_addr = encode_addr(flm.name().c_str(),
                                                OAT_ADDR_METHOD,
                                                as->address(f));
            DEBUGLOG(("%s 0x%" PRIx64 " encoded to 0x%" PRIx64 " ",
                      flm.name().c_str(), as->address(f)));
            as->set_address(f, encoded_addr);
          }
        }
      }
    }
  }
}

class IsOatVisitor : public OatDexVisitor {
 public:
  IsOatVisitor() : base_text_(0) { }
  virtual void visitOAT(bool is64bit,
                        uint32_t checksum,
                        uint64_t executable_offset,
                        uint64_t base_text) {
    DEBUGLOG(("is oat: %s-bit base_text 0x%" PRIx64 " exec_offset "
              "0x%" PRIx64 " checksum %d", is64bit ? "64" : "32",
              base_text, executable_offset, checksum));
    base_text_ = base_text;
  }
  bool doVisitDex() { return false; } // don't visit DEX files
  uint64_t base_text() const { return base_text_; }
 private:
  uint64_t base_text_;
};


bool OatMapperImpl::examine_potential_oatfile(LoadModule &loadmodule)
{
  const char *loadmodulepath = loadmodule.name().c_str();
  assert(loadmodulepath);

  DEBUGLOG(("invoking examineOatFile(%s)", loadmodulepath));

  // Is this an OAT file?
  IsOatVisitor visitor;
  if (examineOatFile(loadmodulepath, visitor)) {
    uint64_t base_text = visitor.base_text();
    std::string oatmapcachepath = cachepath(loadmodulepath);
    OatFileInfo oatinfo;
    if (collect_oatfile_checksums(loadmodulepath,
                                  base_text,
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
  if (encoding == OAT_ADDR_METHOD) {
    std::string lm(loadmodulepath);
    MapTableIterator it = mappers_.find(lm);
    uint64_t encoded = 0;
    if (it == mappers_.end()) {
      return 0;
    }
    if (!it->second->lookup_ip(addr, encoded)) {
      return 0;
    }
    return encoded;
  } else if (encoding == OAT_ADDR_RAW) {
    // For testing purposes only
    return addr;
  } else {
    assert(false && "unknown/unsupported encoding");
    return 0;
  }
}

//======================================================================

OatMapper::OatMapper(const char *cachedir, OatMapGenFlavor mapgenflav)
    : impl_(new OatMapperImpl(cachedir, mapgenflav))
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
                                          uint64_t start_addr,
                                          OatFileInfo &oatinfo)
{
  return impl_->collect_oatfile_checksums(oatpath, start_addr, oatinfo);
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
