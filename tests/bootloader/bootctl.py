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
