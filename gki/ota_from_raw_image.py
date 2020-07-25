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

"""
Generate payload.bin from a single image.
"""

import argparse
import os
import subprocess
import tempfile
from zipfile import ZipFile


def CreatePayload(args):
  tf = None
  try:
    fd, tf = tempfile.mkstemp("target_files", ".zip")
    os.close(fd)
    with ZipFile(tf, "w") as zip:
      names = []
      for pair_str in args.input:
        pair = pair_str.split(":")
        assert len(pair) == 2, "Incorrect format: " + pair_str
        name, img_path = tuple(pair)
        zip.write(img_path, arcname=os.path.join("IMAGES", name + ".img"))
        names.append(name)
      zip.writestr("META/ab_partitions.txt", "\n".join(names) + "\n")
    env = os.environ
    env["GENERATOR"] = args.delta_generator
    subprocess.check_call([args.brillo_update_payload, "generate",
                           "--is_partial_update", "true",
                           "--target_image", tf,
                           "--payload", args.out], env=env)
  finally:
    if tf is not None:
      os.remove(tf)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--brillo_update_payload", type=str, default="brillo_update_payload",
                      help="Path to brillo_update_payload binary")
  parser.add_argument("--delta_generator", type=str, default="delta_generator",
                      help="Path to delta_generator binary")
  parser.add_argument("--out", type=str, required=True,
                      help="Required output path to payload.bin")
  parser.add_argument("input", metavar="NAME:IMAGE", nargs="+",
                      help="Name of the image and path to the image, e.g. boot:path/to/boot.img")
  args = parser.parse_args()
  CreatePayload(args)
