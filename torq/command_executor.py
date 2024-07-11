class CommandExecutor:
  def __init__(self):
    raise NotImplementedError

  def execute(self, command):
    error = command.validate()
    if error is not None:
      return error
    print("executing", command.get_type(), "command")
    return None


class ProfilerCommandExecutor(CommandExecutor):
  def __init__(self):
    pass

  def execute(self, profiler_command):
    super().execute(profiler_command)


class HWCommandExecutor(CommandExecutor):
  def __init__(self):
    pass

  def execute(self, hw_command):
    super().execute(hw_command)


class ConfigCommandExecutor(CommandExecutor):
  def __init__(self):
    pass

  def execute(self, config_command):
    super().execute(config_command)