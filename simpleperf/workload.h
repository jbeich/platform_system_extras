/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
