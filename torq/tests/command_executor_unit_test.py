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
from unittest import mock
from io import StringIO
from command import ProfilerCommand
from command_executor import ProfilerCommandExecutor
from validation_error import ValidationError

MOCK_DEVICE = "mock-device"
MOCK_ERROR = "mock-error"
MOCK_CONFIG = "mock-config"


class ProfilerCommandExecutorUnitTest(unittest.TestCase):

  def setUp(self):
    self.mock_create_config = mock.patch.object(ProfilerCommandExecutor,
                                                "create_config").start()

  @mock.patch.object(sys, "stdout", new_callable=StringIO)
  @mock.patch.object(ProfilerCommandExecutor, "retrieve_perf_data",
                     autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "execute_run", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_print_current_run_success(self,
      mock_prepare_device_for_run, mock_execute_run, mock_retrieve_perf_data,
      mock_terminal_output):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.use_ui = False
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = None
    mock_execute_run.side_effect = (lambda profiler_command_executor, device,
        command, config, run: (print("Performing run %s" % run), None)[1])
    mock_retrieve_perf_data.return_value = None
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertEqual(error, None)
    self.assertEqual(mock_terminal_output.getvalue().strip(),
                     "Performing run 1")

  def test_execute_command_create_config_failure(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_error = ValidationError(MOCK_ERROR, None)
    self.mock_create_config.return_value = None, mock_error
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(ProfilerCommandExecutor, "prepare_device", autospec=True)
  def test_execute_command_prepare_device_failure(self, mock_prepare_device):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_error = ValidationError(MOCK_ERROR, None)
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device.return_value = mock_error
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_prepare_device_for_run_failure(self,
      mock_prepare_device_for_run):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_error = ValidationError(MOCK_ERROR, None)
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = mock_error
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(ProfilerCommandExecutor, "execute_run", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_execute_run_failure(self,
      mock_prepare_device_for_run, mock_execute_run):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_error = ValidationError(MOCK_ERROR, None)
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = None
    mock_execute_run.return_value = mock_error
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(ProfilerCommandExecutor, "retrieve_perf_data",
                     autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "execute_run", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_retrieve_perf_data_failure(self,
      mock_prepare_device_for_run, mock_execute_run, mock_retrieve_perf_data):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_error = ValidationError(MOCK_ERROR, None)
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = None
    mock_execute_run.return_value = None
    mock_retrieve_perf_data.return_value = mock_error
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(ProfilerCommandExecutor, "cleanup", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "retrieve_perf_data",
                     autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "execute_run", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_cleanup_failure(self, mock_prepare_device_for_run,
      mock_execute_run, mock_retrieve_perf_data, mock_cleanup):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_error = ValidationError(MOCK_ERROR, None)
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = None
    mock_execute_run.return_value = None
    mock_retrieve_perf_data.return_value = None
    mock_cleanup.return_value = mock_error
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(ProfilerCommandExecutor, "retrieve_perf_data",
                     autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "execute_run", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_no_ui_success(self, mock_prepare_device_for_run,
      mock_execute_run, mock_retrieve_perf_data):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.use_ui = False
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = None
    mock_execute_run.return_value = None
    mock_retrieve_perf_data.return_value = None
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertEqual(error, None)

  @mock.patch.object(ProfilerCommandExecutor, "open_ui", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "retrieve_perf_data",
                     autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "execute_run", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_use_ui_success(self, mock_prepare_device_for_run,
      mock_execute_run, mock_retrieve_perf_data, mock_open_ui):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.use_ui = True
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = None
    mock_execute_run.return_value = None
    mock_retrieve_perf_data.return_value = None
    mock_open_ui.return_value = None
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertEqual(error, None)

  @mock.patch.object(ProfilerCommandExecutor, "open_ui", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "retrieve_perf_data",
                     autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "execute_run", autospec=True)
  @mock.patch.object(ProfilerCommandExecutor, "prepare_device_for_run",
                     autospec=True)
  def test_execute_command_use_ui_failure(self, mock_prepare_device_for_run,
      mock_execute_run, mock_retrieve_perf_data, mock_open_ui):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.runs = 1
    mock_command.use_ui = True
    mock_error = ValidationError(MOCK_ERROR, None)
    self.mock_create_config.return_value = MOCK_CONFIG, None
    mock_prepare_device_for_run.return_value = None
    mock_execute_run.return_value = None
    mock_retrieve_perf_data.return_value = None
    mock_open_ui.return_value = mock_error
    command_executor = ProfilerCommandExecutor()

    error = (
        command_executor.execute_command(mock_command, MOCK_DEVICE))

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, MOCK_ERROR)
    self.assertEqual(error.suggestion, None)


if __name__ == '__main__':
  unittest.main()
