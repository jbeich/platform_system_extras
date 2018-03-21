
#include "perf_data_converter.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/strings.h>
#include <perf_parser.h>
#include <perf_protobuf_io.h>

#include "perfprofd_record.pb.h"
#include "perf_data.pb.h"

#include "map_utils.h"
#include "quipper_helper.h"
#include "symbolizer.h"

using std::map;

namespace android {
namespace perfprofd {

namespace {

void AddSymbolInfo(PerfprofdRecord* record, ::perfprofd::Symbolizer* symbolizer) {
  std::unordered_set<std::string> filenames_w_build_id;
  for (auto& perf_build_id : record->perf_data().build_ids()) {
    filenames_w_build_id.insert(perf_build_id.filename());
  }

  // Map of mmap events with filenames without build id. Key is the start address.
  std::map<uint64_t, const ::quipper::PerfDataProto_MMapEvent*> mmap_table;
  {
    quipper::MmapEventIterator it(record->perf_data());
    for (; it != it.end(); ++it) {
      const ::quipper::PerfDataProto_MMapEvent* mmap_event = &it->mmap_event();
      if (!mmap_event->has_filename() || !mmap_event->has_start() || !mmap_event->has_len()) {
        // Don't care.
        continue;
      }
      if (filenames_w_build_id.count(mmap_event->filename()) == 0) {
        mmap_table.emplace(mmap_event->start(), mmap_event);
      }
    }
  }
  if (mmap_table.empty()) {
    return;
  }

  struct Dso {
    uint64_t min_vaddr;
    RangeMap<std::string, uint64_t> symbols;
    explicit Dso(uint64_t min_vaddr_in) : min_vaddr(min_vaddr_in) {
    }
  };
  std::unordered_map<std::string, Dso> files;

  quipper::SampleEventIterator it(record->perf_data());
  for (; it != it.end(); ++it) {
    const ::quipper::PerfDataProto_SampleEvent& sample_event = it->sample_event();
    auto check_address = [&](uint64_t addr) {
      auto mmap_it = GetLeqIterator(mmap_table, addr);
      if (mmap_it == mmap_table.end()) {
        return;
      }
      const ::quipper::PerfDataProto_MMapEvent& mmap = *mmap_it->second;
      if (addr >= mmap.start() + mmap.len()) {
        return;
      }

      // OK, that's a hit in the mmap segment (w/o build id).

      Dso* dso;
      {
        auto dso_it = files.find(mmap.filename());
        constexpr uint64_t kNoMinAddr = std::numeric_limits<uint64_t>::max();
        if (dso_it == files.end()) {
          uint64_t min_vaddr;
          bool has_min_vaddr = symbolizer->GetMinExecutableVAddr(mmap.filename(), &min_vaddr);
          if (!has_min_vaddr) {
            min_vaddr = kNoMinAddr;
          }
          auto it = files.emplace(mmap.filename(), Dso(min_vaddr));
          dso = &it.first->second;
        } else {
          dso = &dso_it->second;
        }
        if (dso->min_vaddr == kNoMinAddr) {
          return;
        }
      }

      // Normally we'd expect "(mmap.has_pgoff() ? mmap.pgoff() : 0)" instead of min_vaddr.
      // However, relocation packing works better with this. (Copied from simpleperf.)
      const uint64_t file_addr = dso->min_vaddr + addr - mmap.start();

      std::string symbol = symbolizer->Decode(mmap.filename(), file_addr);
      if (symbol.empty()) {
        return;
      }

      dso->symbols.Insert(symbol, file_addr);
    };
    if (sample_event.has_ip()) {
      check_address(sample_event.ip());
    }
    for (uint64_t addr : sample_event.callchain()) {
      check_address(addr);
    }
  }

  if (!files.empty()) {
    // We have extra symbol info, create proto messages now.
    for (auto& file_data : files) {
      const std::string& filename = file_data.first;
      const Dso& dso = file_data.second;
      if (dso.symbols.empty()) {
        continue;
      }

      PerfprofdRecord_SymbolInfo* symbol_info = record->add_symbol_info();
      symbol_info->set_filename(filename);
      // symbol_info->set_filename_md5_prefix(0);
      symbol_info->set_min_vaddr(dso.min_vaddr);
      for (auto& aggr_sym : dso.symbols) {
        PerfprofdRecord_SymbolInfo_Symbol* symbol = symbol_info->add_symbols();
        symbol->set_addr(*aggr_sym.second.offsets.begin());
        symbol->set_size(*aggr_sym.second.offsets.rbegin() - *aggr_sym.second.offsets.begin() + 1);
        symbol->set_symbol_name(aggr_sym.second.symbol);
        // symbol->set_symbol_name_md5_prefix(0);
      }
    }
  }
}

}  // namespace

PerfprofdRecord*
RawPerfDataToAndroidPerfProfile(const string &perf_file,
                                ::perfprofd::Symbolizer* symbolizer) {
  std::unique_ptr<PerfprofdRecord> ret(new PerfprofdRecord());
  ret->set_id(0);  // TODO.

  ::quipper::PerfParserOptions options = {};
  options.do_remap = true;
  options.discard_unused_events = true;
  options.read_missing_buildids = true;

  ::quipper::PerfDataProto* perf_data = ret->mutable_perf_data();

  if (!::quipper::SerializeFromFileWithOptions(perf_file, options, perf_data)) {
    return nullptr;
  }

  // TODO: Symbolization.
  if (symbolizer != nullptr) {
    AddSymbolInfo(ret.get(), symbolizer);
  }

  return ret.release();
}

}  // namespace perfprofd
}  // namespace android
