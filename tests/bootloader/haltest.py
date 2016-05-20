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

import bootctl
import common
import sys
import unittest

class HalTest(common.ShellTest):
    def __init__(self, *args, **kwargs):
        super(HalTest, self).__init__(*args, **kwargs)
        self.bootctl = bootctl.Bootctl(self.device)

    def test_slots(self):
        num_slots = self.bootctl.get_number_slots()
        suffixes = dict()
        for slot in range(num_slots):
            suffix = self.bootctl.get_suffix(slot)
            self.assertNotEqual(suffix, "(null)")
            suffixes[suffix] = slot
        self.assertEqual(len(suffixes), num_slots)

    def test_mark_successful(self):
        self.device.root()
        self.device.wait()
        slot = self.bootctl.get_current_slot()
        self.assertEqual(0, self.bootctl.mark_boot_successful())
        self.assertEqual(0, self.bootctl.is_slot_marked_successful(slot))

    def test_switch_slots(self):
        # Cycle through all slots once
        num_slots = self.bootctl.get_number_slots()
        for i in range(num_slots):
            slot = self.bootctl.get_current_slot()
            new_slot = (slot + 1) % num_slots
            self.device.root()
            self.device.wait()
            self.assertEqual(0, self.bootctl.set_active_boot_slot(new_slot))
            slot2 = self.bootctl.get_current_slot()
            self.assertEqual(slot, slot2)
            self.device.reboot()
            self.device.wait()
            self.assertEqual(new_slot, self.bootctl.get_current_slot())

    def test_unbootable(self):
        # Cycle through all slots once
        num_slots = self.bootctl.get_number_slots()
        for i in range(num_slots):
            slot = self.bootctl.get_current_slot()
            new_slot = (slot + 1) % num_slots
            self.device.root()
            self.device.wait()
            self.assertEqual(0, self.bootctl.set_active_boot_slot(new_slot))
            self.assertEqual(0, self.bootctl.is_slot_bootable(new_slot))
            self.assertEqual(0, self.bootctl.set_slot_as_unbootable_slot(new_slot))
            self.assertEqual(70, self.bootctl.is_slot_bootable(new_slot))
            self.device.reboot()
            self.device.wait()
            self.device.root()
            self.device.wait()
            self.assertEqual(slot, self.bootctl.get_current_slot())
            self.assertEqual(70, self.bootctl.is_slot_bootable(new_slot))
            self.assertEqual(0, self.bootctl.set_active_boot_slot(new_slot))
            self.assertEqual(0, self.bootctl.is_slot_bootable(new_slot))
            self.device.reboot()
            self.device.wait()
            self.assertEqual(new_slot, self.bootctl.get_current_slot());

if __name__ == '__main__':
    unittest.main(verbosity=3)
