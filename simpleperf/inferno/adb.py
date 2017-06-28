import subprocess
import abc


class Adb:

    def __init__(self):
        pass


    def delete_previous_data(self):
        err = subprocess.call(["adb", "shell", "rm", "-f", "/data/local/tmp/perf.data"])


    def get_process_pid(self, process_name):
        piof_output = subprocess.check_output(["adb", "shell", "pidof", process_name])
        try:
            process_id = int(piof_output)
        except ValueError:
            process_id = 0
        return process_id


    def pull_data(self):
        err = subprocess.call(["adb", "pull", "/data/local/tmp/perf.data", "."])
        return err


    @abc.abstractmethod
    def collect_data(self, simpleperf_command):
        raise NotImplementedError("%s.collect_data(str) is not implemented!" % self.__class__.__name__)


    def get_props(self):
        props = {}
        output = subprocess.check_output(["adb", "shell", "getprop"])
        lines = output.split("\n")
        for line in lines:
            tokens = line.split(": ")
            if len(tokens) < 2:
                continue
            key = tokens[0].replace("[", "").replace("]", "")
            value = tokens[1].replace("[", "").replace("]", "")
            props[key] = value
        return props
