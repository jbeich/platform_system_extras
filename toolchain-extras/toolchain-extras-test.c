#include <android/log.h>
#include <unistd.h>

int main() {
  __android_log_print(ANDROID_LOG_ERROR, "PGO_CAM", "pid is %d\n", getpid());
  sleep(20);
  __android_log_print(ANDROID_LOG_ERROR, "PGO_CAM", "returning from main");
  return 0;
}
