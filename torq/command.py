from command_executor import ProfilerCommandExecutor, HWCommandExecutor,\
  ConfigCommandExecutor


class Command:
  def __init__(self, type):
    self.type = type

  def get_type(self):
    return self.type

  def execute(self):
    pass
    raise NotImplementedError

  def validate(self):
    pass
    raise NotImplementedError


class ProfilerCommand(Command):
  def __init__(self, event, profiler, out_dir, dur_ms, app, runs,
      simpleperf_event, perfetto_config, between_dur_ms, ui,
      exclude_ftrace_event, include_ftrace_event, from_user, to_user):
    self.event = event
    self.profiler = profiler
    self.out_dir = out_dir
    self.dur_ms = dur_ms
    self.app = app
    self.runs = runs
    self.simpleperf_event = simpleperf_event
    self.perfetto_config = perfetto_config
    self.between_dur_ms = between_dur_ms
    self.ui = ui
    self.exclude_ftrace_event = exclude_ftrace_event
    self.include_ftrace_event = include_ftrace_event
    self.from_user = from_user
    self.to_user = to_user

  def execute(self):
    command_executor = ProfilerCommandExecutor()
    command_executor.execute(self)

  def validate(self):
    print("Further validating arguments of ProfilerCommand.")
    return None


class HWCommand(Command):
  def __init__(self, type, config, num_cpus, memory):
    super().__init__(type)
    self.config = config
    self.num_cpus = num_cpus
    self.memory = memory

  def execute(self):
    command_executor = HWCommandExecutor()
    command_executor.execute(self)

  def validate(self):
    print("Further validating arguments of HWCommand.")
    return None


class ConfigCommand(Command):
  def __init__(self, type, config_name, file_path):
    super().__init__(type)
    self.config_name = config_name
    self.file_path = file_path

  def execute(self):
    conmand_executor = ConfigCommandExecutor()
    conmand_executor.execute(self)

  def validate(self):
    print("Further validating arguments of ConfigCommand.")
    return None
