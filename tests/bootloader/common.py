import os
import subprocess
import unittest

SERIAL = os.getenv("BOOTLOADER_TEST_SERIAL")

class ShellTest(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(ShellTest, self).__init__(*args, **kwargs)
        self.serial = SERIAL

    def adb(self, cmd):
        return subprocess.check_output("adb " + cmd,
                stderr=subprocess.STDOUT)

    def adb_shell(self, cmd):
        return subprocess.check_output("adb -s %s shell " + cmd,
                stderr=subprocess.STDOUT)
