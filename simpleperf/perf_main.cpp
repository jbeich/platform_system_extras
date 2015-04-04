#include <simpleperf/simpleperf.h>

#include <string>

int main(int argc, char** argv) {
  std::string cmd_string;

  if (argc == 1 || (argc == 2 && strcmp(argv[1], "--help") == 0)) {
    cmd_string = "help";
  } else {
    for (int i = 1; i < argc; ++i) {
      if (i != 1) {
        cmd_string += " ";
      }
      cmd_string += argv[i];
    }
  }

  return simpleperf::execute(cmd_string.c_str()) ? 0 : 1;
}
