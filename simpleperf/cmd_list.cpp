#include <stdio.h>
#include <string>
#include <vector>

#include "command.h"
#include "event.h"

class ListCommand : public Command {
 public:
  ListCommand()
    : Command("list",
              "list all available perf events",
              "Usage: simpleperf list\n"
              "    List all available perf events on this machine.\n") { }

  bool RunCommand(std::vector<std::string>& args) override {
    if (args.size() != 0) {
      return false;
    }

    PrintHardwareEvents();
    PrintSoftwareEvents();
    PrintHwcacheEvents();
    return true;
  }

 private:
  void PrintHardwareEvents() {
    printf("List of hardware events:\n");
    for (auto event : Event::HardwareEvents()) {
      if (event->Supported()) {
        printf("  %s\n", event->Name());
      }
    }
    printf("\n");
  }

  void PrintSoftwareEvents() {
    printf("List of software events:\n");
    for (auto event : Event::SoftwareEvents()) {
      if (event->Supported()) {
        printf("  %s\n", event->Name());
      }
    }
    printf("\n");
  }

  void PrintHwcacheEvents() {
    printf("List of hw-cache events:\n");
    for (auto event : Event::HwcacheEvents()) {
      if (event->Supported()) {
        printf("  %s\n", event->Name());
      }
    }
  }
};

ListCommand list_cmd;


