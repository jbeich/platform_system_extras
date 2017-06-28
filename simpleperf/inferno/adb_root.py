from adb import Adb
import subprocess

class AdbRoot(Adb):
    def collect_data(self, process):
        subprocess.call(["adb", "shell", process.cmd])
        return True