#include "simpleperf/simpleperf.h"

#include <ctype.h>
#include <string>
#include <vector>

#include "command.h"

#include "cmd_help.cpp"
#include "cmd_list.cpp"
#include "cmd_record.cpp"
#include "cmd_stat.cpp"

static std::vector<std::string> ConvertArgs(const char* cmd_string) {
  std::vector<std::string> args;
  std::string arg;

  for (const char* p = cmd_string; *p != '\0'; ++p) {
    if (isspace(*p)) {
      if (arg.size() > 0) {
        args.push_back(arg);
        arg.clear();
      }
    } else {
      arg.push_back(*p);
    }
  }
  if (arg.size() > 0) {
    args.push_back(arg);
  }
  return args;
}

namespace simpleperf {

bool execute(const char* cmd_string) {
  auto args = ConvertArgs(cmd_string);
  if (args.size() == 0) {
    return false;
  } else {
    Command* command = CommandCollection::FindCommand(args[0]);
    if (command == nullptr) {
      fprintf(stderr, "Invalid command: %s\n", args[0].c_str());
      return false;
    }
    args.erase(args.begin());
    return command->RunCommand(args);
  }
}

}  // namespace simpleperf
