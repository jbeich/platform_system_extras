#ifndef SIMPLEPERF_COMMAND_H_
#define SIMPLEPERF_COMMAND_H_

#include <string>
#include <vector>

class Command;

class CommandCollection {
 public:
  static void RegisterCommand(Command& command);

  static Command* FindCommand(const std::string& cmd_name);
  static Command* FindCommand(const char* cmd_name);

  static const std::vector<Command*>& AllCommands();
};

class Command {
 public:
  Command(const std::string& name, const std::string& short_help_info,
          const std::string& detailed_help_info)
    :name(name), short_help_info(short_help_info), detailed_help_info(detailed_help_info) {
    CommandCollection::RegisterCommand(*this);
  }

  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

  virtual ~Command() {}

  const char* Name() const { return name.c_str(); }
  const char* ShortHelpInfo() const { return short_help_info.c_str(); }
  const char* DetailedHelpInfo() const { return detailed_help_info.c_str(); }

  virtual bool RunCommand(std::vector<std::string>& args) = 0;

 private:
  const std::string name;
  const std::string short_help_info;
  const std::string detailed_help_info;
};

#endif  // SIMPLEPERF_COMMAND_H_
