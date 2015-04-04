#ifndef SIMPLE_PERF_EVENT_ATTR_H_
#define SIMPLE_PERF_EVENT_ATTR_H_

#include <linux/perf_event.h>
#include <stddef.h>
#include <stdint.h>

class Event;

class EventAttr {
 public:
  EventAttr(const Event* event, bool enabled = true);
  EventAttr(const perf_event_attr* attr);

  perf_event_attr* Attr() {
    return &attr;
  }

  const perf_event_attr* Attr() const {
    return &attr;
  }

  uint64_t SampleType() const {
    return attr.sample_type;
  }

  void EnableOnExec() {
    attr.enable_on_exec = 1;
  }

  void SetSampleFreq(int freq) {
    attr.freq = 1;
    attr.sample_freq = freq;
  }

  void SampleAll() {
    attr.sample_id_all = 1;
  }

  const Event& GetEvent() const {
    return *event;
  }

  void Print(size_t space_count = 0) const;

 private:
  void Init(uint32_t type, uint64_t config, bool enabled);

  perf_event_attr attr;
  const Event* event;
};

#endif  // SIMPLE_PERF_EVENT_ATTR_H_
