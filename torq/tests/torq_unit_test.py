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

import unittest
import sys
import os
from unittest import mock
from torq import create_parser, verify_args_valid, get_command_type, \
  DEFAULT_DUR_MS, MIN_DURATION_MS, DEFAULT_OUT_DIR, \
  PREDEFINED_PERFETTO_CONFIGS



class TorqUnitTest(unittest.TestCase):

  def create_command(self, parser):
    args, error = self.prepare_arguments(parser)
    command = get_command_type(args)
    return command, error

  def validate_command(self, parser, string):
    command, error = self.create_command(parser)
    self.assertEqual(error, None)
    self.assertEqual(command.get_type(), string)

  def prepare_arguments(self, parser):
    args = parser.parse_args()
    return verify_args_valid(args)

  def verify_error_message(self, parser, message):
    args, error = self.prepare_arguments(parser)
    self.assertEqual(error.message, message)

  def set_up_parser(self, command_string):
    parser = create_parser()
    sys.argv = command_string.split()
    return parser

  # TODO(b/285191111): Parameterize the test functions.
  def test_create_parser_default_values(self):
    parser = self.set_up_parser("torq.py")
    args = parser.parse_args()

    self.assertEqual(args.event, "custom")
    self.assertEqual(args.profiler, "perfetto")
    self.assertEqual(args.out_dir, DEFAULT_OUT_DIR)
    self.assertEqual(args.runs, 1)
    self.assertEqual(args.perfetto_config, PREDEFINED_PERFETTO_CONFIGS[0])
    self.assertEqual(args.dur_ms, DEFAULT_DUR_MS)
    self.assertEqual(args.between_dur_ms, DEFAULT_DUR_MS)
    self.validate_command(parser, "profiler")

  def test_create_parser_valid_event_names(self):
    parser = self.set_up_parser("torq.py -e custom")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -e boot")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -e user-switch")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -e app-startup")

    self.validate_command(parser, "profiler")

  def test_create_parser_invalid_event_names(self):
    parser = self.set_up_parser("torq.py -e fake-event")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_create_parser_valid_profiler_names(self):
    parser = self.set_up_parser("torq.py -p perfetto")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -p simpleperf")

    self.validate_command(parser, "profiler")

  def test_create_parser_invalid_profiler_names(self):
    parser = self.set_up_parser("torq.py -e fake-profiler")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  @mock.patch.object(os.path, "isdir", autospec=True)
  def test_verify_valid_out_dir_path(self, mock_is_dir):
    mock_is_dir.side_effect = (lambda x:
                               x in ["mock-directory", "mock-directory/"])

    parser = self.set_up_parser("torq.py -o mock-directory")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -o mock-directory/")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -o %s" % DEFAULT_OUT_DIR)

    self.validate_command(parser, "profiler")

  @mock.patch.object(os.path, "isfile", autospec=True)
  @mock.patch.object(os.path, "isdir", autospec=True)
  def test_verify_args_invalid_out_dir_paths(self, mock_is_dir, mock_is_file):
    mock_is_dir.side_effect = (lambda x: x in ["mock-directory"])
    mock_is_file.side_effect = (lambda x: x in ["mock-file"])
    parser = self.set_up_parser("torq.py -o mock-file")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --out-dir is not a valid"
                                       " directory path: mock-file."))

    parser = self.set_up_parser("torq.py -o fake-directory")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --out-dir is not a valid"
                                       " directory path: fake-directory."))

  def test_create_parser_valid_ui(self):
    parser = self.set_up_parser("torq.py --ui")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py --no-ui")

    self.validate_command(parser, "profiler")

  def test_create_parser_invalid_ui(self):
    parser = self.set_up_parser("torq.py --fake-ui")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_verify_args_valid_dur_ms_values(self):
    parser = self.set_up_parser("torq.py -d %d" % DEFAULT_DUR_MS)

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -d %d" % 100000)

    self.validate_command(parser, "profiler")

  def test_verify_args_invalid_dur_ms_values(self):
    parser = self.set_up_parser("torq.py -d -200")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --dur-ms cannot be set to a value"
                                       " smaller than %d." % MIN_DURATION_MS))

    parser = self.set_up_parser("torq.py -d 0")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --dur-ms cannot be set to a value"
                                       " smaller than %d." % MIN_DURATION_MS))
    parser = self.set_up_parser("torq.py -d 20")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --dur-ms cannot be set to a value"
                                       " smaller than %d." % MIN_DURATION_MS))

  def test_verify_args_valid_between_dur_ms_values(self):
    parser = self.set_up_parser(("torq.py -r 2 --between-dur-ms %d"
                                 % DEFAULT_DUR_MS))

    self.validate_command(parser, "profiler")

  def test_verify_args_invalid_between_dur_ms_values(self):
    parser = self.set_up_parser("torq.py -r 2 --between-dur-ms -200")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --between-dur-ms cannot be set to"
                                       " a smaller value than %d."
                                       % MIN_DURATION_MS))

    parser = self.set_up_parser("torq.py -r 2 --between-dur-ms 0")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --between-dur-ms cannot be set to a"
                                       " smaller value than %d."
                                       % MIN_DURATION_MS))

    parser = self.set_up_parser("torq.py -r 2 --between-dur-ms 20")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --between-dur-ms cannot be set to a"
                                       " smaller value than %d."
                                       % MIN_DURATION_MS))

  def test_verify_args_valid_runs_values(self):
    parser = self.set_up_parser("torq.py -r 4")

    self.validate_command(parser, "profiler")

  def test_verify_args_invalid_runs_values(self):
    parser = self.set_up_parser("torq.py -r -2")

    self.verify_error_message(parser, ("Command is invalid because --runs"
                                       " cannot be set to a value smaller"
                                       " than 1."))

    parser = self.set_up_parser("torq.py -r 0")

    self.verify_error_message(parser, ("Command is invalid because --runs"
                                       " cannot be set to a value smaller"
                                       " than 1."))

  @mock.patch.object(os.path, "isfile", autospec=True)
  def test_verify_args_valid_perfetto_config_path(self, mock_is_file):
    mock_is_file.side_effect = (lambda x: x in ["mock-file"])
    parser = self.set_up_parser("torq.py --perfetto-config mock-file")

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser(("torq.py --perfetto-config %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[0]))

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser(("torq.py --perfetto-config %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[1]))

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser(("torq.py --perfetto-config %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[2]))

    self.validate_command(parser, "profiler")

  @mock.patch.object(os.path, "isfile", autospec=True)
  @mock.patch.object(os.path, "isdir", autospec=True)
  def test_verify_args_invalid_perfetto_config_path(self, mock_is_dir,
      mock_is_file):
    mock_is_dir.side_effect = (lambda x: x in ["mock-directory",
                                               DEFAULT_OUT_DIR])
    mock_is_file.side_effect = (lambda x: x in ["mock-file"])
    parser = self.set_up_parser("torq.py --perfetto-config fake-file")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --perfetto-config is not a"
                                       " valid file path: fake-file"))

    parser = self.set_up_parser("torq.py --perfetto-config mock-directory")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --perfetto-config is not a"
                                       " valid file path: mock-directory"))

    parser = self.set_up_parser(("torq.py --perfetto-config %s"
                                 % DEFAULT_OUT_DIR))

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --perfetto-config is not a"
                                       " valid file path: %s"
                                       % DEFAULT_OUT_DIR))

  def test_verify_args_valid_hw_num_cpus_values(self):
    parser = self.set_up_parser("torq.py hw set -n 2")

    self.validate_command(parser, "hw set")

    parser = self.set_up_parser("torq.py hw set -n 2 -m 4G")

    self.validate_command(parser, "hw set")

  def test_verify_args_invalid_hw_num_cpus_values(self):
    parser = self.set_up_parser("torq.py hw set -n 0")

    self.verify_error_message(parser, ("Command is invalid because hw set"
                                       " --num-cpus cannot be set to smaller"
                                       " than 1."))

    parser = self.set_up_parser("torq.py hw set -n -1")

    self.verify_error_message(parser, ("Command is invalid because hw set"
                                       " --num-cpus cannot be set to smaller"
                                       " than 1."))

  def test_verify_args_valid_hw_memory_values(self):
    parser = self.set_up_parser("torq.py hw set -m 4G")

    self.validate_command(parser, "hw set")

    parser = self.set_up_parser("torq.py hw set -m 4G -n 2")

    self.validate_command(parser, "hw set")

  def test_verify_args_invalid_hw_memory_values(self):
    parser = self.set_up_parser("torq.py hw set -m 0G")

    self.verify_error_message(parser, ("Command is invalid because hw set"
                                       " --memory cannot be set to smaller"
                                       " than 1."))

    parser = self.set_up_parser("torq.py hw set -m 4g")

    self.verify_error_message(parser, ("Command is invalid because the argument"
                                       " for hw set --memory does not match"
                                       " the <int>G format."))

    parser = self.set_up_parser("torq.py hw set -m G")

    self.verify_error_message(parser, ("Command is invalid because the argument"
                                       " for hw set --memory does not match"
                                       " the <int>G format."))

  def test_create_parser_invalid_hw_memory_values(self):
    parser = self.set_up_parser("torq.py hw set -m -1G")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_create_parser_valid_hw_config_show_values(self):
    parser = self.set_up_parser(("torq.py config show %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[0]))

    self.validate_command(parser, "config show")

    parser = self.set_up_parser(("torq.py config show %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[1]))

    self.validate_command(parser, "config show")

    parser = self.set_up_parser(("torq.py config show %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[2]))

    self.validate_command(parser, "config show")

  def test_create_parser_invalid_hw_config_show_values(self):
    parser = self.set_up_parser("torq.py config show fake-config")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_create_parser_valid_hw_config_pull_values(self):
    parser = self.set_up_parser(("torq.py config pull  %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[0]))

    self.validate_command(parser, "config pull")

    parser = self.set_up_parser(("torq.py config pull %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[1]))

    self.validate_command(parser, "config pull")

    parser = self.set_up_parser(("torq.py config pull %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[2]))

    self.validate_command(parser, "config pull")

    parser = self.set_up_parser(("torq.py config pull %s path"
                                 % PREDEFINED_PERFETTO_CONFIGS[0]))

    self.validate_command(parser, "config pull")

    parser = self.set_up_parser(("torq.py config pull %s path"
                                 % PREDEFINED_PERFETTO_CONFIGS[1]))

    self.validate_command(parser, "config pull")

    parser = self.set_up_parser(("torq.py config pull %s path"
                                 % PREDEFINED_PERFETTO_CONFIGS[2]))

    self.validate_command(parser, "config pull")

  def test_create_parser_invalid_hw_config_pull_values(self):
    parser = self.set_up_parser("torq.py config pull fake-config")

    with self.assertRaises(SystemExit):
      parser.parse_args()

    parser = self.set_up_parser("torq.py config pull fake-config config")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_create_parser_valid_hw_set_values(self):
    parser = self.set_up_parser("torq.py hw set seahawk")

    self.validate_command(parser, "hw set")

    parser = self.set_up_parser("torq.py hw set seaturtle")

    self.validate_command(parser, "hw set")

  def test_create_parser_invalid_hw_set_values(self):
    parser = self.set_up_parser("torq.py hw set fake-device")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  # TODO: Make sure that package name is correct once feature is implemented.
  def test_verify_args_app_and_event_valid_dependency(self):
    parser = self.set_up_parser("torq.py -e app-startup -a google")

    self.validate_command(parser, "profiler")

  def test_verify_args_app_and_event_invalid_dependency(self):
    parser = self.set_up_parser("torq.py -a google")

    self.verify_error_message(parser, ("Command is invalid because --app is"
                                       " passed and --event is not set to"
                                       " app-startup."))

  def test_verify_args_profiler_and_simpleperf_event_valid_dependencies(self):
    parser = self.set_up_parser("torq.py -p simpleperf")
    args, error = self.prepare_arguments(parser)

    self.assertEqual(len(args.simpleperf_event), 1)
    self.assertEqual(args.simpleperf_event[0], "cpu-cycles")
    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -p simpleperf -s cpu-cycles")

    self.validate_command(parser, "profiler")

  def test_verify_args_profiler_and_simpleperf_event_invalid_dependencies(
      self):
    parser = self.set_up_parser("torq.py -s cpu-cycles")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --simpleperf-event cannot be passed if"
                                       " --profiler is not set to simpleperf."))

  def test_verify_args_profiler_and_perfetto_config_valid_dependency(self):
    parser = self.set_up_parser(("torq.py -p perfetto --perfetto-config"
                                 " %s" % PREDEFINED_PERFETTO_CONFIGS[1]))

    self.validate_command(parser, "profiler")

  def test_verify_args_profiler_and_perfetto_config_invalid_dependency(self):
    parser = self.set_up_parser("torq.py -p simpleperf --perfetto-config"
                                " %s" % PREDEFINED_PERFETTO_CONFIGS[1])

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --perfetto-config cannot be passed if"
                                       " --profiler is not set to perfetto."))

  def test_verify_args_runs_and_between_dur_ms_valid_dependency(self):
    parser = self.set_up_parser("torq.py -r 2 --between-dur-ms 5000")

    self.validate_command(parser, "profiler")

  def test_verify_args_runs_and_between_dur_ms_invalid_dependency(self):
    parser = self.set_up_parser("torq.py --between-dur-ms 5000")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --between-dur-ms cannot be passed"
                                       " if --runs is not a value greater"
                                       " than 1."))

    parser = self.set_up_parser("torq.py -r 1 --between-dur-ms 5000")

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --between-dur-ms cannot be passed"
                                       " if --runs is not a value greater"
                                       " than 1."))

  def test_verify_args_profiler_and_ftrace_events_valid_dependencies(self):
    parser = self.set_up_parser(("torq.py --exclude-ftrace-event"
                                 " syscall-enter"))

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser(("torq.py -p perfetto --exclude-ftrace-event"
                                 " syscall-enter"))

    self.validate_command(parser, "profiler")

    parser = self.set_up_parser(("torq.py -p perfetto --include-ftrace-event"
                                 " syscall-enter"))

    self.validate_command(parser, "profiler")

  def test_verify_args_profiler_and_ftrace_events_invalid_dependencies(self):
    parser = self.set_up_parser(("torq.py -p simpleperf"
                                 " --exclude-ftrace-event syscall-enter"))

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --exclude-ftrace-event cannot be"
                                       " passed if --profiler is not set to"
                                       " perfetto."))

    parser = self.set_up_parser(("torq.py -p simpleperf"
                                 " --include-ftrace-event syscall-enter"))

    self.verify_error_message(parser, ("Command is invalid because"
                                       " --include-ftrace-event cannot be"
                                       " passed if --profiler is not set to"
                                       " perfetto."))

  def test_create_parser_hw_set_invalid_dependencies(self):
    parser = self.set_up_parser("torq.py set seahawk -n 2")

    with self.assertRaises(SystemExit):
      parser.parse_args()

    parser = self.set_up_parser("torq.py set seahawk -m 4G")

    with self.assertRaises(SystemExit):
      parser.parse_args()

    parser = self.set_up_parser("torq.py set seahawk -n 2 m 4G")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_verify_args_ui_bool_true_and_runs_dependencies(self):
    parser = self.set_up_parser("torq.py")

    args, error = self.prepare_arguments(parser)

    self.assertEqual(args.ui, True)
    self.validate_command(parser, "profiler")

    parser = self.set_up_parser("torq.py -r 1")

    args, error = self.prepare_arguments(parser)

    self.assertEqual(args.ui, True)
    self.validate_command(parser, "profiler")

  # UI is false by default when multiples runs are specified.
  def test_verify_args_ui_bool_false_and_runs_dependencies(self):
    parser = self.set_up_parser("torq.py -r 2")
    args, error = self.prepare_arguments(parser)

    self.assertEqual(args.ui, False)
    self.validate_command(parser, "profiler")

  def test_verify_args_multiple_valid_simpleperf_events(self):
    parser = self.set_up_parser(("torq.py -p simpleperf -s cpu-cycles"
                                 " -s instructions"))

    self.validate_command(parser, "profiler")

  def test_verify_args_multiple_invalid_simpleperf_events(self):
    parser = self.set_up_parser(("torq.py -p simpleperf -s cpu-cycles"
                                 " -s cpu-cycles"))

    self.verify_error_message(parser, ("Command is invalid because redundant"
                                       " calls to --simpleperf-event cannot"
                                       " be made."))

  def test_create_parser_invalid_perfetto_config_command(self):
    parser = self.set_up_parser("torq.py --perfetto-config")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_verify_args_invalid_set_subcommands(self):
    parser = self.set_up_parser("torq.py hw set")

    self.verify_error_message(parser, ("Command is invalid because torq hw set"
                                       " cannot be called without a"
                                       " subcommand."))

  def test_create_parser_invalid_set_subcommands(self):
    parser = self.set_up_parser("torq.py set show")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_create_parser_invalid_set_num_cpus_subcommand(self):
    parser = self.set_up_parser("torq.py set -n")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_create_parser_invalid_set_memory_subcommand(self):
    parser = self.set_up_parser("torq.py set -m")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_verify_args_invalid_config_subcommands(self):
    parser = self.set_up_parser("torq.py config")

    self.verify_error_message(parser, ("Command is invalid because torq config"
                                       " cannot be called without a"
                                       " subcommand."))

  def test_create_parser_invalid_config_subcommands(self):
    parser = self.set_up_parser("torq.py config get")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_verify_args_default_config_pull_filepath(self):
    parser = self.set_up_parser(("torq.py config pull %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[0]))
    args, error = self.prepare_arguments(parser)

    self.assertEqual(args.file_path, ("./artifacts/%s.bptxt"
                                      % PREDEFINED_PERFETTO_CONFIGS[0]))
    self.validate_command(parser, "config pull")

    parser = self.set_up_parser(("torq.py config pull %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[1]))
    args, error = self.prepare_arguments(parser)

    self.assertEqual(args.file_path, ("./artifacts/%s.bptxt"
                                      % PREDEFINED_PERFETTO_CONFIGS[1]))
    self.validate_command(parser, "config pull")

    parser = self.set_up_parser(("torq.py config pull %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[2]))
    args, error = self.prepare_arguments(parser)

    self.assertEqual(args.file_path, ("./artifacts/%s.bptxt"
                                      % PREDEFINED_PERFETTO_CONFIGS[2]))
    self.validate_command(parser, "config pull")

  def test_verify_args_invalid_mixing_of_profiler_and_hw_subcommand(self):
    parser = self.set_up_parser("torq.py -d 20000 hw set seahawk")

    self.verify_error_message(parser, ("Command is invalid because profiler"
                                       " command is followed by a hw or"
                                       " config command."))

  def test_create_parser_invalid_mixing_of_profiler_and_hw_subcommand(self):
    parser = self.set_up_parser("torq.py hw set seahawk -d 20000 ")

    with self.assertRaises(SystemExit):
      parser.parse_args()

  def test_verify_args_invalid_mixing_of_profiler_and_config_subcommand(self):
    parser = self.set_up_parser(("torq.py -d 20000 config pull %s"
                                 % PREDEFINED_PERFETTO_CONFIGS[1]))

    self.verify_error_message(parser, ("Command is invalid because profiler"
                                       " command is followed by a hw or"
                                       " config command."))

  def test_create_parser_invalid_mixing_of_profiler_and_config_subcommand(self):
    parser = self.set_up_parser(("torq.py config pull %s -d 20000"
                                 % PREDEFINED_PERFETTO_CONFIGS[1]))

    with self.assertRaises(SystemExit):
      parser.parse_args()


if __name__ == '__main__':
  unittest.main()
