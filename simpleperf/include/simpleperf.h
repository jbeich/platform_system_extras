#ifndef _SIMPLEPERF_H
#define _SIMPLEPERF_H

#include <sys/types.h>

#include <string>
#include <vector>

#ifndef SIMPLEPERF_EXPORT
#define SIMPLEPERF_EXPORT
#endif

namespace simpleperf {

std::vector<std::string> GetAllEvents() SIMPLEPERF_EXPORT;
bool IsEventSupported(const std::string& name) SIMPLEPERF_EXPORT;

struct Counter {
  std::string event;
  uint64_t value;
  // If there is not enough hardware counters, kernel will share counters between events.
  // time_enabled_in_ns is the period when counting is enabled, and time_running_in_ns is
  // the period when counting really happens in hardware.
  uint64_t time_enabled_in_ns;
  uint64_t time_running_in_ns;
};

class SIMPLEPERF_EXPORT PerfEventSet {
 public:
  // A PerfEventSet instance can only be used for one purpose, either counting or recording.
  static PerfEventSet* CreateInstance(bool for_counting);
  virtual ~PerfEventSet() {}

  // Add event in the set. All valid events are returned by GetAllEvents().
  // To only monitor events happen in user space, add :u suffix, like cpu-cycles:u.
  virtual bool AddEvent(const std::string& name);

  // Set monitored target. You can only monitor threads in current process.
  virtual bool MonitorCurrentProcess();
  virtual bool MonitorCurrentThread();
  virtual bool MonitorThreadsInCurrentProcess(const std::vector<int>& threads);

  // Counting interface:
  // User can start counting events, stop counting events and read counters many times.
  // There is no need to stop counting before read counters.
  // When reading counters, the counter values are the accumulated values of all counting periods.
  // After finish counting, the resources are released, and you should not call any function
  // in PerfEventSet.
  virtual bool StartCounting();
  virtual bool StopCounting();
  virtual bool ReadCounters(std::vector<Counter>* counters);
  virtual bool FinishCounting();

 protected:
  PerfEventSet() {}
};

}  // namespace simpleperf

#undef SIMPLEPERF_EXPORT

#endif  // _SIMPLEPERF_H
