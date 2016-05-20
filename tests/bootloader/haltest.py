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

class HalTest(common.ShellTest):
    def __init__(self, *args, **kwargs):
        super(HalTest, self).__init__(*args, **kwargs)
        self.bootctl = bootctl.Bootctl(self.serial)

    def test_mark_successful(self):
        slot = self.bootctl.get_current_slot()
        self.assertEqual(1, len(slot))
        status = self.bootctl.mark_boot_successful()
        self.assertEqual(0, len(status))
        self.assertEqual("0", self.bootctl.is_slot_marked_successful(slot))

    def test_switch_slots(self):
        # run twice; once to switch from A to B and once to switch back (WLoG)
        for i in (1, 1):
            slot = self.bootctl.get_current_slot()
            self.assertEqual(1, len(slot))
            slot = int(slot)
            status = self.bootctl.set_active_boot_slot(1 - slot)
            assertEqual(0, len(status))
            self.device.reboot()
            self.device.wait()
            self.assertEqual(repr(1 - slot), self.bootctl.get_current_slot())
