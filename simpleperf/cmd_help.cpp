#include "command.h"

class HelpCmd : public Command {
 public:
  HelpCmd()
    : Command("help",
              "print help information for simpleperf",
              "Usage: simpleperf help [subcommand]\n"
              "    Without subcommand, print brief help information for every subcommand.\n"
              "    With subcommand, print detailed help information for the subcommand.\n\n"
              ) {
  }

  bool RunCommand(std::vector<std::string>& args) override {
    if (args.size() == 0) {
      PrintShortHelp();
    } else {
      Command* need_help_command = CommandCollection::FindCommand(args[0]);
      if (need_help_command == nullptr) {
        PrintShortHelp();
        return false;
      } else {
        PrintDetailedHelp(*need_help_command);
      }
    }
    return true;
  }

  void PrintShortHelp() {
    printf("Usage: simpleperf [--help] subcommand [args_for_subcommand]\n\n");
    for (auto command : CommandCollection::AllCommands()) {
      printf("%-20s%s\n", command->Name(), command->ShortHelpInfo());
    }
  }

  void PrintDetailedHelp(const Command& command) {
    printf("%s\n", command.DetailedHelpInfo());
  }
};

HelpCmd helpcmd;

