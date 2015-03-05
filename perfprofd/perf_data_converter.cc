
#include "perf_data_converter.h"
#include "quipper/perf_parser.h"
#include <map>

using std::map;

namespace wireless_android_logging_awp {

struct RangeTarget {
  RangeTarget(uint64 start, uint64 end, uint64 to)
      : start(start), end(end), to(to) {}

  bool operator<(const RangeTarget &r) const {
    if (start != r.start) {
      return start < r.start;
    } else if (end != r.end) {
      return end < r.end;
    } else {
      return to < r.to;
    }
  }
  uint64 start;
  uint64 end;
  uint64 to;
};

struct BinaryProfile {
  map<uint64, uint64> address_count_map;
  map<RangeTarget, uint64> range_count_map;
};

std::pair<bool, wireless_android_play_playlog::AndroidPerfProfile>
RawPerfDataToAndroidPerfProfile(const std::string &perf_file) {
  wireless_android_play_playlog::AndroidPerfProfile ret;
  quipper::PerfParser parser;
  if (!parser.ReadFile(perf_file) || !parser.ParseRawEvents()) {
    return std::make_pair(false, ret);
  }

  map<string, BinaryProfile> name_profile_map;
  uint64 total_samples = 0;
  for (const auto &event : parser.parsed_events()) {
    if (!event.raw_event ||
        event.raw_event->header.type != PERF_RECORD_SAMPLE) {
      continue;
    }
    const string &name = event.dso_and_offset.dso_name();
    name_profile_map[name].address_count_map[event.dso_and_offset.offset()]++;
    total_samples++;
    for (unsigned i = 1; i < event.branch_stack.size(); i++) {
      if (name == event.branch_stack[i - 1].to.dso_name()) {
        uint64 start = event.branch_stack[i].to.offset();
        uint64 end = event.branch_stack[i - 1].from.offset();
        uint64 to = event.branch_stack[i - 1].to.offset();
        // The interval between two taken branches should not be too large.
        if (end < start || end - start > (1 << 20)) {
          LOG(WARNING) << "Bogus LBR data: " << start << "->" << end;
          continue;
        }
        name_profile_map[name].range_count_map[RangeTarget(start, end, to)]++;
      }
    }
  }

  map<string, string> name_buildid_map;
  parser.GetFilenamesToBuildIDs(&name_buildid_map);
  ret.set_total_samples(total_samples);
  for (const auto &name_profile : name_profile_map) {
    auto load_module_samples = ret.add_load_module_samples();
    load_module_samples->set_name(name_profile.first);
    if (name_profile.first == "[kernel.kallsyms]_text") {
      load_module_samples->set_is_kernel(true);
    }
    auto nbmi = name_buildid_map.find(name_profile.first);
    if (nbmi != name_buildid_map.end()) {
      const std::string &build_id = nbmi->second;
      if (build_id.size() == 40 && build_id.substr(32) == "00000000") {
        load_module_samples->set_build_id(build_id.substr(0, 32));
      } else {
        load_module_samples->set_build_id(build_id);
      }
    }
    for (const auto &addr_count : name_profile.second.address_count_map) {
      auto address_samples = load_module_samples->add_address_samples();
      address_samples->add_address(addr_count.first);
      address_samples->set_count(addr_count.second);
    }
    for (const auto &range_count : name_profile.second.range_count_map) {
      auto range_samples = load_module_samples->add_range_samples();
      range_samples->set_start(range_count.first.start);
      range_samples->set_end(range_count.first.end);
      range_samples->set_to(range_count.first.to);
      range_samples->set_count(range_count.second);
    }
  }
  return std::make_pair(true, ret);
}

}  // namespace wireless_android_logging_awp
