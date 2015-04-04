#ifndef SIMPLEPERF_EVENT_H_
#define SIMPLEPERF_EVENT_H_

#include <stdint.h>
#include <string>
#include <vector>

class Event {
 private:
  enum class SupportState {
    UnChecked,
    Supported,
    UnSupported,
  };

 public:
  Event(const char* name, uint32_t type, uint64_t config)
    : name(name), type(type), config(config), support_state(SupportState::UnChecked) { }

  virtual ~Event() {}

  const char* Name() const {
    return name.c_str();
  };

  uint32_t Type() const {
    return type;
  }

  uint64_t Config() const {
    return config;
  }

  bool Supported() const;

  static const Event* FindEventByName(const std::string& name) {
    return FindEventByName(name.c_str());
  }

  static const Event* FindEventByName(const char* name);
  static const Event* FindEventByConfig(uint32_t type, uint64_t config);

  static const std::vector<const Event*>& HardwareEvents();
  static const std::vector<const Event*>& SoftwareEvents();
  static const std::vector<const Event*>& HwcacheEvents();

 private:
  bool CheckSupport() const;

  const std::string name;
  const uint32_t type;
  const uint64_t config;
  mutable SupportState support_state;
};

#endif  // SIMPLEPERF_EVENT_H_
