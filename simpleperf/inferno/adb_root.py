from adb import Adb
import subprocess

class AdbRoot(Adb):
    def collect_data(self, process):
        if process.args.push_binary:
            self.push_simpleperf_binary()
        subprocess.call(["adb", "shell", "cd /data/local/tmp; " + process.cmd])
        return True