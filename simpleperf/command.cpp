#include "command.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

static std::vector<Command*>& Commands() {
  static std::vector<Command*> commands;
  return commands;
}

static bool CompareCommands(Command* cmd1, Command* cmd2) {
  return strcmp(cmd1->Name(), cmd2->Name()) < 0;
}

void CommandCollection::RegisterCommand(Command& command) {
  Commands().push_back(&command);
  std::sort(Commands().begin(), Commands().end(), CompareCommands);
}

Command* CommandCollection::FindCommand(const std::string& cmd_name) {
  return FindCommand(cmd_name.c_str());
}

Command* CommandCollection::FindCommand(const char* cmd_name) {
  for (auto command : Commands()) {
    if (!strcmp(command->Name(), cmd_name)) {
      return command;
    }
  }
  return nullptr;
}

const std::vector<Command*>& CommandCollection::AllCommands() {
  return Commands();
}

