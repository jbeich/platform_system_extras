#pragma once

#include <inttypes.h>

#include <map>
#include <memory>

#include "event_type.h"
#include "perf_event.h"

// Functions recording Coresight ETM data on ARM device.

namespace simpleperf {

struct ETMPerCpu {
  uint32_t trcidr0;
  uint32_t trcidr1;
  uint32_t trcidr2;
  uint32_t trcidr8;
  uint32_t trcidr9;
  uint32_t trctraceid;

  int GetMajorVersion() const;
  bool IsContextIDSupported() const;
  bool IsTimestampSupported() const;
};

// Help recording Coresight ETM data on ARM devices.
// 1. Get etm event type on device.
// 2. Get sink config, which selects the ETR device moving etm data to memory.
// 3. Get etm info on each cpu.
// The etm event type and sink config are used to build perf_event_attr for etm data tracing.
// The etm info are kept in perf.data to help etm decoding.
class ETMRecorder {
 public:
  static ETMRecorder& GetInstance();

  // If not found, return -1.
  int GetEtmEventType();
  std::unique_ptr<EventType> BuildEventType();
  bool CheckEtmSupport();
  void SetEtmPerfEventAttr(perf_event_attr* attr);

 private:
  bool ReadEtmInfo();
  bool FindSinkConfig();

  int event_type_ = 0;
  bool etm_supported_ = false;
  uint32_t sink_config_ = 0;
  std::map<int, ETMPerCpu> etm_info_;
};

} // namespace simpleperf