class ProfilerCommandExecutor:
  def __init__(self):
    pass

  def execute(self, profiler_command):
    error = profiler_command.validate()
    if error is not None:
      return error
    print("executing profile command")
    return None


class HWCommandExecutor:
  def __init__(self):
    pass

  def execute(self, hw_command):
    error = hw_command.validate()
    if error is not None:
      return error
    print("executing hw command")
    return None


class ConfigCommandExecutor:
  def __init__(self):
    pass

  def execute(self, config_command):
    error = config_command.validate()
    if error is not None:
      return error
    print("executing config command")
    return None
