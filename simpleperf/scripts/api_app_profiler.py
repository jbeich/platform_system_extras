#!/usr/bin/env python
#
# Copyright (C) 2019 The Android Open Source Project
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

"""
    This script is part of controlling simpleperf recording in user code. It is used to prepare
    profiling environment (upload simpleperf to device and enable profiling) before recording
    and collect profiling data on host after recording.
    Controlling simpleperf recording is done in below steps:
    1. Add simpleperf Java API/C++ API to the app's source code. And call the API in user code.
    2. Run `api_app_profiler.py prepare` to prepare profiling environment.
    3. Run the app one or more times to generate profiling data.
    4. Run `api_app_profiler.py collect` to collect profiling data on host.
"""

from __future__ import print_function
import argparse
import os
import os.path
import shutil

from utils import AdbHelper, get_target_binary_path, log_exit, remove

def prepare_recording(args):
    adb = AdbHelper()
    enable_profiling_on_device(adb, args)
    upload_simpleperf_to_device(adb)
    run_simpleperf_prepare_cmd(adb)

def enable_profiling_on_device(adb, args):
    android_version = adb.get_android_version()
    if android_version >= 10:
        adb.set_property('debug.perf_event_max_sample_rate', str(args.max_sample_rate[0]))
        adb.set_property('debug.perf_cpu_time_max_percent', str(args.max_cpu_percent[0]))
        adb.set_property('debug.perf_event_mlock_kb', str(args.max_memory_in_kb[0]))
    adb.set_property('security.perf_harden', '0')

def upload_simpleperf_to_device(adb):
    device_arch = adb.get_device_arch()
    simpleperf_binary = get_target_binary_path(device_arch, 'simpleperf')
    adb.check_run(['push', simpleperf_binary, '/data/local/tmp'])
    adb.check_run(['shell', 'chmod', 'a+x', '/data/local/tmp/simpleperf'])

def run_simpleperf_prepare_cmd(adb):
    adb.check_run(['shell', '/data/local/tmp/simpleperf', 'app-api-prepare'])


def collect_data(args):
    adb = AdbHelper()
    if not os.path.isdir(args.out_dir):
        os.makedirs(args.out_dir)
    move_profiling_data_to_tmp_dir(adb, args.app[0])
    adb.check_run(['pull', '/data/local/tmp/simpleperf_data', args.out_dir])
    adb.check_run(['shell', 'rm', '-rf', '/data/local/tmp/simpleperf_data'])
    simpleperf_data_path = os.path.join(args.out_dir, 'simpleperf_data')
    for name in os.listdir(simpleperf_data_path):
        remove(os.path.join(args.out_dir, name))
        shutil.move(os.path.join(simpleperf_data_path, name), args.out_dir)
        print('collect profiling data: %s' % os.path.join(args.out_dir, name))
    remove(simpleperf_data_path)

def move_profiling_data_to_tmp_dir(adb, app):
    """ move /data/data/<app>/simpleperf_data to /data/local/tmp/simpleperf_data."""
    upload_simpleperf_to_device(adb)
    adb.check_run(['shell', '/data/local/tmp/simpleperf', 'app-api-collect', '--app', app,
                   '--out-dir', '/data/local/tmp/simpleperf_data'])


class ArgumentHelpFormatter(argparse.ArgumentDefaultsHelpFormatter,
                            argparse.RawDescriptionHelpFormatter):
    pass

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=ArgumentHelpFormatter)
    subparsers = parser.add_subparsers()
    prepare_parser = subparsers.add_parser('prepare', help='Prepare recording on device.',
                                           formatter_class=ArgumentHelpFormatter)
    prepare_parser.add_argument('--max-sample-rate', nargs=1, type=int, default=[100000], help="""
                                Set max sample rate (only on Android >= Q).""")
    prepare_parser.add_argument('--max-cpu-percent', nargs=1, type=int, default=[25], help="""
                                Set max cpu percent for recording (only on Android >= Q).""")
    prepare_parser.add_argument('--max-memory-in-kb', nargs=1, type=int, default=[(1024 + 1) * 4 * 8], help="""
                                Set max kernel buffer size for recording (only on Android >= Q).
                                """)
    prepare_parser.set_defaults(func=prepare_recording)
    collect_parser = subparsers.add_parser('collect', help='Collect profiling data.',
                                           formatter_class=ArgumentHelpFormatter)
    collect_parser.add_argument('-p', '--app', nargs=1, required=True, help="""
                                The app package name of the app profiled.""")
    collect_parser.add_argument('-o', '--out-dir', default='simpleperf_data', help="""
                                The directory to store profiling data.""")
    collect_parser.set_defaults(func=collect_data)
    args = parser.parse_args()
    args.func(args)

if __name__ == '__main__':
    main()
