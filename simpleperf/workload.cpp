#include "workload.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
        perror("WorkLoadInNewProcess:WaitFinish() waitpid fails");
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
    if (pipe(pipefd) == -1) {
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

      char** exec_args = new char*[args.size() + 1];
      for (size_t i = 0; i < args.size(); ++i) {
        exec_args[i] = &args[i][0];
      }
      exec_args[args.size()] = nullptr;

      char start_signal = 0;
      ssize_t nread = TEMP_FAILURE_RETRY(read(pipefd[0], &start_signal, 1));
      if (nread == 1 && start_signal == 1) {
        close(pipefd[1]);
        execvp(args[0].c_str(), exec_args);
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
