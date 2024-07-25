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

import time


class CommandExecutor:
  def __init__(self):
    raise NotImplementedError

  def execute(self, command, device):
    error = device.check_device_connection()
    if error is not None:
      return error
    error = command.validate(device)
    if error is not None:
      return None
    print("executing", command.get_type(), "command")
    error = self.execute_command(command, device)
    if error is not None:
      return error
    return None


class ProfilerCommandExecutor(CommandExecutor):
  def __init__(self):
    pass

  def execute_command(self, profiler_command, device):
    for i in range(profiler_command.runs):
      print("Performing run %s" % (i + 1))
      error = self.prepare_device(device, profiler_command)
      if error is not None:
        return error
      error = self.start_event(device, profiler_command)
      if error is not None:
        return error
      error = self.retrieve_perf_data(device, profiler_command)
      if error is not None:
        return error
      error = self.open_ui(device, profiler_command)
      if error is not None:
        return error
      if profiler_command.ui == True:
        error = self.cleanup(device, profiler_command)
        if error is not None:
          return error

      if profiler_command.runs > 1:
        time.sleep(profiler_command.between_dur_ms / 1000)
    return None

  def prepare_device(self, device, profiler_command):
    return None

  def start_event(self, device, profiler_command):
    return None

  def retrieve_perf_data(self, device, profiler_command):
    return None

  def cleanup(self, device, profiler_command):
    return None

  def open_ui(self, device, profiler_command):
    return None


class HWCommandExecutor(CommandExecutor):
  def __init__(self):
    pass

  def execute_command(self, hw_command, device):
    if hw_command.get_type() == "hw set":
      error = self.execute_hw_set_command(device, hw_command.hw_config,
                                          hw_command.num_cpus,
                                          hw_command.memory)
      if error is not None:
        return error
    if hw_command.get_type() == "hw get":
      error = self.execute_hw_get_command(device)
      if error is not None:
        return error
    if hw_command.get_type() == "hw list":
      error = self.execute_hw_list_command(device)
      if error is not None:
        return error
    return None

  def execute_hw_set_command(self, device, hw_config, num_cpus, memory):
    return None

  def execute_hw_get_command(self, device):
    return None

  def execute_hw_list_command(self, device):
    return None


class ConfigCommandExecutor(CommandExecutor):
  def __init__(self):
    pass

  def execute_command(self, config_command, device):
    if config_command.get_type() == "config list":
      error = self.execute_config_list_command(device)
      if error is not None:
        return error
    if config_command.get_type() == "config show":
      error = self.execute_config_show_command(device,
                                               config_command.config_name)
      if error is not None:
        return error
    if config_command.get_type() == "config pull":
      error = self.execute_config_pull_command(device,
                                               config_command.config_name,
                                               config_command.file_path)
      if error is not None:
        return error
    return None

  def execute_config_list_command(self, device):
    return None

  def execute_config_show_command(self, device, config_name):
    return None

  def execute_config_pull_command(self, device, config_name, file_path):
    return None