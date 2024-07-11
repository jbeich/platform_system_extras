#
# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import argparse
from command import ProfilerCommand, HWCommand, ConfigCommand
from validation_error import ValidationError

DEFAULT_DUR_MS = 10000
MIN_DURATION_MS = 3000


def create_parser():
  parser = argparse.ArgumentParser(prog='torq command',
                                   description=('Torq CLI tool for performance'
                                                ' tests.'))
  parser.add_argument('-e', '--event',
                      choices=['boot', 'user-switch', 'app-startup', 'custom'],
                      default='custom', help='The event to trace/profile.')
  parser.add_argument('-p', '--profiler', choices=['perfetto', 'simpleperf'],
                      default='perfetto', help='The performance data source.')
  parser.add_argument('-o', '--out-dir', default='.',
                      help='The path to the output directory.')
  parser.add_argument('-d', '--dur-ms', type=int, default=DEFAULT_DUR_MS,
                      help=('The duration (ms) of the event. Determines when'
                            ' to stop collecting performance data.'))
  parser.add_argument('-a', '--app',
                      help='The package name of the app we want to start.')
  parser.add_argument('-r', '--runs', type=int, default=1,
                      help=('The number of times to run the event and'
                            ' capture the perf data.'))
  parser.add_argument('-s', '--simpleperf-event', action='append',
                      help=('Simpleperf supported events to be collected.'
                            ' e.g. cpu-cycles, instructions'))
  parser.add_argument('--perfetto-config', default='default',
                      help='Filepath of the perfetto config to use.')
  parser.add_argument('--between-dur-ms', type=int, default=DEFAULT_DUR_MS,
                      help='Time (ms) to wait before executing the next event.')
  parser.add_argument('--ui', choices=['true', 'false'],
                      help=('Specifies opening of UI visualization tool'
                            ' after profiling is complete.'))
  parser.add_argument('--exclude-ftrace-event',
                      help=('Excludes the ftrace event from the perfetto'
                            ' config events.'))
  parser.add_argument('--include-ftrace-event',
                      help=('Includes the ftrace event in the perfetto'
                            ' config events.'))
  parser.add_argument('--from-user', type=int,
                      help='The user id from which to start the user switch')
  parser.add_argument('--to-user', type=int,
                      help='The user id of user that system is switching to.')
  subparsers = parser.add_subparsers(dest='subcommands', help='Subcommands')
  hw_parser = subparsers.add_parser('hw',
                                    help=('The hardware subcommand used to'
                                          ' change the H/W configuration of'
                                          ' the device.'))
  hw_subparsers = hw_parser.add_subparsers(dest='hw_subcommand',
                                           help='torq hw subcommands')
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
  hw_subparsers.add_parser('get',
                           help=('Command to get the current hardware'
                                 ' configuration. Will provide the number of'
                                 ' cpus and memory available.'))
  hw_subparsers.add_parser('list',
                           help=('Command to list the supported HW'
                                 ' configurations.'))
  config_parser = subparsers.add_parser('config',
                                        help=('The config subcommand used'
                                              ' to list and show the'
                                              ' predefined perfetto configs.'))
  config_subparsers = config_parser.add_subparsers(dest='config_subcommand',
                                                   help=('torq config'
                                                         ' subcommands'))
  config_subparsers.add_parser('list',
                               help=('Command to list the predefined'
                                     ' perfetto configs'))
  config_show_parser = config_subparsers.add_parser('show',
                                                    help=('Command to print'
                                                          ' the '
                                                          ' perfetto config'
                                                          ' in the terminal.'))
  config_show_parser.add_argument('config_name',
                                  choices=['lightweight', 'default', 'memory'],
                                  help=('Name of the predefined perfetto'
                                        ' config to print.'))
  config_pull_parser = config_subparsers.add_parser('pull',
                                                    help=('Command to copy'
                                                          ' a predefined config'
                                                          ' to the specified'
                                                          ' file path.'))
  config_pull_parser.add_argument('config_name',
                                  choices=['lightweight', 'default', 'memory'],
                                  help='Name of the predefined config to copy')
  config_pull_parser.add_argument('file_path', nargs='?',
                                  help=('File path to copy the predefined'
                                        ' config to'))
  return parser


def verify_args_valid(args):
  if args.subcommands is not None and (args.event != "custom" or
                                       args.profiler != "perfetto" or
                                       args.out_dir != "." or
                                       args.dur_ms != DEFAULT_DUR_MS or
                                       args.app is not None or
                                       args.runs != 1 or
                                       args.simpleperf_event is not None or
                                       args.perfetto_config != "default" or
                                       args.between_dur_ms != DEFAULT_DUR_MS or
                                       args.ui is not None or
                                       args.exclude_ftrace_event is not None or
                                       args.include_ftrace_event is not None or
                                       args.from_user is not None or
                                       args.to_user is not None):
    return None, ValidationError(
        ("Command is invalid because profiler command is followed by a hw"
         " or config command."),
        "Remove the 'hw' or 'config' subcommand to profile the device instead.")

  if args.dur_ms < MIN_DURATION_MS:
    return None, ValidationError(
        "Command is invalid because --dur-ms cannot be set to smaller than %d."
        % MIN_DURATION_MS,
        "Set --dur-ms %d to capture a trace for 3 seconds."
        % MIN_DURATION_MS)

  if args.app is not None and args.event != "app-startup":
    return None, ValidationError(
        ("Command is invalid because --app is passed and --event is not set"
         " to app-startup."),
        ("To profile an app startup run:"
         " torq --event app-startup --app <package-name>"))

  if args.runs < 1:
    return None, ValidationError(
        "Command is invalid because --runs cannot be set to smaller than 1.",
        None)

  if args.simpleperf_event is not None and args.profiler != "simpleperf":
    return None, ValidationError(
        ("Command is invalid because --simpleperf-event cannot be passed"
         " if --profiler is not set to simpleperf."),
        ("To capture the simpleperf event run:"
         " torq --profiler simpleperf --simpleperf-event %s"
         % args.simpleperf_event[0]))

  if (args.simpleperf_event is not None and
      len(args.simpleperf_event) != len(set(args.simpleperf_event))):
    return None, ValidationError(
        ("Command is invalid because redundant calls -to -simpleperf-event"
         " cannot be made."),
        ("Only set --simpleperf-event cpu-cycles once if you want"
         " to collect cpu-cycles."))

  if args.perfetto_config != "default" and args.profiler != "perfetto":
    return None, ValidationError(
        ("Command is invalid because --perfetto-config cannot be passed"
         " if --profiler is not set to perfetto."),
        ("Set --profiler perfetto to choose a perfetto-config"
         " to use."))

  if args.between_dur_ms < MIN_DURATION_MS:
    return None, ValidationError(
        ("Command is invalid because --between-dur-ms cannot be set to smaller"
         " than %d." % MIN_DURATION_MS),
        ("Set --between-dur-ms %d to wait 3 seconds between"
         " each run." % MIN_DURATION_MS))

  if args.between_dur_ms != DEFAULT_DUR_MS and args.runs == 1:
    return None, ValidationError(
        ("Command is invalid because --between-dur-ms cannot be passed"
         " if --runs is not greater than 1."),
        "Set --runs 2 to run 2 tests.")

  if args.exclude_ftrace_event is not None and args.profiler != "perfetto":
    return None, ValidationError(
        ("Command is invalid because --exclude-ftrace-event cannot be passed"
         " if --profiler is not set to perfetto."),
        ("Set --profiler perfetto to exclude an ftrace event"
         " from perfetto config."))

  if args.include_ftrace_event is not None and args.profiler != "perfetto":
    return None, ValidationError(
        ("Command is invalid because --include-ftrace-event cannot be passed"
         " if --profiler is not set to perfetto."),
        ("Set --profiler perfetto to include an ftrace event"
         " in perfetto config."))

  if args.subcommands == "hw" and args.hw_subcommand is None:
    return None, ValidationError(
        ("Command is invalid because torq hw cannot be called without"
         " a subcommand."),
        ("Use one of the following subcommands:\n"
         "\t torq hw set <config-name>\n"
         "\t torq hw get\n"
         "\t torq hw list"))

  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.config is not None and args.num_cpus is not None):
    return None, ValidationError(
        ("Command is invalid because torq hw --num-cpus cannot be passed if a"
         " new hardware configuration is also set at the same time"),
        ("Set torq hw --num-cpus 2 by itself to set 2 active"
         " cores in the hardware."))

  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.config is not None and args.memory is not None):
    return None, ValidationError(
        ("Command is invalid because torq hw --memory cannot be passed if a"
         " new hardware configuration is also set at the same time"),
        ("Set torq hw --memory 4G by itself to limit the memory"
         " of the device to 4 gigabytes."))

  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.num_cpus is not None and args.num_cpus < 1):
    return None, ValidationError(
        ("Command is invalid because hw set --num-cpus cannot be set to"
         " smaller than 1."),
        ("Set hw set --num-cpus 1 to set 1 active core in "
         " hardware."))

  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.memory is not None):
    index = args.memory.find("G")
    if index == -1 or args.memory[-1] != "G" or len(args.memory) == 1:
      return None, ValidationError(
          ("Command is invalid because the argument for hw set --memory does"
           " not match the <int>G format."),
          ("Set hw set --memory 4G to limit the memory of the"
           " device to 4 gigabytes."))
    for i in range(index):
      if not args.memory[i].isdigit():
        return None, ValidationError(
            ("Command is invalid because the argument for hw set --memory"
             " does not match the <int>G format."),
            ("Set hw set --memory 4G to limit the memory of"
             " the device to 4 gigabytes."))
    if args.memory[0] == "0":
      return None, ValidationError(
          ("Command is invalid because hw set --memory cannot be set to"
           " smaller than 1."),
          ("Set hw set --memory 4G to limit the memory of"
           " the device to 4 gigabytes."))

  if (args.subcommands == "hw" and args.hw_subcommand == "set" and
      args.config is None and args.num_cpus is None and args.memory is None):
    return None, ValidationError(
        ("Command is invalid because torq hw set cannot be called without"
         " a subcomand."),
        ("Use one of the following subcommands:"
         " (torq hw set <config>, torq hw set --num-cpus <int>,"
         " torq hw set --memory <int>G,"
         " torq hw set --num-cpus <int> --memory <int>G,"
         " torq hw set --memory <int>G --num-cpus <int>)"
         " in order to use torq hw set."))

  if args.subcommands == "config" and args.config_subcommand is None:
    return None, ValidationError(
        ("Command is invalid because torq config cannot be called "
         " a subcomand."),
        ("Use one of the following subcommands:"
         " (torq config list, torq config show, torq config pull)"
         " in order to use torq config."))

  if args.profiler == "simpleperf" and args.simpleperf_event is None:
    args.simpleperf_event = ['cpu-cycles']

  if args.ui is None:
    args.ui = (args.runs == 1)

  if (args.subcommands == "config" and args.config_subcommand == "pull" and
      args.file_path is None):
    args.file_path = "./" + args.config_name + ".config"

  return args, None


def create_profiler_command(args):
  return ProfilerCommand("profiler", args.event, args.profiler, args.out_dir,
                         args.dur_ms,
                         args.app, args.runs, args.simpleperf_event,
                         args.perfetto_config, args.between_dur_ms,
                         args.ui, args.exclude_ftrace_event,
                         args.include_ftrace_event, args.from_user,
                         args.to_user)


def create_hw_command(args):
  command = None
  type = "hw " + args.hw_subcommand
  if args.hw_subcommand == "set":
    command = HWCommand(type, args.config, args.num_cpus,
                        args.memory)
  else:
    command = HWCommand(type, None, None, None)
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
  print(error.message)
  if error.suggestion is not None:
    print("Suggestion:\n\t", error.suggestion)


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
