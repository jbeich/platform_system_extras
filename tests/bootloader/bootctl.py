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

import subprocess

class Bootctl(object):
    def __init__(self, serial):
        if not serial:
            self.base = ["adb", "shell", "bootctl"]
        else:
            self.base = ["adb", "-s", serial, "shell", "bootctl"]

    def _exec(self, cmd):
        return subprocess.check_output(self.base + [cmd],
                stderr=subprocess.STDOUT)

    def get_current_slot(self):
        return self._exec("get-current-slot")

    def mark_boot_successful(self):
        return self._exec("mark-boot-successful")

    def set_active_boot_slot(self, slot):
        return self._exec("set-active-boot-slot " + slot)

    def is_slot_marked_successful(self, slot):
        return self._exec("is-slot-marked-successful " + slot)
