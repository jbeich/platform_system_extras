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

import subprocess
import os
from abc import ABC, abstractmethod
from validation_error import ValidationError


class Device(ABC):
  """
  Abstract base class representing a device. This class defines the APIs
  needed to interact with the current device.
  """

  @abstractmethod
  def __init__(self):
    raise NotImplementedError

  @abstractmethod
  def get_adb_devices(self):
    raise NotImplementedError

  @abstractmethod
  def check_device_connection(self, command):
    raise NotImplementedError

  @abstractmethod
  def get_num_cpus(self):
    raise NotImplementedError

  @abstractmethod
  def get_memory(self):
    raise NotImplementedError

  @abstractmethod
  def get_max_num_cpus(self):
    raise NotImplementedError

  @abstractmethod
  def get_max_memory(self):
    raise NotImplementedError

  @abstractmethod
  def set_hw_config(self, hw_config):
    raise NotImplementedError

  @abstractmethod
  def set_num_cpus(self, num_cpus):
    raise NotImplementedError

  @abstractmethod
  def set_memory(self, memory):
    raise NotImplementedError

  @abstractmethod
  def app_exists(self, app):
    raise NotImplementedError

  @abstractmethod
  def simpleperf_event_exists(self, simpleperf_event):
    raise NotImplementedError

  @abstractmethod
  def user_exists(self, user):
    raise NotImplementedError


class AdbDevice(Device):
  """
  Class representing a device. APIs interact with the current device through
  the adb bridge.
  """
  def __init__(self):
    pass

  def get_adb_devices(self):
    command = None
    try:
      command = subprocess.run(["adb", "devices"], capture_output=True)
    except Exception as e:
      return None, ValidationError("Command 'adb devices' failed.", None)
    command_output_lines = command.stdout.decode("utf-8").split("\n")[1:-2]
    devices = []
    for line in command_output_lines:
      words_in_line = line.split('\t')
      if words_in_line[1] == "device":
        devices.append(words_in_line[0])
    return devices, None

  def check_device_connection(self, command):
    devices, error = self.get_adb_devices()
    if error is not None:
      return error
    if len(devices) == 0:
      return ValidationError("There are currently no devices connected.", None)
    if command.serial is not None:
      if command.serial in devices:
        print("Connected to device with serial %s." % command.serial)
        return None
      else:
        return ValidationError(("Device with serial %s is not connected."
                                % command.serial), None)
    else:
      if "ANDROID_SERIAL" in os.environ:
        if os.environ["ANDROID_SERIAL"] in devices:
          print(("Connected to device with serial %s."
                % os.environ["ANDROID_SERIAL"]))
          command.serial = os.environ["ANDROID_SERIAL"]
          return None
        else:
          return ValidationError(("Device with serial %s is set as environment"
                                  " variable, ANDROID_SERIAL, but is not"
                                  " connected."
                                  % os.environ["ANDROID_SERIAL"]), None)
      else:
        if len(devices) == 1:
          command.serial = devices[0]
          print("Connected to device with serial %s." % devices[0])
          return None
        else:
          return ValidationError(("There is more than one device currently"
                                  " connected."),
                                 ("Run one of the following commands to choose"
                                  " one of the connected devices:\n"
                                  "\t torq --serial %s"
                                  % "\n\t torq --serial ".join(devices)))

  def get_num_cpus(self):
    raise NotImplementedError

  def get_memory(self):
    raise NotImplementedError

  def get_max_num_cpus(self):
    raise NotImplementedError

  def get_max_memory(self):
    raise NotImplementedError

  def set_hw_config(self, hw_config):
    raise NotImplementedError

  def set_num_cpus(self, num_cpus):
    raise NotImplementedError

  def set_memory(self, memory):
    raise NotImplementedError

  def app_exists(self, app):
    raise NotImplementedError

  def simpleperf_event_exists(self, simpleperf_event):
    raise NotImplementedError

  def user_exists(self, user):
    raise NotImplementedError
