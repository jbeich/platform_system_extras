from adb import Adb
import subprocess
import time
import os

BIN_PATH = "../scripts/bin/android/%s/simpleperf"

class Abi:
    ARM    = 1
    ARM_64 = 2
    X86    = 3
    X86_64 = 4

    def __init__(self):
        pass

class AdbNonRoot(Adb):

    def parse_abi(self, str):
        if str.index("arm64") != -1:
            return Abi.ARM_64
        if str.index("arm") != -1:
            return Abi.ARM
        if str.index("x86_64") != -1:
            return Abi.X86_64
        if str.index("x86") != -1:
            return Abi.X86
        return Abi.ARM_64

    def get_exec_path(self, abi):
        folder_name = "arm64"
        if abi == Abi.ARM:
            folder_name = "arm"
        if abi == Abi.X86:
            folder_name = "x86"
        if abi == Abi.X86_64:
            folder_name = "x86_64"
        return os.path.join(os.path.dirname(__file__), BIN_PATH % folder_name)


    # If adb cannot run as root, there is still a way to collect data but it is much more complicated.
    # 1. Identify the platform abi, use getprop:  ro.product.cpu.abi
    # 2. Push the precompiled scripts/bin/android/[ABI]/simpleperf to device /data/local/tmp/simpleperf
    # 4. Use run-as to copy /data/local/tmp/simplerperf -> /apps/installation_path/simpleperf
    # 5. Use run-as to run: /apps/installation_path/simpleperf -p APP_PID -o /apps/installation_path/perf.data
    # 6. Use run-as fork+pipe trick to copy /apps/installation_path/perf.data to /data/local/tmp/perf.data
    def collect_data(self, process):

        # Detect the ABI of the device
        props = self.get_props()
        abi_raw = props["ro.product.cpu.abi"]
        abi = self.parse_abi(abi_raw)
        exec_path = self.get_exec_path(abi)

        # Push simpleperf to the device
        print "Pushing local '%s' to device." % exec_path
        subprocess.call(["adb", "push", exec_path, "/data/local/tmp/simpleperf"])

        # Copy simpleperf to the data
        subprocess.check_output(["adb", "shell", "run-as %s" % process.name, "cp", "/data/local/tmp/simpleperf", "."])

        # Patch command to run with path to data folder where simpleperf was written.
        process.cmd = process.cmd.replace("/data/local/tmp/perf.data", "./perf.data")

        # Start collecting samples.
        process.cmd = ("run-as %s " % process.name) + process.cmd
        subprocess.call(["adb", "shell", process.cmd])

        # Wait sampling_duration+1.5 seconds.
        time.sleep(int(process.args.capture_duration) + 1)

        # Move data to a location where shell user can read it.
        subprocess.call(["adb", "shell", "run-as %s cat perf.data | tee /data/local/tmp/perf.data >/dev/null" % (process.name)])

        return True
