#!/usr/bin/env python3
#
# Copyright (C) 2017 The Android Open Source Project
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
"""Release test for simpleperf prebuilts.

It includes below tests:
1. Test profiling Android apps on different Android versions (starting from Android N).
2. Test simpleperf python scripts on different Hosts (linux, darwin and windows) on x86_64.
3. Test using both devices and emulators.
4. Test using both `adb root` and `adb unroot`.

"""

import argparse
from dataclasses import dataclass
import fnmatch
import inspect
import multiprocessing as mp
import os
from pathlib import Path
import re
from simpleperf_utils import extant_dir, log_exit, remove, ArgParseFormatter
import sys
import time
import types
from typing import List, Optional
import unittest

from . api_profiler_test import *
from . app_profiler_test import *
from . app_test import *
from . binary_cache_builder_test import *
from . cpp_app_test import *
from . debug_unwind_reporter_test import *
from . java_app_test import *
from . kotlin_app_test import *
from . pprof_proto_generator_test import *
from . report_html_test import *
from . report_lib_test import *
from . run_simpleperf_on_device_test import *
from . tools_test import *
from . test_utils import TestHelper


def get_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=ArgParseFormatter)
    parser.add_argument('--browser', action='store_true', help='pop report html file in browser.')
    parser.add_argument(
        '-d', '--device', nargs='+',
        help='set devices used to run tests. Each device in format name:serial-number')
    parser.add_argument('--list-tests', action='store_true', help='List all tests.')
    parser.add_argument('--ndk-path', type=extant_dir, help='Set the path of a ndk release')
    parser.add_argument('-p', '--pattern', nargs='+',
                        help='Run tests matching the selected pattern.')
    parser.add_argument('-r', '--repeat', type=int, default=1, help='times to repeat tests')
    parser.add_argument('--test-from', help='Run left tests from the selected test.')
    parser.add_argument('--test-dir', default='test_dir', help='Directory to store test results')
    return parser.parse_args()


def get_all_tests() -> List[str]:
    tests = []
    for name, value in globals().items():
        if isinstance(value, type) and issubclass(value, unittest.TestCase):
            for member_name, member in inspect.getmembers(value):
                if isinstance(member, (types.MethodType, types.FunctionType)):
                    if member_name.startswith('test'):
                        tests.append(name + '.' + member_name)
    return sorted(tests)


def get_filtered_tests(test_from: Optional[str], test_pattern: Optional[List[str]]) -> List[str]:
    tests = get_all_tests()
    if test_from:
        for i, test in enumerate(tests):
            if test == test_from:
                tests = tests[i:]
                break
        else:
            log_exit("Can't find test %s" % test_from)
    if test_pattern:
        patterns = [re.compile(fnmatch.translate(x)) for x in test_pattern]
        tests = [t for t in tests if any(pattern.match(t) for pattern in patterns)]
        if not tests:
            log_exit('No tests are matched.')
    return tests


def build_testdata(testdata_dir: Path):
    """ Collect testdata in testdata_dir.
        In system/extras/simpleperf/scripts, testdata comes from:
            <script_dir>/../testdata, <script_dir>/test/script_testdata, <script_dir>/../demo
        In prebuilts/simpleperf, testdata comes from:
            <script_dir>/test/testdata
    """
    testdata_dir.mkdir()

    script_test_dir = Path(__file__).resolve().parent
    script_dir = script_test_dir.parent

    source_dirs = [
        script_test_dir / 'script_testdata',
        script_test_dir / 'testdata',
        script_dir.parent / 'testdata',
        script_dir.parent / 'demo',
    ]

    for source_dir in source_dirs:
        if not source_dir.is_dir():
            continue
        for src_path in source_dir.iterdir():
            dest_path = testdata_dir / src_path.name
            if dest_path.exists():
                continue
            if src_path.is_file():
                shutil.copyfile(src_path, dest_path)
            elif src_path.is_dir():
                shutil.copytree(src_path, dest_path)


def run_tests(tests: List[str]) -> bool:
    argv = [sys.argv[0]] + tests
    test_runner = unittest.TextTestRunner(stream=TestHelper.log_fh, verbosity=0)
    test_program = unittest.main(argv=argv, testRunner=test_runner,
                                 exit=False, verbosity=0, module='test.do_test')
    return test_program.result.wasSuccessful()


def test_process_entry(tests: List[str], test_options: List[str], conn: mp.connection.Connection):
    parser = argparse.ArgumentParser()
    parser.add_argument('--browser', action='store_true')
    parser.add_argument('--device', help='android device serial number')
    parser.add_argument('--ndk-path', type=extant_dir)
    parser.add_argument('--testdata-dir', type=extant_dir)
    parser.add_argument('--test-dir', help='directory to store test results')
    args = parser.parse_args(test_options)

    TestHelper.init(args.test_dir, args.testdata_dir,
                    args.browser, args.ndk_path, args.device, conn)
    run_tests(tests)


@dataclass
class Device:
    name: str
    serial_number: str


@dataclass
class TestResult:
    try_time: int
    ok: bool


class TestProcess:
    """ Create a test process to run selected tests on a device. """

    TEST_MAX_TRY_TIME = 10
    TEST_TIMEOUT_IN_SEC = 10 * 60

    def __init__(
            self, tests: List[str],
            device: Optional[Device],
            repeat_index: int,
            test_options: List[str]):
        self.tests = tests
        self.device = device
        self.repeat_index = repeat_index
        self.test_options = test_options
        self.try_time = 1
        self.test_results: Dict[str, TestResult] = {}
        self.parent_conn: Optional[mp.connection.Connection] = None
        self.proc: Optional[mp.Process] = None
        self.last_update_time = 0.0
        self._start_test_process()

    def _start_test_process(self):
        unfinished_tests = [test for test in self.tests if test not in self.test_results]
        self.parent_conn, child_conn = mp.Pipe(duplex=False)
        test_options = self.test_options[:]
        test_options += ['--test-dir', str(self.test_dir)]
        if self.device:
            test_options += ['--device', self.device.serial_number]
        self.proc = mp.Process(target=test_process_entry, args=(
            unfinished_tests, test_options, child_conn))
        self.proc.start()
        self.last_update_time = time.time()

    @property
    def name(self) -> str:
        name = 'test'
        if self.device:
            name += '_' + self.device.name
        name += '_repeat_%d' % self.repeat_index
        return name

    @property
    def test_dir(self) -> Path:
        """ Directory to run the tests. """
        return Path.cwd() / (self.name + '_try_%d' % self.try_time)

    @property
    def alive(self) -> bool:
        """ Return if the test process is alive. """
        return self.proc.is_alive()

    @property
    def finished(self) -> bool:
        """ Return if all tests are finished. """
        return len(self.test_results) == len(self.tests)

    def check_update(self):
        """ Check if there is any test update. """
        try:
            while self.parent_conn.poll():
                msg = self.parent_conn.recv()
                self._process_msg(msg)
                self.last_update_time = time.time()
        except (EOFError, BrokenPipeError) as e:
            pass
        if time.time() - self.last_update_time > TestProcess.TEST_TIMEOUT_IN_SEC:
            self.proc.terminate()

    def _process_msg(self, msg: str):
        test_name, test_success = msg.split()
        test_success = test_success == 'OK'
        self.test_results[test_name] = TestResult(self.try_time, test_success)

    def join(self):
        self.proc.join()

    def restart(self) -> bool:
        """ Create a new test process to run unfinished tests. """
        if self.finished:
            return False
        if self.try_time == self.TEST_MAX_TRY_TIME:
            """ Exceed max try time. So mark left tests as failed. """
            for test in self.tests:
                if test not in self.test_results:
                    self.test_results[test] = TestResult(self.try_time, False)
            return False

        self.try_time += 1
        self._start_test_process()
        return True


class TestSummary:
    def __init__(self, test_count: int):
        self.summary_fh = open('test_summary.txt', 'w')
        self.failed_summary_fh = open('failed_test_summary.txt', 'w')
        self.results: Dict[Tuple[str, str], bool] = {}
        self.test_count = test_count

    @property
    def failed_test_count(self) -> int:
        return self.test_count - sum(1 for result in self.results.values() if result)

    def update(self, test_proc: TestProcess):
        for test, result in test_proc.test_results.items():
            key = (test, '%s_try_%s' % (test_proc.name, result.try_time))
            if key not in self.results:
                self.results[key] = result.ok
                self._write_result(key[0], key[1], result.ok)

    def _write_result(self, test_name: str, test_env: str, test_result: bool):
        print(
            '%s    %s    %s' % (test_name, test_env, 'OK' if test_result else 'FAILED'),
            file=self.summary_fh, flush=True)
        if not test_result:
            print('%s    %s    FAILED' % (test_name, test_env),
                  file=self.failed_summary_fh, flush=True)

    def end_tests(self):
        # Show sorted results after testing.
        self.summary_fh.seek(0, 0)
        self.failed_summary_fh.seek(0, 0)
        for key in sorted(self.results.keys()):
            self._write_result(key[0], key[1], self.results[key])
        self.summary_fh.close()
        self.failed_summary_fh.close()


class TestManager:
    """ Create test processes, monitor their status and log test progresses. """

    def __init__(self, args: argparse.Namespace):
        self.repeat_count = args.repeat
        self.test_options = self._build_test_options(args)
        self.devices = self._build_test_devices(args)
        self.test_summary: Optional[TestSummary] = None

    def _build_test_devices(self, args: argparse.Namespace) -> List[Device]:
        devices = []
        if args.device:
            for s in args.device:
                name, serial_number = s.split(':')
                devices.append(Device(name, serial_number))
        else:
            devices.append(Device('default', ''))
        return devices

    def _build_test_options(self, args: argparse.Namespace) -> List[str]:
        test_options: List[str] = []
        if args.browser:
            test_options.append('--browser')
        if args.ndk_path:
            test_options += ['--ndk-path', args.ndk_path]
        testdata_dir = Path('testdata').resolve()
        test_options += ['--testdata-dir', str(testdata_dir)]
        return test_options

    def run_all_tests(self, tests: List[str]):
        total_test_count = len(tests) * len(self.devices) * self.repeat_count
        self.test_summary = TestSummary(total_test_count)
        test_procs: List[TestProcess] = []
        for device in self.devices:
            test_procs.append(TestProcess(tests, device, 1, self.test_options))
        self.wait_for_test_results(test_procs, self.repeat_count)
        self.test_summary.end_tests()

    def wait_for_test_results(self, test_procs: List[TestProcess], repeat_count: int):
        test_count = sum(len(test_proc.tests) for test_proc in test_procs)
        while test_procs:
            dead_procs: List[TestProcess] = []
            # Check update.
            for test_proc in test_procs:
                if not test_proc.alive:
                    dead_procs.append(test_proc)
                test_proc.check_update()
                self.test_summary.update(test_proc)

            # Process dead procs.
            for test_proc in dead_procs:
                test_proc.join()
                if not test_proc.finished and test_proc.restart():
                    continue
                test_procs.remove(test_proc)
                if test_proc.repeat_index < repeat_count:
                    test_procs.append(
                        TestProcess(test_proc.tests, test_proc.device,
                                    test_proc.repeat_index + 1, test_proc.test_options))
            time.sleep(0.1)
        return True


def run_tests_in_child_process(tests: List[str], args: argparse.Namespace) -> bool:
    """ run tests in child processes, read test results through a pipe. """
    mp.set_start_method('spawn')  # to be consistent on darwin, linux, windows
    test_manager = TestManager(args)
    test_manager.run_all_tests(tests)

    total_test_count = test_manager.test_summary.test_count
    failed_test_count = test_manager.test_summary.failed_test_count
    if failed_test_count == 0:
        print('All tests passed!')
        return True
    print('%d of %d tests failed. See %s/failed_test_summary.txt for details.' %
          (failed_test_count, total_test_count, args.test_dir))
    return False


def main() -> bool:
    args = get_args()
    if args.list_tests:
        print('\n'.join(get_all_tests()))
        return True

    tests = get_filtered_tests(args.test_from, args.pattern)

    test_dir = Path(args.test_dir).resolve()
    remove(test_dir)
    test_dir.mkdir(parents=True)
    # Switch to the test dir.
    os.chdir(test_dir)
    build_testdata(Path('testdata'))
    return run_tests_in_child_process(tests, args)
