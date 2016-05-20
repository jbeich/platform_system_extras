# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import subprocess
import unittest

class ShellTest(unittest.TestCase):
    def __init__(self, serial=None, *args, **kwargs):
        super(ShellTest, self).__init__(*args, **kwargs)
        self.serial = serial
        if self.serial is None:
            self.serial = os.getenv("BOOTLOADER_TEST_SERIAL")

    def adb(self, cmd):
        return subprocess.check_output("adb " + cmd,
                stderr=subprocess.STDOUT)

    def adb_shell(self, cmd):
        return subprocess.check_output("adb -s %s shell " + cmd,
                stderr=subprocess.STDOUT)
