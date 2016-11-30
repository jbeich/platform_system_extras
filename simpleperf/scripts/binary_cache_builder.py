#!/usr/bin/env python
#
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
#

"""build_binary_cache.py: read perf.data, collect binaries needed by
    it, and put them in binary_cache.
"""

from __future__ import print_function
import os
import os.path
import re
import shutil
import subprocess
import sys
import time

from simpleperf_report_lib import *
from utils import *


class BinaryCacheBuilder(object):
    """Collect all binaries needed by perf.data in binary_cache."""
    def __init__(self, config):
        config_names = ['perf_data_path', 'symfs_dirs', 'adb_path',
                        'readelf_path', 'binary_cache_dir']
        for name in config_names:
            if not config.has_key(name):
                log_fatal('config for "%s" is missing' % name)

        self.perf_data_path = config.get('perf_data_path')
        if not os.path.isfile(self.perf_data_path):
            log_fatal("can't find file %s" % self.perf_data_path)
        self.symfs_dirs = config.get('symfs_dirs')
        for symfs_dir in self.symfs_dirs:
            if not os.path.isdir(symfs_dir):
                log_fatal("symfs_dir '%s' is not a directory" % symfs_dir)
        self.adb = AdbHelper(config['adb_path'])
        self.readelf_path = config['readelf_path']
        self.binary_cache_dir = config['binary_cache_dir']
        if not os.path.isdir(self.binary_cache_dir):
            os.makedirs(self.binary_cache_dir)


    def build_binary_cache(self):
        self._collect_used_binaries()
        self._copy_binaries_from_symfs_dirs()
        self._pull_binaries_from_device()
        self._pull_kernel_symbols()


    def _collect_used_binaries(self):
        """read perf.data, collect all used binaries and their build id (if available)."""
        # A dict mapping from binary name to build_id
        binaries = dict()
        lib = ReportLib()
        lib.SetRecordFile(self.perf_data_path)
        lib.SetLogSeverity('error')
        while True:
            sample = lib.GetNextSample()
            if sample is None:
                lib.Close()
                break
            symbols = [lib.GetSymbolOfCurrentSample()]
            callchain = lib.GetCallChainOfCurrentSample()
            for i in range(callchain.nr):
                symbols.append(callchain.entries[i].symbol)

            for symbol in symbols:
                dso_name = symbol.dso_name
                if not binaries.has_key(dso_name):
                    binaries[dso_name] = lib.GetBuildIdForPath(dso_name)
        self.binaries = binaries


    def _copy_binaries_from_symfs_dirs(self):
        """collect all files in symfs_dirs."""
        if len(self.symfs_dirs) == 0:
            return

        # It is possible that the path of the binary in symfs_dirs doesn't match
        # the one recorded in perf.data. For example, a file in symfs_dirs might
        # be "debug/arm/obj/armeabi-v7a/libsudo-game-jni.so", but the path in
        # perf.data is "/data/app/xxxx/lib/arm/libsudo-game-jni.so". So we match
        # binaries if they have the same filename (like libsudo-game-jni.so)
        # and same build_id.

        # Map from filename to binary paths.
        filename_dict = dict()
        for binary in self.binaries:
            index = binary.rfind('/')
            filename = binary[index+1:]
            paths = filename_dict.get(filename)
            if paths is None:
                filename_dict[filename] = paths = []
            paths.append(binary)

        # Walk through all files in symfs_dirs, and copy matching files to build_cache.
        for symfs_dir in self.symfs_dirs:
            for root, dirs, files in os.walk(symfs_dir):
                for file in files:
                    paths = filename_dict.get(file)
                    if paths is not None:
                        build_id = self.read_build_id(os.path.join(root, file))
                        if len(build_id) == 0:
                            continue
                        for binary in paths:
                            expected_build_id = self.binaries.get(binary)
                            if expected_build_id == build_id:
                                self._copy_to_binary_cache(os.path.join(root, file), binary)
                                del self.binaries[binary]


    def _copy_to_binary_cache(self, from_path, target_file):
        if target_file[0] == '/':
            target_file = target_file[1:]
        target_file = target_file.replace('/', os.sep)
        target_file = os.path.join(self.binary_cache_dir, target_file)
        target_dir = os.path.dirname(target_file)
        if not os.path.isdir(target_dir):
            os.makedirs(target_dir)
        log_info('copy to binary_cache: %s to %s' % (from_path, target_file))
        shutil.copy(from_path, target_file)


    def _pull_binaries_from_device(self):
        """pull binaries needed in perf.data to binary_cache."""
        for binary in self.binaries:
            build_id = self.binaries[binary]
            if binary[0] != '/' or binary == "//anon":
                # [kernel.kallsyms] or unknown, or something we can't find binary.
                continue
            binary_cache_file = binary[1:].replace('/', os.sep)
            binary_cache_file = os.path.join(self.binary_cache_dir, binary_cache_file)
            self._check_and_pull_binary(binary, build_id, binary_cache_file)


    def _check_and_pull_binary(self, binary, expected_build_id, binary_cache_file):
        """If the binary_cache_file exists and has the expected_build_id, there
           is no need to pull the binary from device. Otherwise, pull it.
        """
        need_pull = True
        if os.path.isfile(binary_cache_file):
            need_pull = False
            if len(expected_build_id) > 0:
                build_id = self.read_build_id(binary_cache_file)
                if expected_build_id != build_id:
                    need_pull = True
        if need_pull:
            target_dir = os.path.dirname(binary_cache_file)
            if not os.path.isdir(target_dir):
                os.makedirs(target_dir)
            if os.path.isfile(binary_cache_file):
                os.remove(binary_cache_file)
            log_info('pull file to binary_cache: %s to %s' % (binary, binary_cache_file))
            self._pull_file_from_device(binary, binary_cache_file)
        else:
            log_info('use current file in binary_cache: %s' % binary_cache_file)


    def read_build_id(self, file):
        """read build id of a binary on host."""
        if len(self.readelf_path) == 0:
            return ""
        output = subprocess.check_output([self.readelf_path, '-n', file])
        result = re.search(r'Build ID:\s*(\S+)', output)
        if result:
            build_id = result.group(1)
            if len(build_id) < 40:
                build_id += '0' * (40 - len(build_id))
            build_id = '0x' + build_id
            return build_id
        return ""


    def _pull_file_from_device(self, device_path, host_path):
        if self.adb.run(['pull', device_path, host_path]):
            return True
        # In non-root device, we can't pull /data/app/XXX/base.odex directly.
        # Instead, we can first copy the file to /data/local/tmp, then pull it.
        filename = device_path[device_path.rfind('/')+1:]
        if self.adb.run(['shell', 'cp', device_path, '/data/local/tmp']) and \
            self.adb.run(['pull', '/data/local/tmp/' + filename, host_path]):
            self.adb.run(['shell', 'rm', '/data/local/tmp/' + filename])
            return True
        log_warning('failed to pull %s from device' % device_path)
        return False


    def _pull_kernel_symbols(self):
        file = os.path.join(self.binary_cache_dir, 'kallsyms')
        if os.path.isfile(file):
            os.remove(file)
        if self.adb.switch_to_root():
            self.adb.run(['shell', '"echo 0>/proc/sys/kernel/kptr_restrict"'])
            self.adb.run(['pull', '/proc/kallsyms', file])


def load_config(config_file):
    """See annotate_source_file.config for explanation of configurations."""
    if not os.path.exists(config_file):
        log_fatal("can't find config_file: %s" % config_file)
    config = {}
    execfile(config_file, config)
    return config


def usage():
    print('python %s [options]' % __file__)
    print('Pull binaries needed by perf.data from device to binary_cache.')
    print('-h/--help print this help message')
    print('--config <config_file>  Set config file. Default is binary_cache_builder.config.')


if __name__ == '__main__':
    i = 1
    config_file = 'binary_cache_builder.config'
    while i < len(sys.argv):
        if sys.argv[i] == '-h' or sys.argv[i] == '--help':
            usage()
            sys.exit(0)
        elif sys.argv[i] == '--config':
            if i + 1 < len(sys.argv):
                config_file = sys.argv[i+1]
                i += 1
            else:
                print('argument for --config is missing')
                sys.exit(1)
        else:
            print('unknown option "%s"' % sys.argv[i])
            sys.exit(1)
        i += 1
    config = load_config(config_file)
    builder = BinaryCacheBuilder(config)
    builder.build_binary_cache()