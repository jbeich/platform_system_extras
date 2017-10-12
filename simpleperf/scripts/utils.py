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

"""utils.py: export utility functions.
"""

from __future__ import print_function
import logging
import os
import os.path
import shutil
import struct
import subprocess
import sys
import time

def get_script_dir():
    return os.path.dirname(os.path.realpath(__file__))


def is_windows():
    return sys.platform == 'win32' or sys.platform == 'cygwin'

def is_darwin():
    return sys.platform == 'darwin'

def is_python3():
    return sys.version_info >= (3, 0)


def log_debug(msg):
    logging.debug(msg)


def log_info(msg):
    logging.info(msg)


def log_warning(msg):
    logging.warning(msg)


def log_fatal(msg):
    raise Exception(msg)

def log_exit(msg):
    sys.exit(msg)

def disable_debug_log():
    logging.getLogger().setLevel(logging.WARN)

def str_to_bytes(str):
    if not is_python3():
        return str
    # In python 3, str are wide strings whereas the C api expects 8 bit strings, hence we have to convert
    # For now using utf-8 as the encoding.
    return str.encode('utf-8')

def bytes_to_str(bytes):
    if not is_python3():
        return bytes
    return bytes.decode('utf-8')

def get_target_binary_path(arch, binary_name):
    if arch == 'aarch64':
        arch = 'arm64'
    arch_dir = os.path.join(get_script_dir(), "bin", "android", arch)
    if not os.path.isdir(arch_dir):
        log_fatal("can't find arch directory: %s" % arch_dir)
    binary_path = os.path.join(arch_dir, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


def get_host_binary_path(binary_name):
    dir = os.path.join(get_script_dir(), 'bin')
    if is_windows():
        if binary_name.endswith('.so'):
            binary_name = binary_name[0:-3] + '.dll'
        elif '.' not in binary_name:
            binary_name += '.exe'
        dir = os.path.join(dir, 'windows')
    elif sys.platform == 'darwin': # OSX
        if binary_name.endswith('.so'):
            binary_name = binary_name[0:-3] + '.dylib'
        dir = os.path.join(dir, 'darwin')
    else:
        dir = os.path.join(dir, 'linux')
    dir = os.path.join(dir, 'x86_64' if sys.maxsize > 2 ** 32 else 'x86')
    binary_path = os.path.join(dir, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


def is_executable_available(executable, option='--help'):
    """ Run an executable to see if it exists. """
    try:
        subproc = subprocess.Popen([executable, option], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        subproc.communicate()
        return subproc.returncode == 0
    except:
        return False

expected_tool_paths = {
    'adb': {
        'test_option': 'version',
        'darwin': [(True, 'Library/Android/sdk/platform-tools/adb'),
                   (False, '../../platform-tools/adb')],
        'linux': [(True, 'Android/Sdk/platform-tools/adb'),
                  (False, '../../platform-tools/adb')],
        'windows': [(True, 'AppData/Local/Android/sdk/platform-tools/adb'),
                    (False, '../../platform-tools/adb')],
    },
    'readelf': {
        'test_option': '--help',
        'darwin': [(True, 'Library/Android/sdk/ndk-bundle/toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-readelf'),
                   (False, '../toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-readelf')],
        'linux': [(True, 'Android/Sdk/ndk-bundle/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-readelf'),
                  (False, '../toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-readelf')],
        'windows': [(True, 'AppData/Local/Android/sdk/ndk-bundle/toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64/bin/aarch64-linux-android-readelf'),
                    (False, '../toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64/bin/aarch64-linux-android-readelf')],
    },
    'addr2line': {
        'test_option': '--help',
        'darwin': [(True, 'Library/Android/sdk/ndk-bundle/toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-addr2line'),
                   (False, '../toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-addr2line')],
        'linux': [(True, 'Android/Sdk/ndk-bundle/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-addr2line'),
                  (False, '../toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-addr2line')],
        'windows': [(True, 'AppData/Local/Android/sdk/ndk-bundle/toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64/bin/aarch64-linux-android-addr2line'),
                    (False, '../toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64/bin/aarch64-linux-android-addr2line')],
    },
    # TODO: add file paths in ndk.
    'aarch64-linux-android-objdump': {
        'test_option': '--help',

    },
    'arm-linux-androideabi-objdump': {
        'test_option': '--help',
    },
    'i686-linux-android-objdump': {
        'test_option': '--help',
    },
    'x86_64-linux-android-objdump': {
        'test_option': '--help',
    },
}

def find_tool_path(toolname):
    if toolname not in expected_tool_paths:
        return None
    test_option = expected_tool_paths[toolname]['test_option']
    if is_executable_available(toolname, test_option):
        return toolname
    platform = 'linux'
    if is_windows():
        platform = 'windows'
    elif is_darwin():
        platform = 'darwin'
    paths = expected_tool_paths[toolname][platform]
    home = os.environ.get('HOMEPATH') if is_windows() else os.environ.get('HOME')
    for (relative_to_home, path) in paths:
        path = path.replace('/', os.sep)
        if relative_to_home:
            path = os.path.join(home, path)
        else:
            path = os.path.join(get_script_dir(), path)
        if is_executable_available(path, test_option):
            return path
    return None


class AdbHelper(object):
    def __init__(self, enable_switch_to_root=True):
        adb_path = find_tool_path('adb')
        if not adb_path:
            log_exit("Can't find adb in PATH environment.")
        self.adb_path = adb_path
        self.enable_switch_to_root = enable_switch_to_root


    def run(self, adb_args):
        return self.run_and_return_output(adb_args)[0]


    def run_and_return_output(self, adb_args, stdout_file=None, log_output=True):
        adb_args = [self.adb_path] + adb_args
        log_debug('run adb cmd: %s' % adb_args)
        if stdout_file:
            with open(stdout_file, 'wb') as stdout_fh:
                returncode = subprocess.call(adb_args, stdout=stdout_fh)
            stdoutdata = ''
        else:
            subproc = subprocess.Popen(adb_args, stdout=subprocess.PIPE)
            (stdoutdata, _) = subproc.communicate()
            returncode = subproc.returncode
        result = (returncode == 0)
        if stdoutdata and adb_args[1] != 'push' and adb_args[1] != 'pull':
            stdoutdata = bytes_to_str(stdoutdata)
            if log_output:
                log_debug(stdoutdata)
        log_debug('run adb cmd: %s  [result %s]' % (adb_args, result))
        return (result, stdoutdata)

    def check_run(self, adb_args):
        self.check_run_and_return_output(adb_args)


    def check_run_and_return_output(self, adb_args, stdout_file=None, log_output=True):
        result, stdoutdata = self.run_and_return_output(adb_args, stdout_file, log_output)
        if not result:
            log_exit('run "adb %s" failed' % adb_args)
        return stdoutdata


    def _unroot(self):
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if not result:
            return
        if 'root' not in stdoutdata:
            return
        log_info('unroot adb')
        self.run(['unroot'])
        self.run(['wait-for-device'])
        time.sleep(1)


    def switch_to_root(self):
        if not self.enable_switch_to_root:
            self._unroot()
            return False
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if not result:
            return False
        if 'root' in stdoutdata:
            return True
        build_type = self.get_property('ro.build.type')
        if build_type == 'user':
            return False
        self.run(['root'])
        time.sleep(1)
        self.run(['wait-for-device'])
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        return result and 'root' in stdoutdata

    def get_property(self, name):
        result, stdoutdata = self.run_and_return_output(['shell', 'getprop', name])
        return stdoutdata if result else None

    def set_property(self, name, value):
        return self.run(['shell', 'setprop', name, value])


    def get_device_arch(self):
        output = self.check_run_and_return_output(['shell', 'uname', '-m'])
        if 'aarch64' in output:
            return 'arm64'
        if 'arm' in output:
            return 'arm'
        if 'x86_64' in output:
            return 'x86_64'
        if '86' in output:
            return 'x86'
        log_fatal('unsupported architecture: %s' % output.strip())


    def get_android_version(self):
        build_version = self.get_property('ro.build.version.release')
        android_version = 0
        if build_version:
            if not build_version[0].isdigit():
                c = build_version[0].upper()
                if c.isupper() and c >= 'L':
                    android_version = ord(c) - ord('L') + 5
            else:
                strs = build_version.split('.')
                if strs:
                    android_version = int(strs[0])
        return android_version


def flatten_arg_list(arg_list):
    res = []
    if arg_list:
        for items in arg_list:
            res += items
    return res


def remove(dir_or_file):
    if os.path.isfile(dir_or_file):
        os.remove(dir_or_file)
    elif os.path.isdir(dir_or_file):
        shutil.rmtree(dir_or_file, ignore_errors=True)


def open_report_in_browser(report_path):
    import webbrowser
    try:
        # Try to open the report with Chrome
        browser_key = ''
        for key, value in webbrowser._browsers.items():
            if 'chrome' in key:
                browser_key = key
        browser = webbrowser.get(browser_key)
        browser.open(report_path, new=0, autoraise=True)
    except:
        # webbrowser.get() doesn't work well on darwin/windows.
        webbrowser.open_new_tab(report_path)


class Elf(object):
    EM_386 = 3
    EM_X86_64 = 62
    EM_ARM = 40
    EM_AARCH64 = 183

    def __init__(self, path):
        self.path = path
        self.fh = None
        self.addr_size = None
        self.header = None
        self.machine = None
        self.section_headers = None
        self.section_names = None

    def close(self):
        if self.fh:
            self.fh.close()
            self.fh = None

    def read_header(self):
        try:
            self.fh = open(self.path, 'rb')
        except IOError:
            return False
        magic = self._read(16)
        if not magic or magic[0:4] != '\x7fELF':
            return False
        self.addr_size = 4 if ord(magic[4]) == 1 else 8
        if ord(magic[5]) != 1 or ord(magic[6]) != 1:
            return False
        data = self._read(2 + 2 + 4 + self.addr_size * 3 + 4 + 2 * 6)
        if not data:
            return False
        addr_c = 'I' if self.addr_size == 4 else 'Q'
        items = struct.unpack('HHI' + addr_c * 3 + 'I' + 'H' * 6, data)
        e_machine = items[1]
        e_shoff = items[5]
        e_shentsize = items[10]
        e_shnum = items[11]
        e_shstrndx = items[12]
        self.header = {
            'e_machine': e_machine,
            'e_shoff': e_shoff,
            'e_shentsize': e_shentsize,
            'e_shnum': e_shnum,
            'e_shstrndx': e_shstrndx,
        }
        return True

    def read_section_headers(self):
        self.section_headers = []
        self.fh.seek(self.header['e_shoff'])
        for i in range(self.header['e_shnum']):
            data = self._read(4 * 2 + self.addr_size * 4 + 4 * 2 + self.addr_size * 2)
            if not data:
                return False
            addr_c = 'I' if self.addr_size == 4 else 'Q'
            items = struct.unpack('II' + addr_c * 4 + 'II' + addr_c * 2, data)
            sh_name = items[0]
            sh_offset = items[4]
            sh_size = items[5]
            self.section_headers.append({
                'sh_name': sh_name,
                'sh_offset': sh_offset,
                'sh_size': sh_size,
            })
        self.section_names = {}
        if self.header['e_shstrndx']:
            section_header = self.section_headers[self.header['e_shstrndx']]
            self.fh.seek(section_header['sh_offset'])
            data = self._read(section_header['sh_size'])
            if not data:
                return False
            for i in range(len(self.section_headers)):
                sh_name = self.section_headers[i]['sh_name']
                end = data.index('\0', sh_name)
                name = data[sh_name : end]
                self.section_names[name] = i
        return True

    def _read(self, size):
        data = self.fh.read(size)
        if len(data) != size:
            return None
        return data

    def get_arch(self):
        if self.header['e_machine'] == self.EM_386:
            return 'x86'
        if self.header['e_machine'] == self.EM_X86_64:
            return 'x86_64'
        if self.header['e_machine'] == self.EM_ARM:
            return 'arm'
        if self.header['e_machine'] == self.EM_AARCH64:
            return 'arm64'
        return None


def find_dso_path_in_symfs_dir(dso_path, symfs_dir):
        """ Convert dso_path in perf.data to file path in symfs_dir. """
        if dso_path[0] != '/' or dso_path == '//anon':
            return None
        if symfs_dir:
            tmp_path = os.path.join(symfs_dir, dso_path[1:])
            if os.path.isfile(tmp_path):
                return tmp_path
        if os.path.isfile(dso_path):
            return dso_path
        return None

class LookAheadAddr2Line(object):
    """ Convert (dso_path, func_vaddr, vaddr) to (source_file, line) pairs.
        dso_path: the path of elf file containing the vaddr.
        func_vaddr: the start address of symbol containing vaddr.
        vaddr: the vaddr in dso_path for which we want to find line info.
        Each (dso_path, func_vaddr, vaddr) tuple can match a list of (source_file, line) info,
        with each (source_file, line) except the last inside an inlined function.

        The steps to convert addrs to lines are as below:
        1. Collects all (dso_path, func_vaddr, vaddr) requests before converting. So it can convert
           all vaddrs in the same dso_path at one time.
        2. Convert vaddrs to (source_file, line) pairs for each dso_path as below:
          2.1 Check if the dso_path has .debug_line. If not, omit the conversion for this dso_path.
          2.2 Get arch of the dso_path, and decide the addr_step for it.
          2.2 Create a subprocess running addr2line, pass it all exact vaddrs.
              Collect (source_file, line) pairs for each vaddr.
          2.3 For vaddrs don't have valid line info (having ? in the first line of
              (source_file, line) pairs), they hit instructions without matching statements
              (Possibly instructions generated for stack check, switch statement optimization,
              etc.). For each vaddr in these vaddrs, get line info for
              range(vaddr - addr_step, max(vaddr - 4 * addr_step, func_vaddr) - 1, -addr_step).
              If any addr in the range has valid line info, assign the line info to vaddr.
          2.4 For vaddrs still can't find valid line info (Possibly because the auto generated
              instruction list is too long), get line info for
              range(vaddr - 5 * addr_step, max(vaddr - 128 * addr_step, func_vaddr) - 1, -addr_step).
              If any addr in the range has valid line info, assign the line info to vaddr.

        The value of addr_step is as below:
          x86: 1
          x86_64: 1
          arm: 2
          arm64: 4
    """

    class Dso(object):
        def __init__(self):
            self.real_path = None
            self.addr_step = None
            self.addrs = {}

    class Addr(object):
        def __init__(self, func_addr, addr):
            self.func_addr = func_addr
            self.addr = addr
            self.source_lines = None

    def __init__(self, addr2line_path, symfs_dir=None):
        self.dso_dict = {}
        if addr2line_path and is_executable_available(addr2line_path):
            self.addr2line_path = addr2line_path
        else:
            self.addr2line_path = find_tool_path('addr2line')
            if not self.addr2line_path:
                log_exit("Can't find addr2line.")
        self.symfs_dir = symfs_dir
        self.file_name_to_id = {}
        self.file_id_to_name = []

    def add_addr(self, dso_path, func_vaddr, vaddr):
        dso = self.dso_dict.get(dso_path)
        if dso is None:
            dso = self.dso_dict[dso_path] = self.Dso()
        if vaddr not in dso.addrs:
            dso.addrs[vaddr] = self.Addr(func_vaddr, vaddr)

    def convert_addrs_to_lines(self):
        for dso_path in self.dso_dict:
            self._convert_addrs_in_one_dso(dso_path, self.dso_dict[dso_path])

    def get_dso(self, dso_path):
        return self.dso_dict.get(dso_path)

    def get_sources(self, dso, addr):
        return dso.addrs[addr].source_lines

    def get_file_path(self, file_id):
        return self.file_id_to_name[file_id]

    def _convert_addrs_in_one_dso(self, dso_path, dso):
        log_info('convert addrs to lines for path: %s' % dso_path)
        # step 1: check dso_path.
        if not self._check_debug_line_section(dso_path, dso):
            return
        log_info('find real path %s' % dso.real_path)
        # step 2: collect line info for exact addrs.
        self._collect_line_info(dso, [0])

        # step 3: collect line info for addrs >= (addr - addr_step * 4).
        step = dso.addr_step
        add_addrs = range(-step, -step * 4 - 1, -step)
        self._collect_line_info(dso, add_addrs)

        # step 4: collect line info for addrs >= (addr - addr_step * 128).
        add_addrs = range(-step * 5, -step * 128 - 1, -step)
        self._collect_line_info(dso, add_addrs)

    def _check_debug_line_section(self, dso_path, dso):
        real_path = find_dso_path_in_symfs_dir(dso_path, self.symfs_dir)
        if real_path is None:
            if dso_path not in ['//anon', 'unknown', '[kernel.kallsyms]']:
                log_debug("Can't find dso %s" % dso_path)
            return False
        elf = Elf(real_path)
        success = False
        if not elf.read_header() or not elf.read_section_headers():
            log_debug("file %s isn't an elf file" % real_path)
        elif '.debug_line' not in elf.section_names:
            log_debug("file %s doesn't contain .debug_line section." % real_path)
        else:
            dso.real_path = real_path
            dso.addr_step = self._get_addr_step(elf.get_arch())
            success = True
        elf.close()
        return success

    def _get_addr_step(self, arch):
        if arch == 'arm64':
            return 4
        if arch == 'arm':
            return 2
        return 1

    def _collect_line_info(self, dso, add_addrs):
        # Collect request addrs.
        addr_dict = {}
        for addr_obj in dso.addrs.values():
            if addr_obj.source_lines:
                continue
            for add_addr in add_addrs:
                addr = max(addr_obj.addr + add_addr, addr_obj.func_addr)
                addr_dict[addr] = True
                if addr == addr_obj.func_addr:
                    break
        addr_str = []
        for addr in sorted(addr_dict.keys()):
            addr_str.append('%x' % addr)
        addr_str = '\n'.join(addr_str)

        # Use addr2line to collect line info.
        subproc = subprocess.Popen([self.addr2line_path, '-e', dso.real_path, '-aiC'],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        (stdoutdata, _) = subproc.communicate(str_to_bytes(addr_str))
        stdoutdata = bytes_to_str(stdoutdata)
        #log_info('addr_str = %s' % addr_str)
        #log_info('stdout = %s' % stdoutdata)
        cur_line_list = None
        for line in stdoutdata.strip().split('\n'):
            #log_info('line = %s' % line)
            if line[:2] == '0x':
                # a new address
                cur_line_list = addr_dict[int(line, 16)] = []
            else:
                if cur_line_list is None:
                    continue
                if '?' in line:
                    # If the first line of an addr isn't clear, omit the following lines.
                    if not cur_line_list:
                        cur_line_list = None
                    continue
                # Handle lines like "C:\Users\...\file:32".
                items = line.rsplit(':', 1)
                if len(items) != 2:
                    continue
                (file_path, line_number) = items
                line_number = line_number.split()[0]  # Remove comments after line number
                #log_info('items = %s' % items)
                try:
                    line_number = int(line_number)
                except ValueError:
                    continue
                file_id = self._get_file_id(file_path)
                cur_line_list.append((file_id, line_number))

        # Fill line info in dso.addrs.
        for addr_obj in dso.addrs.values():
            if addr_obj.source_lines:
                continue
            for add_addr in add_addrs:
                addr = max(addr_obj.addr + add_addr, addr_obj.func_addr)
                lines = addr_dict.get(addr)
                if not lines:
                    continue
                addr_obj.source_lines = lines
                #log_info('set line info for addr 0x%x: %s' % (addr_obj.addr, lines))
                break

    def _get_file_id(self, file_path):
        file_id = self.file_name_to_id.get(file_path)
        if file_id is None:
            file_id = self.file_name_to_id[file_path] = len(self.file_id_to_name)
            self.file_id_to_name.append(file_path)
        return file_id


class Objdump(object):
    """ Disassemble functions. """
    def __init__(self, symfs_dir=None):
        self.symfs_dir = symfs_dir

    def disassemble_function(self, dso_path, start_addr, len):
        log_info('disassemble_function for %s, %x, %x' % (dso_path, start_addr, len))
        # step 1: find real dso path and objdump.
        real_path = find_dso_path_in_symfs_dir(dso_path, self.symfs_dir)
        if real_path is None:
            log_info('no file')
            return None
        elf = Elf(real_path)
        if not elf.read_header():
            log_info('read header file')
            return None
        objdump = self._get_objdump_name(elf.get_arch())
        if not objdump:
            log_info('objdump name fail')
            return None
        log_info('need to find %s' % objdump)
        objdump_path = find_tool_path(objdump)
        if not objdump_path:
            log_info("can't find %s" % objdump)
            return None

        # step 2: run objdump to get disassembled code.
        args = [objdump_path, '-dlC', '--no-show-raw-insn',
                '--start-address=0x%x' % start_addr,
                '--stop-address=0x%x' % (start_addr + len), real_path]
        log_info('%s' % args)
        subproc = subprocess.Popen(args, stdout=subprocess.PIPE, stdin=subprocess.PIPE)
        (stdoutdata, _) = subproc.communicate()
        if not stdoutdata:
            return None
        result = []
        for line in stdoutdata.split('\n'):
            items = line.split(':', 1)
            try:
                addr = int(items[0], 16)
            except ValueError:
                addr = 0
            result.append((line, addr))
        return result


    def _get_objdump_name(self, arch):
        log_info('arch = %s' % arch)
        if arch == 'arm64':
            return 'aarch64-linux-android-objdump'
        if arch == 'arm':
            return 'arm-linux-androideabi-objdump'
        if arch == 'x86':
            return 'i686-linux-android-objdump'
        if arch == 'x86_64':
            return 'x86_64-linux-android-objdump'
        return None



logging.getLogger().setLevel(logging.DEBUG)
