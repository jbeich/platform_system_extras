import subprocess
import unittest

SERIAL = sys.getenv("BOOTLOADER_TEST_SERIAL")

class ShellTest(unittest.TestCase):
    def __init__(self):
        self.serial = SERIAL

    def adb(self, cmd):
        return subprocess.check_output("adb " + cmd,
                stderr=subprocess.STDOUT)

    def adb_shell(self, cmd):
        return subprocess.check_output("adb -s %s shell " + cmd,
                stderr=subprocess.STDOUT)
