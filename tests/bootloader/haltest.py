import bootctl
import common
import sys

class HalTest(common.ShellTest):
    def __init__(self):
        self.bootctl = bootctl.Bootctl(self.serial)

    def test_mark_successful(self):
        slot = self.bootctl.get_current_slot()
        self.assertEqual(1, len(slot))
        status = self.bootctl.mark_boot_successful()
        self.assertEqual(0, len(status))
        self.assertEqual("0", self.bootctl.is_slot_marked_successful(slot))

    def test_switch_slots(self):
        # run twice; once to switch from A to B and once to switch back (WLoG)
        for i in (1,1):
            slot = self.bootctl.get_current_slot()
            self.assertEqual(1, len(slot))
            slot = int(slot)
            status = self.bootctl.set_active_boot_slot(1 - slot)
            assertEqual(0, len(status))
            self.adb("reboot")
            self.adb("wait-for-device")
            self.assertEqual(`1 - slot`, self.bootctl.get_current_slot())
