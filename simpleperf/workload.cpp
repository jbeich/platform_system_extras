/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "workload.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <thread>

#include <base/logging.h>

using namespace std::chrono;

class WorkloadOfSleep : public Workload {
 private:
  enum WorkState {
    NotYetStarted,
    Started,
    Finished,
  };

 public:
  WorkloadOfSleep(seconds sleep_duration)
      : work_state_(NotYetStarted), sleep_duration_(sleep_duration) {
  }

  bool Start() override;
  bool IsFinished() override;
  bool WaitFinish() override;

  pid_t GetWorkPid() override {
    return getpid();
  }

 private:
  bool CheckFinish();

  WorkState work_state_;
  seconds sleep_duration_;
  steady_clock::time_point start_time_;
};

bool WorkloadOfSleep::Start() {
  if (work_state_ != NotYetStarted) {
    return false;
  }
  work_state_ = Started;
  start_time_ = steady_clock::now();
  return true;
}

bool WorkloadOfSleep::IsFinished() {
  if (work_state_ == Started) {
    if (CheckFinish()) {
      work_state_ = Finished;
    }
  }
  return work_state_ == Finished;
}

bool WorkloadOfSleep::CheckFinish() {
  return steady_clock::now() >= start_time_ + sleep_duration_;
}

bool WorkloadOfSleep::WaitFinish() {
  if (work_state_ != Started && work_state_ != Finished) {
    return false;
  }
  while (!IsFinished()) {
    std::this_thread::sleep_until(start_time_ + sleep_duration_);
  }
  return true;
}

class WorkloadInNewProcess : public Workload {
 private:
  enum WorkState {
    NotYetCreateNewProcess,
    NotYetStartNewProcess,
    Started,
    Finished,
  };

 public:
  WorkloadInNewProcess(const std::vector<std::string>& args)
      : work_state_(NotYetCreateNewProcess), args_(args), start_signal_fd_(-1), work_pid_(-1) {
  }

  bool CreateNewProcess();
  bool Start() override;
  bool IsFinished() override;
  bool WaitFinish() override;
  pid_t GetWorkPid() override {
    return work_pid_;
  }

 private:
  void ChildProcessFn(int start_signal_fd);

  WorkState work_state_;
  std::vector<std::string> args_;
  int start_signal_fd_;
  pid_t work_pid_;
};

bool WorkloadInNewProcess::CreateNewProcess() {
  if (work_state_ != NotYetCreateNewProcess) {
    return false;
  }

  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC) != 0) {
    PLOG(ERROR) << "pipe2() failed";
    return false;
  }

  pid_t pid = fork();
  if (pid == -1) {
    PLOG(ERROR) << "fork() failed";
    return false;
  } else if (pid == 0) {
    // In child process.
    close(pipefd[1]);
    ChildProcessFn(pipefd[0]);
  }
  // In parent process.
  close(pipefd[0]);
  start_signal_fd_ = pipefd[1];
  work_pid_ = pid;
  work_state_ = NotYetStartNewProcess;
  return true;
}

void WorkloadInNewProcess::ChildProcessFn(int start_signal_fd) {
  std::vector<char*> argv(args_.size() + 1);
  for (size_t i = 0; i < args_.size(); ++i) {
    argv[i] = &args_[i][0];
  }
  argv[args_.size()] = nullptr;

  char start_signal = 0;
  ssize_t nread = TEMP_FAILURE_RETRY(read(start_signal_fd, &start_signal, 1));
  if (nread == 1 && start_signal == 1) {
    close(start_signal_fd);
    execvp(argv[0], argv.data());
    // If execvp() succeed, we shouldn't arrive here.
    PLOG(ERROR) << "execvp() failed";
  }
  abort();
}

bool WorkloadInNewProcess::Start() {
  if (work_state_ != NotYetStartNewProcess) {
    return false;
  }
  char start_signal = 1;
  ssize_t nwrite = TEMP_FAILURE_RETRY(write(start_signal_fd_, &start_signal, 1));
  if (nwrite != 1) {
    PLOG(ERROR) << "write start signal failed.";
    return false;
  }
  close(start_signal_fd_);
  work_state_ = Started;
  return true;
}

bool WorkloadInNewProcess::IsFinished() {
  if (work_state_ == Started) {
    int status;
    pid_t result = TEMP_FAILURE_RETRY(waitpid(work_pid_, &status, WNOHANG));
    if (result == work_pid_) {
      work_state_ = Finished;
      if (WIFSIGNALED(status)) {
        PLOG(ERROR) << "work process was terminated by signal " << strsignal(WTERMSIG(status));
      } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        PLOG(ERROR) << "work process exited with exitcode " << WEXITSTATUS(status);
      }
    }
  }
  return work_state_ == Finished;
}

bool WorkloadInNewProcess::WaitFinish() {
  if (work_state_ != Started && work_state_ != Finished) {
    return false;
  }
  while (!IsFinished()) {
    int status;
    pid_t result = TEMP_FAILURE_RETRY(waitpid(work_pid_, &status, 0));
    if (result == work_pid_) {
      work_state_ = Finished;
      if (WIFSIGNALED(status)) {
        PLOG(ERROR) << "work process was terminated by signal " << strsignal(WTERMSIG(status));
      } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        PLOG(ERROR) << "work process exited with exitcode " << WEXITSTATUS(status);
      }
    } else {
      PLOG(ERROR) << "waitpid() failed";
      return false;
    }
  }
  return true;
}

std::unique_ptr<Workload> Workload::CreateWorkloadOfSleep(seconds sleep_duration) {
  return std::unique_ptr<Workload>(new WorkloadOfSleep(sleep_duration));
}

std::unique_ptr<Workload> Workload::CreateWorkloadInNewProcess(const std::vector<std::string>& args) {
  WorkloadInNewProcess* workload = new WorkloadInNewProcess(args);
  if (workload != nullptr && workload->CreateNewProcess() == false) {
    delete workload;
    return nullptr;
  }
  return std::unique_ptr<Workload>(workload);
}
