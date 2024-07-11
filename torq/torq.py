import argparse
from command import ProfilerCommand, HWCommand, ConfigCommand
from validation_error import ValidationError

DEFAULT_BETWEEN_DUR_MS = 10000


def create_parser():
  parser = argparse.ArgumentParser(prog='torq command',
                                   description=('Torq CLI tool for performance'
                                                ' tests.'))
  # --event, --profiler, --dur-ms, --runs, --simpleperf-event,
  # --perfetto-config, --between-dur-ms, and --ui are done similarly
  parser.add_argument('-e', '--event',
                      choices=['boot', 'user-switch', 'app-startup', 'custom'],
                      default='custom', help='The event to trace/profile.')
  parser.add_argument('-p', '--profiler', choices=['perfetto', 'simpleperf'],
                      default='perfetto', help='The performance data source.')
  parser.add_argument('-o', '--out-dir', default='.',
                      help='The path to the output directory.')
  parser.add_argument('-d', '--dur-ms', type=int,
                      help=('The duration (ms) of the event. Determines when'
                            ' to stop collecting performance data.'))
  parser.add_argument('-a', '--app',
                      help='The package name of the app we want to start.')
  parser.add_argument('-r', '--runs', type=int, default=1,
                      help=('The number of times to run the event and'
                            ' capture the perf data.'))
  parser.add_argument('-s', '--simpleperf-event', action='append',
                      choices=['cpu-cycles', 'instructions'],
                      help=('Simpleperf supported events to be collected.'
                            ' e.g. cpu-cycles, instructions'))
  parser.add_argument('--perfetto-config', default='default',
                      help=('Filepath of the perfetto config to use '
                            ' predefined configs.'))
  parser.add_argument('--between-dur-ms', type=int,
                      default=DEFAULT_BETWEEN_DUR_MS,
                      help='Time (ms) to wait before executing the next event.')
  parser.add_argument('--ui', choices=['true', 'false'],
                      help=('Specifies opening of UI visualization tool'
                            ' after profiling is complete.'))
  # --exclude-frtace-event, --include-ftrace-event use same approach,
  # --to, and --from are also done similarly
  parser.add_argument('--exclude-ftrace-event',
                      help=('Excludes the ftrace event from perfetto'
                            ' config even.'))
  parser.add_argument('--include-ftrace-event',
                      help='Includes the ftrace event from perfetto config.')
  parser.add_argument('--from-user', type=int,
                      help='The user id from which to start the user switch')
  parser.add_argument('--to-user', type=int,
                      help='The user id of user that system is switching to.')
  # hw and config subcommands are handled similarly
  # Create object to hold subcomand parsers of torq
  subparsers = parser.add_subparsers(dest='subcommands', help='Subcommands')
  # Create nested parser object for hw subcommand
  hw_parser = subparsers.add_parser('hw',
                                    help=('The hardware subcommand used to'
                                          ' change the H/W configuration of'
                                          ' the device.'))
  # Create nested object to hold subcommand parsers of hw
  hw_subparsers = hw_parser.add_subparsers(dest='hw_subcommand',
                                           help='torq hw subcommands')
  # Create nested parser object for hw set, hw get,
  # and hw list are done similarly
  hw_set_parser = hw_subparsers.add_parser('set',
                                           help=('Command to set a new'
                                                 ' hardware configuration'))
  hw_set_parser.add_argument('config', nargs='?',
                             choices=['seahawk', 'seaturtle'],
                             help='Pre-defined hardware configuration')
  hw_set_parser.add_argument('-n', '--num-cpus', type=int,
                             help='The amount of active cores in the hardware.')
  hw_set_parser.add_argument('-m', '--memory',
                             help=('The memory limit the device would have.'
                                   ' E.g. 4G'))
  # hw get
  hw_subparsers.add_parser('get',
                           help=('Command to get the current hardware'
                                 ' configuration. Will provide the number of'
                                 ' cpus and memory available.'))
  # hw list
  hw_subparsers.add_parser('list',
                           help=('Command to list the supported HW'
                                 ' configurations.'))
  # config subcomand
  config_parser = subparsers.add_parser('config',
                                        help=('The config subcommand used'
                                              ' to list and show the'
                                              ' predefined perfetto configs.'))
  config_subparsers = config_parser.add_subparsers(dest='config_subcommand',
                                                   help=('torq config'
                                                         ' subcommands'))
  # config list
  config_subparsers.add_parser('list',
                               help=('Command to list the predefined'
                                     ' perfetto configs'))
  # config show
  config_show_parser = config_subparsers.add_parser('show',
                                                    help=('Command to print'
                                                          ' the '
                                                          ' perfetto config'
                                                          ' in the terminal.'))
  config_show_parser.add_argument('config_name',
                                  choices=['lightweight', 'default', 'memory'],
                                  help=('Name of the predefined confetto'
                                        ' config to print.'))
  # config pull
  config_pull_parser = config_subparsers.add_parser('pull',
                                                    help=('Command to copy'
                                                          ' a predefined config'
                                                          ' to the specified'
                                                          ' file path.'))
  config_pull_parser.add_argument('config_name',
                                  choices=['lightweight', 'default', 'memory'],
                                  help='Name of the predefined config to copy')
  config_pull_parser.add_argument('file_path', nargs='?',
                                  help='File path to copy the predefined to')
  return parser


def verify_args_valid(args):
  # Checks that a profiler command cannot be followed by a hw or config command.
  if args.subcommands is not None and (args.event != "custom" or
                                       args.profiler != "perfetto" or
                                       args.out_dir != "." or
                                       args.dur_ms is not None or
                                       args.app is not None or
                                       args.runs != 1 or
                                       args.simpleperf_event is not None or
                                       args.perfetto_config != "default" or
                                       args.between_dur_ms !=
                                       DEFAULT_BETWEEN_DUR_MS or
                                       args.ui is not None or
                                       args.exclude_ftrace_event is not None or
                                       args.include_ftrace_event is not None or
                                       args.from_user is not None or
                                       args.to_user is not None):
    return None, ValidationError(
        ("Command is invalid because profiler command is followed by a hw"
         " or config command."),
        "For instance, remove the hw or config subcommand to perform a test.")

  # Checks that if --profiler is set to simpleperf and --simpleperf-event
  # is not passed, then --simpleperf-event is set to cpu-cycles.
  if args.profiler == "simpleperf" and args.simpleperf_event is None:
    args.simpleperf_event = ['cpu-cycles']

  # Checks that if --dur-ms is passed,
  # then  --dur-ms is set greater or equal to 3000.
  if args.dur_ms is not None and args.dur_ms < 3000:
    # Test has failed, so we return an error.
    return None, ValidationError(
      "Command is invalid because --dur-ms cannot be set to smaller than 3000.",
      "For instance, set --dur-ms 3000 to capture a trace for 3 seconds.")

  # Checks that if --app is passed, then the --event is set to app-startup.
  if args.app is not None and args.event != "app-startup":
    return None, ValidationError(
        ("Command is invalid because --app is passed and --event is not set"
         " to app-startup."),
        ("For instance, set --event app-startup to perform a test for an"
         " app-startup."))

  # Checks that if --runs is passed, then --runs is set greater than 1.
  if args.runs < 1:
    return None, ValidationError(
      "Command is invalid because --runs cannot be set to smaller than 1.",
      "For instance, set --runs 1 to run a test.")

  # Checks that if --simpleperf-event is passed,
  # then --profiler is set to simpleperf.
  if args.simpleperf_event is not None and args.profiler != "simpleperf":
    return None, ValidationError(
        ("Command is invalid because --simpleperf-event cannot be passed"
         " if --profiler is not set to simpleperf."),
        ("For instance, set --profiler simpleperf to collect a simpleperf"
         " supported event."))

  # Checks that redundant calls to --simpleperf-event can’t be made
  # (e.g. appending cpu-cycles or instructions more than once).
  if args.simpleperf_event is not None and\
      len(args.simpleperf_event) != len(set(args.simpleperf_event)):
    return None, ValidationError(
        ("Command is invalid because redundant calls -to -simpleperf-event"
         " cannot be made."),
        ("For instance, only set --simpleperf-event cpu-cycles once if you want"
         " to collect cpu-cycles and only set --simpleperf-event instructions"
         " once if you want to collect instructions."))

  # Checks that if --perfetto-config is passed,
  # then --profiler is set to perfetto.
  if args.perfetto_config != "default" and args.profiler != "perfetto":
    return None, ValidationError(
        ("Command is invalid because --perfetto-config cannot be passed"
         " if --profiler is not set to perfetto."),
        ("For instance, set --profiler perfetto to choose a perfetto-config"
         " to use."))

  # Checks that if --between-dur-ms is passed,
  # then --between-dur-ms is set greater or equal to 3000.
  if args.between_dur_ms < 3000:
    # Test has failed, so we return an error.
    return None, ValidationError(
        ("Command is invalid because --between-dur-ms cannot be set to smaller"
         " than 3000."),
        ("For instance, set --between-dur-ms 3000 to wait 3 seconds between"
         " each run."))

  # Checks that if --between-dur-ms is passed,
  # then --runs is set to set greater than 1
  if args.between_dur_ms != DEFAULT_BETWEEN_DUR_MS and args.runs == 1:
    # Test has failed, so we return an error.
    return None, ValidationError(
        ("Command is invalid because --between-dur-ms cannot be passed"
         " if --runs is not set greater than 1."),
        "For instance, set --runs 2 to run 2 tests.")

  # Checks that if --runs is set to 1 and --ui is not passed,
  # then --ui is set to true.
  if args.runs == 1 and args.ui is None:
    args.ui = True

  # Checks that if --runs is set to greater than 1 and --ui is not passed,
  # then --ui is set to false.
  if args.runs > 1 and args.ui is None:
    args.ui = False

  # Checks that if --exclude-ftrace-event is passed,
  # then --profiler is set to perfetto.
  if args.exclude_ftrace_event is not None and args.profiler != "perfetto":
    return None, ValidationError(
        ("Command is invalid because --exclude-ftrace-event cannot be passed"
         " if --profiler is not set to perfetto."),
        ("For instance, set --profiler perfetto to exclude an ftrace event"
         " from perfetto config."))

  # Checks that if --include-ftrace-event is passed,
  # then --profiler is set to perfetto.
  if args.include_ftrace_event is not None and args.profiler != "perfetto":
    return None, ValidationError(
        ("Command is invalid because --include-ftrace-event cannot be passed"
         " if --profiler is not set to perfetto."),
        ("For instance, set --profiler perfetto to include an ftrace event"
         " in perfetto config."))

  # Checks that torq hw doesn’t work when passed by itself.
  if args.subcommands == "hw" and args.hw_subcommand is None:
    return None, ValidationError(
        ("Command is invalid because torq hw cannot be called without"
         " a subcommand."),
        ("For instance, using one of the following subcommands:"
         " (torq hw set, torq hw get, torq hw list) in order to use torq hw."))

  # Checks that hw set --num-cpus <int> can’t be passed
  # if hardware is being set.
  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.config is not None and args.num_cpus is not None):
    return None, ValidationError(
        ("Command is invalid because torq hw --num-cpus cannot be passed if a"
         " new hardware configuration is also set at the same time"),
        ("For instance, set torq hw --num-cpus 2 by itself to set 2 active"
         " cores in the hardware."))

    # Checks that hw set --memory <intG> can’t be passed
    # if hardware is being set.
  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.config is not None and args.memory is not None):
    return None, ValidationError(
        ("Command is invalid because torq hw --memory cannot be passed if a"
         " new hardware configuration is also set at the same time"),
        ("For instance, set torq hw --memory 4G by itself to limit the memory"
         " of the device to 4 gigabytes."))

  # Checks that if hw set --num-cpus <int> is passed,
  # then int is greater than 1.
  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.num_cpus is not None and args.num_cpus < 1):
    return None, ValidationError(
        ("Command is invalid because hw set --num-cpus cannot be set to"
         " smaller than 1."),
        ("For instance, set hw set --num-cpus 1 to set 1 active core in "
         " hardware."))

  # Checks that if hw set -m <intG> is passed,
  # then string matches <intG> format and int is greater than 1.
  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.memory is not None):
    index = args.memory.find("G")
    if index == -1 or args.memory[-1] != "G" or len(args.memory) == 1:
      return None, ValidationError(
          ("Command is invalid because the argument for hw set --memory does"
           " not match the <int>G format."),
          ("For instance, set hw set --memory 4G to limit the memory of the"
           " device to 4 gigabytes."))
    for i in range(index):
      if not args.memory[i].isdigit():
        return None, ValidationError(
            ("Command is invalid because the argument for hw set --memory"
             " does not match the <int>G format."),
            ("For instance, set hw set --memory 4G to limit the memory of"
             " the device to 4 gigabytes."))
    if args.memory[0] == "0":
      return None, ValidationError(
          ("Command is invalid because hw set --memory cannot be set to"
           " smaller than 1."),
          ("For instance, set hw set --memory 4G to limit the memory of"
           " the device to 4 gigabytes."))

  # Checks that torq hw set doesn’t work when passed by itself.
  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.config is None and args.num_cpus is None and args.memory is None):
    return None, ValidationError(
        ("Command is invalid because torq hw set cannot be called without"
         " a subcomand."),
        ("For instance, using one of the following subcommands:"
         " (torq hw set <config>, torq hw set --num-cpus <int>,"
         " torq hw set --memory <int>G,"
         " torq hw set --num-cpus <int> --memory <int>G,"
         " torq hw set --memory <int>G --num-cpus <int>)"
         " in order to use torq hw set."))

  # Checks that torq config doesn’t work by itself.
  if args.subcommands == "config" and args.config_subcommand is None:
    return None, ValidationError(
        ("Command is invalid because torq config cannot be called "
         " a subcomand."),
        ("For instance, using one of the following subcommands:"
         " (torq config list, torq config show, torq config pull)"
         " in order to use torq config."))

  # Checks than when torq config pull <config name> [file-path] is passed
  # without the [file-path] argument,
  # then the [filepath] argument is set to ./<config-name>.config
  if (args.subcommands == "config" and args.config_subcommand == "pull" and
      args.file_path == None):
    args.file_path = "./" + args.config_name + ".config"

  print(args)

  # All tests pass, so we return the arguments.
  return args, None


def create_profiler_command(args):
  command = ProfilerCommand("profiler", args.event, args.profiler, args.out_dir,
                            args.dur_ms,
                            args.app, args.runs, args.simpleperf_event,
                            args.perfetto_config, args.between_dur_ms,
                            args.ui, args.exclude_ftrace_event,
                            args.include_ftrace_event, args.from_user,
                            args.to_user)
  return command


def create_hw_command(args):
  command = None
  type = "hw " + args.hw_subcommand
  if args.hw_subcommand == "set":
    command = HWCommand(type, args.config, args.num_cpus,
                        args.memory)
  else:
    command = HWCommand(args.hw_subcommand, None, None, None)
  return command


def create_config_command(args):
  command = None
  type = "config " + args.config_subcommand
  if args.config_subcommand == "pull":
    command = ConfigCommand(type, args.config_name,
                            args.file_path)
  if args.config_subcommand == "show":
    command = ConfigCommand(type, args.config_name, None)
  if args.config_subcommand == "list":
    command = ConfigCommand(type, None, None)
  return command


def get_command_type(args):
  command = None
  if args.subcommands is None:
    command = create_profiler_command(args)
  if args.subcommands == "hw":
    command = create_hw_command(args)
  if args.subcommands == "config":
    command = create_config_command(args)
  return command


def prettify_error(error):
  print(error.message, "\nSuggestion:\n\t", error.suggestion)


def main():
  parser = create_parser()
  args = parser.parse_args()
  args, error = verify_args_valid(args)
  if error is not None:
    prettify_error(error)
    return
  command = get_command_type(args)
  error = command.execute()
  if error is not None:
    prettify_error(error)
    return


if __name__ == '__main__':
  main()
