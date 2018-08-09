#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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

import argparse
import logging
import os
import subprocess
import sys

def RunCommand(cmd, env):
  """Echo and run the given command.

  Args:
    cmd: the command represented as a list of strings.
    env: a dictionary of additional environment variables.
    verbose: show commands being executed.
  Returns:
    A tuple of the output and the exit code.
  """
  env_copy = os.environ.copy()
  env_copy.update(env)

  logging.info("Env: " + str(env))
  logging.info("Running: " + " ".join(cmd))

  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       env=env_copy)
  output, _ = p.communicate()

  return output, p.returncode


def main():
  logging.basicConfig(level=logging.INFO)

  parser = argparse.ArgumentParser(
    description=__doc__,
    formatter_class=argparse.RawDescriptionHelpFormatter
  )
  parser.add_argument("src_dir", help="source directory")
  parser.add_argument("output_file", help="output file")
  parser.add_argument("ext_variant", help="extension variant")
  parser.add_argument("mount_point", help="mount point")
  parser.add_argument("fs_size", help="size of partition")
  parser.add_argument("file_contexts", nargs='?', help="file context")

  parser.add_argument("--android_sparse", "-s", action="store_true", help="android sparse image")
  parser.add_argument("--journal_size", "-j", help="journal size")
  parser.add_argument("--timestamp", "-T", help="timetamp")
  parser.add_argument("--fs_config", "-C", help="fs configuration")
  parser.add_argument("--product_out", "-D", help="product out")
  parser.add_argument("--block_list_file", "-B", help="block list file")
  parser.add_argument("--base_alloc_file_in", "-d", help="base alloc file in")
  parser.add_argument("--base_alloc_file_out", "-A", help="base alloc file out")
  parser.add_argument("--label", "-L", help="label")
  parser.add_argument("--inodes", "-i", help="inodes")
  parser.add_argument("--rsv_pct", "-M", help="rsv pct")
  parser.add_argument("--erase_block_size", "-e", help="erase block size")
  parser.add_argument("--flash_block_size", "-o", help="flash block size")
  parser.add_argument("--mke2fs_uuid", "-U", help="mke2fs uuid")
  parser.add_argument("--mke2fs_hash_seed", "-S", help="mke2fs hash seed")
  parser.add_argument("--ext4_share_dup_blocks", "-c", action="store_true", help="ext4 share dup blocks")

  args = parser.parse_args()

  #print(vars(args))

  BLOCKSIZE = 4096

  if not os.path.isdir(args.src_dir):
    logging.error("Can not find directory {}".format(args.src_dir))
    sys.exit(2)
  if not args.ext_variant in ["ext2", "ext4"]:
    logging.error("Only ext2/4 are supported, got {}".format(args.ext_variant))
    sys.exit(3)
  if not args.mount_point:
    logging.error("Mount point is required")
    sys.exit(2)
  if args.mount_point[0] != '/':
    args.mount_point = '/' + args.mount_point
  if not args.fs_size:
    logging.error("Size of the filesystem is required")
    sys.exit(2)

  e2fsdroid_opts = []
  mke2fs_extended_opts =[]
  mke2fs_opts =[]

  if args.android_sparse:
    mke2fs_extended_opts.append("android_sparse")
  else:
    e2fsdroid_opts.append("-e")
  if args.timestamp:
    e2fsdroid_opts += ["-T", args.timestamp]
  if args.fs_config:
    e2fsdroid_opts += ["-C", args.fs_config]
  if args.product_out:
    e2fsdroid_opts += ["-D", args.product_out]
  if args.block_list_file:
    e2fsdroid_opts += ["-B", args.block_list_file]
  if args.base_alloc_file_in:
    e2fsdroid_opts += ["-d", args.base_alloc_file_in]
  if args.base_alloc_file_out:
    e2fsdroid_opts += ["-D", args.base_alloc_file_out]
  if args.label:
    e2fsdroid_opts += ["-L", args.label]
  if args.inodes:
    e2fsdroid_opts += ["-N", args.inodes]
  if args.rsv_pct:
    e2fsdroid_opts += ["-m", args.rsv_pct]
  if args.ext4_share_dup_blocks:
    e2fsdroid_opts.append("-s")
  if args.file_contexts:
    e2fsdroid_opts += ["-S", args.file_contexts]

  if args.erase_block_size:
    mke2fs_extended_opts.append("stripe_width={}".format(
        int(args.erase_block_size) / BLOCKSIZE))
  if args.flash_block_size:
    # stride should be the max of 8kb and the logical block size
    stride = max(int(args.flash_block_size), 8192)
    mke2fs_extended_opts.append("stride={}".format(stride))
  if args.mke2fs_hash_seed:
   mke2fs_extended_opts.append("hash_seed=" + args.mke2fs_hash_seed)

  if args.journal_size:
    if args.journal_size == "0":
      mke2fs_opts += ["-O", "^has_journal"]
    else:
      mke2fs_opts += ["-J", "size=" + args.journal_size]
  if args.mke2fs_uuid:
    mke2fs_opts += ["-U", args.mke2fs_uuid]
  if mke2fs_extended_opts:
    mke2fs_opts += ["-E", ','.join(mke2fs_extended_opts)]


  # run mke2fs
  make_ext4fs_env = {"MKE2FS_CONFIG" : "./system/extras/ext4_utils/mke2fs.conf"}
  if args.timestamp:
    make_ext4fs_env["E2FSPROGS_FAKE_TIME"] = args.timestamp
  # Round down the filesystem length to be a multiple of the block size
  blocks = int(args.fs_size) / BLOCKSIZE
  make_ext4fs_cmd = ["mke2fs"] + mke2fs_opts + [
    "-t", args.ext_variant, "-b", str(BLOCKSIZE), args.output_file, str(blocks)]

  output, ret = RunCommand(make_ext4fs_cmd, make_ext4fs_env)
  if ret != 0:
    logging.error("Failed to run mke2fs: " + output)
    sys.exit(4)

  # run e2fsdroid
  e2fsdroid_env = {}
  if args.timestamp:
    e2fsdroid_env["E2FSPROGS_FAKE_TIME"] = args.timestamp

  e2fsdroid_cmd = ["e2fsdroid"] + e2fsdroid_opts + [
    "-f", args.src_dir, "-a", args.mount_point, args.output_file]

  output, ret = RunCommand(e2fsdroid_cmd, e2fsdroid_env)
  if ret != 0:
    logging.error("Failed to run e2fsdroid_cmd: " + output)
    sys.exit(4)


if __name__ == '__main__'  :
  main()

