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
from abc import ABC, abstractmethod
from config_builder import DefaultConfigBuilder


class CommandExecutor(ABC):
  """
  Abstract base class representing a command executor.
  """
  def __init__(self):
    pass

  def execute(self, command, device):
    error = device.check_device_connection()
    if error is not None:
      return error
    error = command.validate(device)
    if error is not None:
      return error
    return self.execute_command(command, device)

  @abstractmethod
  def execute_command(self, command, device):
    raise NotImplementedError


class ProfilerCommandExecutor(CommandExecutor):

  def execute_command(self, command, device):
    config, error = self.create_config(command)
    if error is not None:
      return error
    error = self.prepare_device(command, device, config)
    if error is not None:
      return error
    for run in range(command.runs):
      error = self.prepare_device_for_run(command, device, run + 1)
      if error is not None:
        return error
      error = self.execute_run(command, device, config, run + 1)
      if error is not None:
        return error
      error = self.retrieve_perf_data(command, device)
      if error is not None:
        return error
      if command.runs != run + 1:
        time.sleep(command.between_dur_ms / 1000)
    error = self.cleanup(command, device)
    if error is not None:
      return error
    if command.ui is True:
      return self.open_ui(command)
    return None

  def create_config(self, command):
    config_builder = self.get_config_builder_type(command.perfetto_config)
    config, error = config_builder.build_config(command)
    if error is not None:
      return None, error
    return config, None

  def get_config_builder_type(self, perfetto_config):
    from torq import PREDEFINED_PERFETTO_CONFIGS
    config_builder = None
    if perfetto_config == PREDEFINED_PERFETTO_CONFIGS[0]:
      config_builder = DefaultConfigBuilder()
    elif perfetto_config == PREDEFINED_PERFETTO_CONFIGS[1]:
      raise NotImplementedError
    elif perfetto_config == PREDEFINED_PERFETTO_CONFIGS[2]:
      raise NotImplementedError
    else:
      raise NotImplementedError
    return config_builder

  def prepare_device(self, command, device, config):
    return None

  def prepare_device_for_run(self, command, device, current_run):
    return None

  def execute_run(self, command, device, config, current_run):
    print("Performing run %s" % current_run)
    return None

  def trigger_system_event(self, command, device):
    return None

  def retrieve_perf_data(self, command, device):
    return None

  def cleanup(self, command, device):
    return None

  def open_ui(self, command):
    return None


class HWCommandExecutor(CommandExecutor):

  def execute_command(self, hw_command, device):
    match hw_command.get_type():
      case "hw set":
        return self.execute_hw_set_command(device, hw_command.hw_config,
                                           hw_command.num_cpus,
                                           hw_command.memory)
      case "hw get":
        return self.execute_hw_get_command(device)
      case "hw list":
        return self.execute_hw_list_command(device)
      case _:
        raise Exception("Invalid hw subcommand was used.")

  def execute_hw_set_command(self, device, hw_config, num_cpus, memory):
    return None

  def execute_hw_get_command(self, device):
    return None

  def execute_hw_list_command(self, device):
    return None


class ConfigCommandExecutor(CommandExecutor):

  def execute(self, command, device):
    return self.execute_command(command, device)

  def execute_command(self, config_command, device):
    match config_command.get_type():
      case "config list":
        return self.execute_config_list_command()
      case "config show":
        return self.execute_config_show_command(config_command.config_name)
      case "config pull":
        return self.execute_config_pull_command(config_command.config_name,
                                                config_command.file_path)
      case _:
        raise Exception("Invalid config subcommand was used.")

  def execute_config_list_command(self):
    return None

  def execute_config_show_command(self, config_name):
    return None

  def execute_config_pull_command(self, config_name, file_path):
    return None
