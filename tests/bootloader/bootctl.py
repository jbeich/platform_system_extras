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

class Bootctl(object):
    def __init__(self, device):
        self.device = device
        self.base = ["bootctl"]

    def _exec(self, cmd):
        return self.device.shell_nocheck(self.base + [cmd])

    # returns number of slots
    def get_number_slots(self):
        return int(self._exec("get-number-slots")[1])

    # returns current slot number
    def get_current_slot(self):
        return int(self._exec("get-current-slot")[1])

    # returns true on success, false on failure
    def mark_boot_successful(self):
        return self._exec("mark-boot-successful")[0] == 0

    # returns true on success, false on failure
    def set_active_boot_slot(self, slot):
        return self._exec("set-active-boot-slot " + str(slot))[0] == 0

    # returns true on success, false on failure
    def set_slot_as_unbootable_slot(self, slot):
        return self._exec("set-slot-as-unbootable " + str(slot))[0] == 0

    # true if slot is bootable
    def is_slot_bootable(self, slot):
        return self._exec("is-slot-bootable " + str(slot))[0] == 0

    # returns true on success, false on failure
    def is_slot_marked_successful(self, slot):
        return self._exec("is-slot-marked-successful " + str(slot))[0] == 0

    # returns suffix string for specified slot number
    def get_suffix(self, slot):
        return self._exec("get-suffix " + str(slot))[1].strip()
