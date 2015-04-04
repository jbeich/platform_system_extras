#ifndef SIMPLE_PERF_WORKLOAD_H_
#define SIMPLE_PERF_WORKLOAD_H_

#include <sys/types.h>
#include <memory>
#include <string>
#include <vector>

class WorkLoad {
 public:
  static std::unique_ptr<WorkLoad> CreateWorkLoadInNewProcess(std::vector<std::string>& args);
  static std::unique_ptr<WorkLoad> CreateWorkLoadInCurrentProcess(void (*work_fn)());

  virtual ~WorkLoad() { }
  WorkLoad(const WorkLoad&) = delete;
  WorkLoad& operator=(const WorkLoad&) = delete;

  virtual pid_t GetWorkProcess() = 0;
  virtual bool Finished() = 0;
  virtual bool Start() = 0;
  virtual bool UseExec() = 0;
  virtual bool WaitFinish() = 0;

 protected:
  WorkLoad() { }
};

#endif  // SIMPLE_PERF_WORKLOAD_H_
