class ProfilerCommandExecutor:
  def __init__(self):
    pass

  def execute(self, profilerCommand):
    error = profilerCommand.validate()
    if error is not None:
      return error
    print("executing profile command")
    return None


class HWCommandExecutor:
  def __init__(self):
    pass

  def execute(self, hwCommand):
    error = hwCommand.validate()
    if error is not None:
      return error
    print("executing hw command")
    return None


class ConfigCommandExecutor:
  def __init__(self):
    pass

  def execute(self, configCommand):
    error = configCommand.validate()
    if error is not None:
      return error
    print("executing config command")
    return None
