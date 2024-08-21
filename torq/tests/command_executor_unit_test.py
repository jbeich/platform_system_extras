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
import subprocess
import command_executor
from unittest import mock
from io import StringIO
from command import ProfilerCommand
from command_executor import ProfilerCommandExecutor
from device import AdbDevice
from validation_error import ValidationError
from torq import DEFAULT_OUT_DIR

MOCK_ERROR = "mock-error"
MOCK_CONFIG = "mock-config"
MOCK_EXCEPTION = Exception(MOCK_ERROR)


class ProfilerCommandExecutorUnitTest(unittest.TestCase):

  def setUp(self):
    self.mock_create_config = mock.patch.object(ProfilerCommandExecutor,
                                                "create_config").start()
    self.mock_device = AdbDevice("mock-serial")
    self.executor = ProfilerCommandExecutor()

  @mock.patch.object(sys, "stdout", new_callable=StringIO)
  @mock.patch.object(command_executor, "open_trace", autospec=True)
  @mock.patch.object(AdbDevice, "pull_file", autospec=True)
  @mock.patch.object(subprocess, "Popen", autospec=True)
  @mock.patch.object(AdbDevice, "start_perfetto_trace", autospec=True)
  @mock.patch.object(AdbDevice, "remove_file", autospec=True)
  @mock.patch.object(AdbDevice, "root_device", autospec=True)
  def test_execute_command_one_run_and_use_ui_success(self, mock_root_device,
      mock_remove_file, mock_start_perfetto_trace, mock_process,
      mock_pull_file, mock_open_trace, mock_terminal_output):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.out_dir = DEFAULT_OUT_DIR
    mock_command.use_ui = True
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_root_device.return_value = None
    mock_remove_file.return_value = None
    mock_start_perfetto_trace.return_value = mock_process
    mock_process.wait.return_value = None
    mock_pull_file.return_value = None
    mock_open_trace.return_value = None

    error = self.executor.execute_command(mock_command, self.mock_device)

    self.assertEqual(error, None)
    self.assertEqual(mock_terminal_output.getvalue().strip(),
                     "Performing run 1")

  @mock.patch.object(AdbDevice, "pull_file", autospec=True)
  @mock.patch.object(subprocess, "Popen", autospec=True)
  @mock.patch.object(AdbDevice, "start_perfetto_trace", autospec=True)
  @mock.patch.object(AdbDevice, "remove_file", autospec=True)
  @mock.patch.object(AdbDevice, "root_device", autospec=True)
  def test_execute_command_one_run_no_ui_success(self, mock_root_device,
      mock_remove_file, mock_start_perfetto_trace, mock_process,
      mock_pull_file):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.out_dir = DEFAULT_OUT_DIR
    mock_command.use_ui = False
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_root_device.return_value = None
    mock_remove_file.return_value = None
    mock_start_perfetto_trace.return_value = mock_process
    mock_process.wait.return_value = None
    mock_pull_file.return_value = None

    error = self.executor.execute_command(mock_command, self.mock_device)

    self.assertEqual(error, None)

  def test_execute_command_create_config_failure(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    self.mock_create_config.return_value = (None,
                                            ValidationError(MOCK_ERROR, None))

    error = self.executor.execute_command(mock_command, self.mock_device)

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(AdbDevice, "root_device", autospec=True)
  def test_execute_command_prepare_device_for_run_root_failure(self,
      mock_root_device):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.out_dir = DEFAULT_OUT_DIR
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_root_device.side_effect = MOCK_EXCEPTION

    with self.assertRaises(Exception) as e:
      self.executor.execute_command(mock_command, self.mock_device)

    self.assertEqual(str(e.exception), MOCK_ERROR)

  @mock.patch.object(AdbDevice, "remove_file", autospec=True)
  @mock.patch.object(AdbDevice, "root_device", autospec=True)
  def test_execute_command_prepare_device_for_run_remove_file_failure(self,
      mock_root_device, mock_remove_file):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.out_dir = DEFAULT_OUT_DIR
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_root_device.return_value = None
    mock_remove_file.side_effect = MOCK_EXCEPTION

    with self.assertRaises(Exception) as e:
      self.executor.execute_command(mock_command, self.mock_device)

    self.assertEqual(str(e.exception), MOCK_ERROR)

  @mock.patch.object(AdbDevice, "start_perfetto_trace", autospec=True)
  @mock.patch.object(AdbDevice, "remove_file", autospec=True)
  @mock.patch.object(AdbDevice, "root_device", autospec=True)
  def test_execute_command_execute_run_start_perfetto_trace_failure(self,
      mock_root_device, mock_remove_file, mock_start_perfetto_trace):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.out_dir = DEFAULT_OUT_DIR
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_root_device.return_value = None
    mock_remove_file.return_value = None
    mock_start_perfetto_trace.side_effect = MOCK_EXCEPTION

    with self.assertRaises(Exception) as e:
      self.executor.execute_command(mock_command, self.mock_device)

    self.assertEqual(str(e.exception), MOCK_ERROR)

  @mock.patch.object(subprocess, "Popen", autospec=True)
  @mock.patch.object(AdbDevice, "start_perfetto_trace", autospec=True)
  @mock.patch.object(AdbDevice, "remove_file", autospec=True)
  @mock.patch.object(AdbDevice, "root_device", autospec=True)
  def test_execute_command_execute_run_start_process_wait_failure(self,
      mock_root_device, mock_remove_file, mock_start_perfetto_trace,
      mock_process):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.out_dir = DEFAULT_OUT_DIR
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_root_device.return_value = None
    mock_remove_file.return_value = None
    mock_start_perfetto_trace.return_value = mock_process
    mock_process.wait.side_effect = MOCK_EXCEPTION

    with self.assertRaises(Exception) as e:
      self.executor.execute_command(mock_command, self.mock_device)

    self.assertEqual(str(e.exception), MOCK_ERROR)

  @mock.patch.object(AdbDevice, "pull_file", autospec=True)
  @mock.patch.object(subprocess, "Popen", autospec=True)
  @mock.patch.object(AdbDevice, "start_perfetto_trace", autospec=True)
  @mock.patch.object(AdbDevice, "remove_file", autospec=True)
  @mock.patch.object(AdbDevice, "root_device", autospec=True)
  def test_execute_command_execute_retrieve_perf_data_pull_file_failure(self,
      mock_root_device, mock_remove_file, mock_start_perfetto_trace,
      mock_process, mock_pull_file):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.out_dir = DEFAULT_OUT_DIR
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_root_device.return_value = None
    mock_remove_file.return_value = None
    mock_start_perfetto_trace.return_value = mock_process
    mock_process.wait.return_value = None
    mock_pull_file.side_effect = MOCK_EXCEPTION

    with self.assertRaises(Exception) as e:
      self.executor.execute_command(mock_command, self.mock_device)

    self.assertEqual(str(e.exception), MOCK_ERROR)


if __name__ == '__main__':
  unittest.main()
