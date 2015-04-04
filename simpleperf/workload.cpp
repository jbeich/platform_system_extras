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

#include "workload.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>

class WorkLoadInCurrentProcess : public WorkLoad {
 public:
  WorkLoadInCurrentProcess(void (*work_fn)()) : work_fn(work_fn) {
    pid = getpid();
    finished = false;
  }

  bool Finished() override {
    return finished;
  }

  pid_t GetWorkProcess() {
    return pid;
  }

  bool Start() override {
    return true;
  }

  bool UseExec() override {
    return false;
  }

  bool WaitFinish() override {
    (*work_fn)();
    finished = true;
    return true;
  }

 private:
  void (*work_fn)();
  pid_t pid;
  bool finished;
};

class WorkLoadInNewProcess : public WorkLoad {
 public:
  WorkLoadInNewProcess(std::vector<std::string>& args) {
    has_error = false;
    finished = false;
    CreateProcess(args);
  }

  bool Finished() override {
    if (!finished) {
      int status;
      pid_t result = TEMP_FAILURE_RETRY(waitpid(pid, &status, WNOHANG));
      if (result == pid) {
        finished = true;
      } else if (result < 0) {
        perror("wait work process failed");
        finished = true;
      }
    }
    return finished;
  }

  pid_t GetWorkProcess() override {
    return pid;
  }

  bool Start() override {
    if (has_error) {
      fprintf(stderr, "WorloadInNewProcess, can't start because of CreateProcess error\n");
      return false;
    }
    char start_signal = 1;
    ssize_t nwrite = TEMP_FAILURE_RETRY(write(start_signal_fd, &start_signal, 1));
    if (nwrite != 1) {
      has_error = true;
      fprintf(stderr, "WorkloadInNewProcess, write start signal failed: %s\n", strerror(errno));
      return false;
    }
    close(start_signal_fd);
    return true;
  }

  bool WaitFinish() override {
    if (has_error) {
      return false;
    }
    if (!finished) {
      int status;
      pid_t ret = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
      if (ret == -1) {
        perror("WorkLoadInNewProcess:WaitFinish() waitpid failed");
        return false;
      }
      if (WIFSIGNALED(status)) {
        fprintf(stderr, "work process killed by signal %s\n", strsignal(WTERMSIG(status)));
        return false;
      } else if (WEXITSTATUS(status) != 0) {
        fprintf(stderr, "work process exit with code %d\n", WEXITSTATUS(status));
        return false;
      }
      finished = true;
    }
    return true;
  }

  bool UseExec() override {
    return true;
  }

 private:
  void CreateProcess(std::vector<std::string>& args) {
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) == -1) {
      perror("create pipe failed");
      has_error = true;
      return;
    }

    pid = fork();
    if (pid == -1) {
      perror("create new process failed");
      has_error = true;
    } else if (pid == 0) {
      // Child process, waiting start signal from pipe.
      close(pipefd[1]);

      std::vector<char*> exec_args(args.size() + 1);
      for (size_t i = 0; i < args.size(); ++i) {
        exec_args[i] = &args[i][0];
      }
      exec_args[args.size()] = nullptr;

      char start_signal = 0;
      ssize_t nread = TEMP_FAILURE_RETRY(read(pipefd[0], &start_signal, 1));
      if (nread == 1 && start_signal == 1) {
        execvp(args[0].c_str(), exec_args.data());
        perror("execvp() in new process failed");
        abort();
      }
      abort();
    } else {
      close(pipefd[0]);
      start_signal_fd = pipefd[1];
    }
  }

 private:
  std::vector<std::string> args;
  pid_t pid;
  int start_signal_fd;
  bool has_error;
  bool finished;
};

std::unique_ptr<WorkLoad> WorkLoad::CreateWorkLoadInNewProcess(std::vector<std::string>& args) {
  return std::unique_ptr<WorkLoad>(new WorkLoadInNewProcess(args));
}

std::unique_ptr<WorkLoad> WorkLoad::CreateWorkLoadInCurrentProcess(void (*work_fn)()) {
  return std::unique_ptr<WorkLoad>(new WorkLoadInCurrentProcess(work_fn));
}
