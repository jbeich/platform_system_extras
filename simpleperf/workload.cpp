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

#include <base/logging.h>

std::unique_ptr<Workload> Workload::CreateWorkload(const std::vector<std::string>& args) {
  std::unique_ptr<Workload> workload(new Workload(args));
  if (workload != nullptr && workload->CreateNewProcess()) {
    return workload;
  }
  return nullptr;
}

static void ChildProcessFn(std::vector<std::string>& args, int start_signal_fd, int exec_child_fd);

bool Workload::CreateNewProcess() {
  CHECK(work_state_ == NotYetCreateNewProcess);

  int start_signal_pipe[2];
  if (pipe2(start_signal_pipe, O_CLOEXEC) != 0) {
    PLOG(ERROR) << "pipe2() failed";
    return false;
  }

  int exec_child_pipe[2];
  if (pipe2(exec_child_pipe, O_CLOEXEC) != 0) {
    PLOG(ERROR) << "pipe2() failed";
    close(start_signal_pipe[0]);
    close(start_signal_pipe[1]);
    return false;
  }

  pid_t pid = fork();
  if (pid == -1) {
    PLOG(ERROR) << "fork() failed";
    close(start_signal_pipe[0]);
    close(start_signal_pipe[1]);
    close(exec_child_pipe[0]);
    close(exec_child_pipe[1]);
    return false;
  } else if (pid == 0) {
    // In child process.
    close(start_signal_pipe[1]);
    close(exec_child_pipe[0]);
    ChildProcessFn(args_, start_signal_pipe[0], exec_child_pipe[1]);
  }
  // In parent process.
  close(start_signal_pipe[0]);
  close(exec_child_pipe[1]);
  start_signal_fd_ = start_signal_pipe[1];
  exec_child_fd_ = exec_child_pipe[0];
  work_pid_ = pid;
  work_state_ = NotYetStartNewProcess;
  return true;
}

static void ChildProcessFn(std::vector<std::string>& args, int start_signal_fd, int exec_child_fd) {
  std::vector<char*> argv(args.size() + 1);
  for (size_t i = 0; i < args.size(); ++i) {
    argv[i] = &args[i][0];
  }
  argv[args.size()] = nullptr;

  char start_signal = 0;
  ssize_t nread = TEMP_FAILURE_RETRY(read(start_signal_fd, &start_signal, 1));
  if (nread == 1 && start_signal == 1) {
    close(start_signal_fd);
    execvp(argv[0], argv.data());
    // If execvp() succeed, we will not arrive here. But if it failed, we need to
    // report the failure to the parent process by writing 1 to exec_child_fd.
    int saved_errno = errno;
    char exec_child_failed = 1;
    TEMP_FAILURE_RETRY(write(exec_child_fd, &exec_child_failed, 1));
    close(exec_child_fd);
    errno = saved_errno;
    PLOG(FATAL) << "execvp() failed";
  } else {
    PLOG(FATAL) << "child process failed to receive start_signal";
  }
}

bool Workload::Start() {
  CHECK(work_state_ == NotYetStartNewProcess);
  char start_signal = 1;
  ssize_t nwrite = TEMP_FAILURE_RETRY(write(start_signal_fd_, &start_signal, 1));
  if (nwrite != 1) {
    PLOG(ERROR) << "write start signal failed";
    return false;
  }
  char exec_child_failed;
  ssize_t nread = TEMP_FAILURE_RETRY(read(exec_child_fd_, &exec_child_failed, 1));
  if (nread != 0) {
    PLOG(ERROR) << "exec child failed";
    return false;
  }
  work_state_ = Started;
  return true;
}

static void CheckProcessExitStatus(int status) {
  if (WIFSIGNALED(status)) {
    PLOG(ERROR) << "work process was terminated by signal " << strsignal(WTERMSIG(status));
  } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    PLOG(ERROR) << "work process exited with exit code " << WEXITSTATUS(status);
  }
}

bool Workload::IsFinished() {
  if (work_state_ == Started) {
    int status;
    pid_t result = TEMP_FAILURE_RETRY(waitpid(work_pid_, &status, WNOHANG));
    if (result == work_pid_) {
      work_state_ = Finished;
      CheckProcessExitStatus(status);
    } else if (result == -1) {
      PLOG(FATAL) << "waitpid() failed";
    }
  }
  return work_state_ == Finished;
}

void Workload::WaitFinish() {
  CHECK(work_state_ == Started || work_state_ == Finished);
  while (!IsFinished()) {
    int status;
    pid_t result = TEMP_FAILURE_RETRY(waitpid(work_pid_, &status, 0));
    if (result == work_pid_) {
      work_state_ = Finished;
      CheckProcessExitStatus(status);
    } else {
      PLOG(FATAL) << "waitpid() failed";
    }
  }
}
