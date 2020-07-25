#!/usr/bin/env python3

# Copyright (C) 2020 The Android Open Source Project
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
import sys
import tempfile
from zipfile import ZipFile


def CreatePayload(brillo_update_payload, delta_generator, input, output):
  tf = None
  try:
    fd, tf = tempfile.mkstemp("target_files", ".zip")
    os.close(fd)
    with ZipFile(tf, "w") as zip:
      zip.write(input, arcname=os.path.join("IMAGES", os.path.basename(input)))
      zip.writestr("META/ab_partitions.txt", "boot\n")
    env = os.environ
    env["GENERATOR"] = delta_generator
    subprocess.check_call([brillo_update_payload, "generate",
                           "--is_partial_update", "true",
                           "--target_image", tf,
                           "--payload", output], env=env)
  finally:
    if tf is not None:
      os.remove(tf)


if __name__ == "__main__":
  if len(sys.argv) != 5:
    sys.write(sys.stderr, "usage: {} brillo_update_payload boot.img payload.bin\n")
    sys.exit(1)
  CreatePayload(*sys.argv[1:])
