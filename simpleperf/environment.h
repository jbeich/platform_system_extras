#ifndef SIMPLEPERF_ENVIRONMENT_H_
#define SIMPLEPERF_ENVIRONMENT_H_

#include <sys/types.h>
#include <vector>

class Environment {
 public:
  static std::vector<int> GetOnlineCpus();
  static int64_t NanoTime();
};

#endif  // SIMPLEPERF_ENVIRONMENT_H_
