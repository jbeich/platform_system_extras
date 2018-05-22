#define LOG_TAG "TC_EXTRAS"

#include <signal.h>
#include <errno.h>
#include <log/log.h>

extern "C" {

int __llvm_profile_write_file(void);
void __llvm_profile_reset_counters(void);
void __llvm_profile_set_filename(const char *);

sighandler_t old_sigusr1_handler = SIG_IGN;
sighandler_t old_sigusr2_handler = SIG_IGN;

// Signal handler:
//   - zero out counters for SIGUSR1
//   - write profile file and then zero out counters for SIGUSR2
void profile_signal_handler(int signum) {
  ALOGV("Entering signal handler");
  if (signum == SIGUSR1) {
    ALOGV("resetting counters");
    __llvm_profile_reset_counters();
  }
  else if (signum == SIGUSR2) {
    // write counters and zero out
    ALOGV("writing log");
    __llvm_profile_write_file();
    ALOGV("resetting counters");
    __llvm_profile_reset_counters();
  }
  ALOGV("calling prior signal handlers");
  if (signum == SIGUSR1 && old_sigusr1_handler != SIG_IGN && old_sigusr1_handler != SIG_DFL)
    old_sigusr1_handler(signum);
  if (signum == SIGUSR2 && old_sigusr2_handler != SIG_IGN && old_sigusr2_handler != SIG_DFL)
    old_sigusr2_handler(signum);
  ALOGV("exitting signal handler");
}

int init_toolchain_extras(void) {
  __llvm_profile_set_filename("/data/local/traces/default_%m.profraw");
  ALOGV("setting signal handlers");

  sighandler_t ret1 = signal(SIGUSR1, profile_signal_handler);
  if (ret1 == SIG_ERR) {
    ALOGE("setting signal handler for sigusr1 failed %d\n", errno);
  }
  else {
    old_sigusr1_handler = ret1;
  }
  sighandler_t ret2 = signal(SIGUSR2, profile_signal_handler);
  if (ret2 == SIG_ERR) {
    ALOGE("setting signal handler for sigusr2 failed %d\n", errno);
  }
  else {
    old_sigusr2_handler = ret1;
  }
  return 0;
}

int __toolchain_extras = init_toolchain_extras();
}
