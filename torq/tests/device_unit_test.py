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
from io import StringIO
from unittest import mock
from device import AdbDevice
from command import ProfilerCommand
from validation_error import ValidationError


class DeviceUnitTest(unittest.TestCase):

  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_get_adb_devices_command_failure_error(self, mock_get_adb_devices):
    mock_get_adb_devices.return_value = None, ValidationError(("Command 'adb"
                                                               " devices'"
                                                               " failed."),
                                                              None)
    adbDevice = AdbDevice()

    devices, error = adbDevice.get_adb_devices()

    self.assertEqual(devices, None)
    self.assertEqual(error.message, "Command 'adb devices' failed.")
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_get_adb_devices_returns_no_device(self, mock_get_adb_devices):
    mock_get_adb_devices.return_value = [], None
    adbDevice = AdbDevice()

    devices, error = adbDevice.get_adb_devices()

    self.assertEqual(devices, [])
    self.assertEqual(error, None)

  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_get_adb_devices_returns_devices(self, mock_get_adb_devices):
    mock_get_adb_devices.return_value = ["mock-device-serial",
                                         "mock-device-serial2"], None
    adbDevice = AdbDevice()

    devices, error = adbDevice.get_adb_devices()

    self.assertEqual(len(devices), 2)
    self.assertEqual(devices[0], "mock-device-serial")
    self.assertEqual(devices[1], "mock-device-serial2")
    self.assertEqual(error, None)

  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_adb_devices_fails_error(self,
      mock_get_adb_devices):
    mock_get_adb_devices.return_value = None, ValidationError(("Command 'adb"
                                                               " devices'"
                                                               " failed."),
                                                              None)
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                   None, None, None, None, None, None, None,
                                   None, None)

    error = adbDevice.check_device_connection(mock_command)

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, "Command 'adb devices' failed.")
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_no_devices_connected_error(self,
      mock_get_adb_devices):
    mock_get_adb_devices.return_value = [], None
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                   None, None, None, None, None, None, None,
                                   None, None)

    error = adbDevice.check_device_connection(mock_command)

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, "There are currently no devices connected.")
    self.assertEqual(error.suggestion, None)

  @mock.patch.object(sys, "stdout", new_callable=StringIO)
  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_serial_arg_in_devices(self,
      mock_get_adb_devices, mock_terminal_output):
    mock_get_adb_devices.return_value = ["mock-device-serial"], None
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                   None, None, None, None, None, None, None,
                                   None, "mock-device-serial")

    error = adbDevice.check_device_connection(mock_command)

    self.assertEqual(error, None)
    self.assertEqual(mock_terminal_output.getvalue(), ("Connected to device with"
                                                    " serial"
                                                    " mock-device-serial.\n"))

  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_command_serial_arg_not_in_devices_error(
      self,
      mock_get_adb_devices):
    mock_get_adb_devices.return_value = ["mock-device-serial"], None
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                   None, None, None, None, None, None, None,
                                   None, "invalid-device-serial")

    error = adbDevice.check_device_connection(mock_command)

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, ("Device with serial"
                                     " invalid-device-serial is not"
                                     " connected."))
    self.assertEqual(error.suggestion, None)

  @mock.patch.dict(os.environ, {"ANDROID_SERIAL": "mock-device-serial"},
                   clear=True)
  @mock.patch.object(sys, "stdout", new_callable=StringIO)
  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_env_variable_in_devices(self,
      mock_get_adb_devices, mock_terminal_output):
    mock_get_adb_devices.return_value = ["mock-device-serial"], None
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                   None, None, None, None, None, None, None,
                                   None, None)

    error = adbDevice.check_device_connection(mock_command)

    self.assertEqual(error, None)
    self.assertEqual(mock_command.serial, "mock-device-serial")
    self.assertEqual(mock_terminal_output.getvalue(), ("Connected to device"
                                                       " with serial"
                                                       " mock-device-serial."
                                                       "\n"))

  @mock.patch.dict(os.environ, {"ANDROID_SERIAL": "invalid-device-serial"},
                   clear=True)
  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_env_variable_not_in_devices_error(self,
      mock_get_adb_devices):
    mock_get_adb_devices.return_value = ["mock-device-serial"], None
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                   None, None, None, None, None, None, None,
                                   None, None)

    error = adbDevice.check_device_connection(mock_command)

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, ("Device with serial invalid-device-serial"
                                     " is set as environment variable,"
                                     " ANDROID_SERIAL, but is not connected."))
    self.assertEqual(error.suggestion, None)

  @mock.patch.dict(os.environ, {}, clear=True)
  @mock.patch.object(sys, "stdout", new_callable=StringIO)
  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_only_one_device(self,
      mock_get_adb_devices, mock_terminal_output):
    mock_get_adb_devices.return_value = ["mock-device-serial"], None
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                 None, None, None, None, None, None, None,
                                 None, None)

    error = adbDevice.check_device_connection(mock_command)

    self.assertEqual(error, None)
    self.assertEqual(mock_command.serial, "mock-device-serial")
    self.assertEqual(mock_terminal_output.getvalue(), ("Connected to device"
                                                       " with serial"
                                                       " mock-device-serial."
                                                       "\n"))

  @mock.patch.dict(os.environ, {}, clear=True)
  @mock.patch.object(AdbDevice, "get_adb_devices", autospec=True)
  def test_check_connection_connection_multiple_devices_error(self,
      mock_get_adb_devices):
    mock_get_adb_devices.return_value = ["mock-device-serial",
                                         "mock-device-serial2"], None
    adbDevice = AdbDevice()
    mock_command = ProfilerCommand(None, None, None, None, None, None, None,
                                   None, None, None, None, None, None, None,
                                   None, None)

    error = adbDevice.check_device_connection(mock_command)

    self.assertNotEqual(error, None)
    self.assertEqual(error.message, ("There is more than one device currently"
                                     " connected."))
    self.assertEqual(error.suggestion, "Run one of the following commands to"
                                       " choose one of the connected devices:"
                                       "\n\t torq --serial mock-device-serial"
                                       "\n\t torq --serial mock-device-serial2")


if __name__ == '__main__':
  unittest.main()
