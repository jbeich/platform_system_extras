#!/usr/bin/python
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

import argparse
import datetime
import json
import os
import subprocess
import sys
import tempfile

from annotate import Addr2Line
from simpleperf_report_lib import ReportLib
from utils import *


class HtmlWriter(object):

    def __init__(self, output_path):
        self.fh = open(output_path, 'w')
        self.tag_stack = []

    def close(self):
        self.fh.close()

    def open_tag(self, tag, **attrs):
        attr_str = ''
        for key in attrs.keys():
            attr_str += ' %s="%s"' % (key, attrs[key])
        self.fh.write('<%s%s>' % (tag, attr_str))
        self.tag_stack.append(tag)
        return self

    def close_tag(self, tag=None):
        if tag:
            assert tag == self.tag_stack[-1]
        self.fh.write('</%s>\n' % self.tag_stack.pop())

    def add(self, text):
        self.fh.write(text)
        return self

    def add_file(self, file_path):
        file_path = os.path.join(get_script_dir(), file_path)
        with open(file_path, 'r') as f:
            self.add(f.read())
        return self


class EventScope(object):

    def __init__(self, name):
        self.name = name
        self.processes = {}  # map from pid to ProcessScope
        self.sample_count = 0
        self.event_count = 0

    def get_process(self, pid):
        process = self.processes.get(pid)
        if not process:
            process = self.processes[pid] = ProcessScope(pid)
        return process

    def get_sample_info(self, add_addr_hit_map):
        result = {}
        result['eventName'] = self.name
        result['eventCount'] = self.event_count
        result['processes'] = [process.get_sample_info(add_addr_hit_map) for process in self.processes.values()]
        return result


class ProcessScope(object):

    def __init__(self, pid):
        self.pid = pid
        self.name = ''
        self.event_count = 0
        self.threads = {}  # map from tid to ThreadScope

    def get_thread(self, tid, thread_name):
        thread = self.threads.get(tid)
        if not thread:
            thread = self.threads[tid] = ThreadScope(tid)
        thread.name = thread_name
        if self.pid == tid:
            self.name = thread_name
        return thread

    def get_sample_info(self, add_addr_hit_map):
        result = {}
        result['pid'] = self.pid
        result['eventCount'] = self.event_count
        result['threads'] = [thread.get_sample_info(add_addr_hit_map) for thread in self.threads.values()]
        return result


class ThreadScope(object):

    def __init__(self, tid):
        self.tid = tid
        self.name = ''
        self.event_count = 0
        self.libs = {}  # map from libId to LibScope

    def add_callstack(self, event_count, callstack, build_addr_hit_map):
        """ callstack is a list of tuple (lib_id, func_id, addr).
            For each i > 0, callstack[i] calls callstack[i-1]."""
        # When a callstack contains recursive function, we should only add event count
        # and callchain for each recursive function once.
        hit_func_ids = {}
        for i in range(len(callstack)):
            lib_id, func_id, addr = callstack[i]
            lib = self.libs.get(lib_id)
            if not lib:
                lib = self.libs[lib_id] = LibScope(lib_id)
            if func_id in hit_func_ids:
                continue
            hit_func_ids[func_id] = True
            function = lib.get_function(func_id)
            if i == 0:
                lib.event_count += event_count
                function.sample_count += 1
            function.add_reverse_callchain(callstack, i + 1, len(callstack), event_count)

            if build_addr_hit_map:
                if function.addr_hit_map is None:
                    function.addr_hit_map = {}
                count_info = function.addr_hit_map.get(addr)
                if count_info is None:
                    count_info = function.addr_hit_map[addr] = [0, 0]
                if i == 0:
                    count_info[0] += event_count
                count_info[1] += event_count
                

        hit_func_ids = {}
        for i in range(len(callstack) - 1, -1, -1):
            lib_id, func_id, _ = callstack[i]
            if func_id in hit_func_ids:
                continue
            hit_func_ids[func_id] = True
            lib = self.libs.get(lib_id)
            lib.get_function(func_id).add_callchain(callstack, i - 1, -1, event_count)

    def get_sample_info(self, add_addr_hit_map):
        result = {}
        result['tid'] = self.tid
        result['eventCount'] = self.event_count
        result['libs'] = [lib.gen_sample_info(add_addr_hit_map) for lib in self.libs.values()]
        return result


class LibScope(object):

    def __init__(self, lib_id):
        self.lib_id = lib_id
        self.event_count = 0
        self.functions = {}  # map from func_id to FunctionScope.

    def get_function(self, func_id):
        function = self.functions.get(func_id)
        if not function:
            function = self.functions[func_id] = FunctionScope(func_id)
        return function

    def gen_sample_info(self, add_addr_hit_map):
        result = {}
        result['libId'] = self.lib_id
        result['eventCount'] = self.event_count
        result['functions'] = [func.gen_sample_info(add_addr_hit_map) for func in self.functions.values()]
        return result


class FunctionScope(object):

    def __init__(self, func_id):
        self.sample_count = 0
        self.call_graph = CallNode(func_id)
        self.reverse_call_graph = CallNode(func_id)
        # map from addr to [event_count, subtree_event_count].
        self.addr_hit_map = None
        # map from (source_file_id, line) to [event_count, subtree_event_count].
        self.source_code_hit_map = None

    def add_callchain(self, callchain, start, end, event_count):
        node = self.call_graph
        for i in range(start, end, -1):
            node = node.get_child(callchain[i][1])
        node.event_count += event_count

    def add_reverse_callchain(self, callchain, start, end, event_count):
        node = self.reverse_call_graph
        for i in range(start, end):
            node = node.get_child(callchain[i][1])
        node.event_count += event_count

    def update_subtree_event_count(self):
        a = self.call_graph.update_subtree_event_count()
        b = self.reverse_call_graph.update_subtree_event_count()
        return max(a, b)

    def limit_callchain_percent(self, min_callchain_percent, hit_func_ids):
        min_limit = min_callchain_percent * 0.01 * self.call_graph.subtree_event_count
        self.call_graph.cut_edge(min_limit, hit_func_ids)
        self.reverse_call_graph.cut_edge(min_limit, hit_func_ids)

    def gen_sample_info(self, add_addr_hit_map):
        result = {}
        result['c'] = self.sample_count
        result['g'] = self.call_graph.gen_sample_info()
        result['rg'] = self.reverse_call_graph.gen_sample_info()
        if self.source_code_hit_map:
            source_code_list = []
            for key in self.source_code_hit_map:
                source_code = {}
                source_code['f'] = key[0]
                source_code['l'] = key[1]
                count = self.source_code_hit_map[key]
                source_code['e'] = count[0]
                source_code['s'] = count[1]
                source_code_list.append(source_code)
            result['sc'] = source_code_list
        if add_addr_hit_map and self.addr_hit_map:
            addr_map = {}
            for addr in self.addr_hit_map:
                count = self.addr_hit_map[addr]
                addr_map[addr] = {'e': count[0], 's': count[1]}
            result['ad'] = addr_map
        return result


class CallNode(object):

    def __init__(self, func_id):
        self.event_count = 0
        self.subtree_event_count = 0
        self.func_id = func_id
        self.children = {}  # map from func_id to CallNode

    def get_child(self, func_id):
        child = self.children.get(func_id)
        if not child:
            child = self.children[func_id] = CallNode(func_id)
        return child

    def update_subtree_event_count(self):
        self.subtree_event_count = self.event_count
        for child in self.children.values():
            self.subtree_event_count += child.update_subtree_event_count()
        return self.subtree_event_count

    def cut_edge(self, min_limit, hit_func_ids):
        hit_func_ids.add(self.func_id)
        to_del_children = []
        for key in self.children:
            child = self.children[key]
            if child.subtree_event_count < min_limit:
                to_del_children.append(key)
            else:
                child.cut_edge(min_limit, hit_func_ids)
        for key in to_del_children:
            del self.children[key]

    def gen_sample_info(self):
        result = {}
        result['e'] = self.event_count
        result['s'] = self.subtree_event_count
        result['f'] = self.func_id
        result['c'] = [child.gen_sample_info() for child in self.children.values()]
        return result


class LibSet(object):

    def __init__(self):
        self.lib_name_to_id = {}
        self.lib_id_to_name = []

    def get_lib_id(self, lib_name):
        lib_id = self.lib_name_to_id.get(lib_name)
        if lib_id is None:
            lib_id = len(self.lib_name_to_id)
            self.lib_name_to_id[lib_name] = lib_id
            self.lib_id_to_name.append(lib_name)
        return lib_id

    def get_lib_name(self, lib_id):
        return self.lib_id_to_name[lib_id]


class Function(object):
    def __init__(self, lib_id, func_name, func_id):
        self.lib_id = lib_id
        self.func_name = func_name
        self.func_id = func_id
        self.start_addr = None
        self.addr_len = None
        self.source_file_id = None
        self.line_range = None
        self.disassemble_code = None


class FunctionSet(object):
    """ Collect information for each function. """

    def __init__(self):
        # Create two map for Functions, funcs_by_name uses key (lib_id, function_name),
        # funcs_by_id uses key func_id.
        self.funcs_by_name = {}
        self.funcs_by_id = {}

    def get_func_id(self, lib_id, symbol):
        key = (lib_id, symbol.symbol_name)
        function = self.funcs_by_name.get(key)
        if function is None:
            function = Function(lib_id, symbol.symbol_name, len(self.funcs_by_name))
            function.start_addr = symbol.symbol_addr
            function.addr_len = symbol.addr_len
            self.funcs_by_name[key] = function
            self.funcs_by_id[function.func_id] = function
        return function.func_id


class LineRange(object):
    def __init__(self, start_line, end_line):
        self.start_line = start_line
        self.end_line = end_line  # inclusive


class SourceFile(object):
    """ Source code in a source code file. """
    def __init__(self, file_id, file_path):
        self.file_id = file_id
        self.file_path = file_path
        self.line_ranges = []
        self.code = {}  # map from line to code for that line.

    def add_line_range(self, start_line, end_line):
        self.line_ranges.append(LineRange(start_line, end_line))

    def sort_and_merge_line_ranges(self):
        ranges = sorted(self.line_ranges, key=lambda x: x.start_line)
        i = 0
        for j in range(1, len(ranges)):
            if ranges[j].start_line <= ranges[i].end_line + 1:
                ranges[i].end_line = max(ranges[i].end_line, ranges[j].end_line)
            else:
                i += 1
                ranges[i] = ranges[j]
        self.line_ranges = ranges[:i+1]


class SourceFileSet(object):
    """ a set of SourceFile. """
    def __init__(self):
        self.source_files = {}  # map from file_path to SourceFile

    def get_source_file(self, file_path):
        source_file = self.source_files.get(file_path)
        if not source_file:
            source_file = SourceFile(len(self.source_files), file_path)
            self.source_files[file_path] = source_file
        return source_file

    def sort_and_merge_line_ranges(self):
        for source_file in self.source_files.values():
            source_file.sort_and_merge_line_ranges()


class SourceFileLoader(object):
    """ Load source code for SourceFile.
        The file_path provided by SourceFile can be an absolute path, a relative path,
        or just a filename. To work with all situations, the way to find source file
        in the file system is as below:
        1. Collect all source file paths under provided source_dirs. The suffix of a source file
          should contain one of below:
          h : for c/c++ header files.
          c : for c/c++ source files.
          java : for java source files.
          kt : for kotlin source files.
        2. Given a file_path from SourceFile, find the best source file path, depending
          on the length of common characters from the end to the beginning.
        3. Read sources code from source file path, fill it in SourceFile.source_code.
    """
    def __init__(self, source_dirs):
        if source_dirs is None:
            source_dirs = []
        self.source_dirs = source_dirs
        self.file_name_to_paths = {}  # map from file_name to a list of file_paths.
        self._collect_source_paths()

    def _collect_source_paths(self):
        for source_dir in self.source_dirs:
            for root, _, files in os.walk(source_dir):
                for file_name in files:
                    if self._is_source_file_path(file_name):
                        file_paths = self.file_name_to_paths.get(file_name)
                        if file_paths is None:
                            file_paths = self.file_name_to_paths[file_name] = []
                        file_paths.append(os.path.join(root, file_name))
        #for file_name in self.file_name_to_paths:
        #    print('file_name %s' % file_name)
        #    file_paths = self.file_name_to_paths[file_name]
        #    for file_path in file_paths:
        #        print('file: %s' % file_path)

    def _is_source_file_path(self, path):
        dot_index = path.rfind('.')
        if dot_index == -1:
            return False
        suffix = path[dot_index + 1:]
        if suffix.find('h') != -1 or suffix.find('c') != -1 or suffix.find('java') != -1 or \
            suffix.find('kt') != -1:
            return True
        return False

    def load_code_for_source_file(self, source_file):
        log_info('look for source file %s' % source_file.file_path)
        file_path = self._find_best_path(source_file.file_path)
        if not file_path:
            return
        log_info('found file path %s' % file_path)
        source_code = self._read_source_code(file_path)
        max_line_number = len(source_code)
        code = {}
        for i in range(len(source_file.line_ranges)):
            line_range = source_file.line_ranges[i]
            line_range.start_line = max(1, line_range.start_line)
            line_range.end_line = min(max_line_number, line_range.end_line)
            if line_range.start_line > line_range.end_line:
                continue
            for i in range(line_range.start_line, line_range.end_line + 1):
                code[i] = source_code[i-1]
        source_file.code = code

    def _find_best_path(self, file_path):
        if os.sep != '/':
            file_path = file_path.replace('/', os.sep)
        file_name = file_path[file_path.rfind(os.sep) + 1:]
        candidate_paths = self.file_name_to_paths.get(file_name)
        if candidate_paths is None:
            return None
        best_path = None
        best_match_length = 0
        for path in candidate_paths:
            match_length = len(os.path.commonprefix((path[::-1], file_path[::-1])))
            if match_length > best_match_length:
                best_match_length = match_length
                best_path = path
        return best_path

    def _read_source_code(self, file_path):
        with open(file_path, 'r') as f:
            return f.readlines()



class RecordData(object):

    """RecordData reads perf.data, and generates data used by report.js in json format.
       Record Info contains below items:
            1. recordTime: string
            2. machineType: string
            3. androidVersion: string
            4. recordCmdline: string
            5. totalSamples: int
            6. processNames: map from pid to processName.
            7. threadNames: map from tid to threadName.
            8. libList: an array of libNames, indexed by libId.
            9. functionMap: map from functionId to funcData.
                funcData = {
                    l: libId
                    f: functionName
                    s: source_file_id [optional]
                    r: [start_line, end_line] [optional]
                    d: [(disassemble_code_line, addr)] [optional]
                }

            10.  sampleInfo = [eventInfo]
                eventInfo = {
                    eventName
                    eventCount
                    processes: [processInfo]
                }
                processInfo = {
                    pid
                    eventCount
                    threads: [threadInfo]
                }
                threadInfo = {
                    tid
                    eventCount
                    libs: [libInfo],
                }
                libInfo = {
                    libId,
                    eventCount,
                    functions: [funcInfo]
                }
                funcInfo = {
                    c: sampleCount
                    g: callGraph
                    rg: reverseCallgraph
                    sc: [sourceCodeInfo] [optional]
                    ad: addrHitMap [optional]
                }
                callGraph and reverseCallGraph are both of type CallNode.
                callGraph shows how a function calls other functions.
                reverseCallGraph shows how a function is called by other functions.
                CallNode {
                    e: selfEventCount
                    s: subTreeEventCount
                    f: functionId
                    c: [CallNode] # children
                }

                sourceCodeInfo {
                    f: sourceFileId
                    l: line
                    e: eventCount
                    s: subtreeEventCount
                }

                addrHitMap  # map from addr to addrInfo
                addrInfo {
                    e: eventCount
                    s: subtreeEventCount
                }

            11: sourceFiles: an array of sourceFile, indexed by sourceFileId.
                sourceFile {
                    path
                    code:  # a map from line to code for that line.
                }
    """

    def __init__(self, record_file, binary_cache_path):
        self.record_file = record_file
        self.binary_cache_path = binary_cache_path
        self.meta_info = None
        self.cmdline = None
        self.arch = None
        self.events = {}
        self.libs = LibSet()
        self.functions = FunctionSet()
        self.total_samples = 0
        self.source_files = SourceFileSet()
        self.need_disassemble_code = False

    def load_record_file(self, need_addr_hit_map):
        lib = ReportLib()
        lib.SetRecordFile(self.record_file)
        if self.binary_cache_path:
            lib.SetSymfs(self.binary_cache_path)
        self.meta_info = lib.MetaInfo()
        self.cmdline = lib.GetRecordCmd()
        self.arch = lib.GetArch()
        while True:
            raw_sample = lib.GetNextSample()
            if not raw_sample:
                lib.Close()
                break
            raw_event = lib.GetEventOfCurrentSample()
            symbol = lib.GetSymbolOfCurrentSample()
            callchain = lib.GetCallChainOfCurrentSample()
            event = self._get_event(raw_event.name)
            self.total_samples += 1
            event.sample_count += 1
            event.event_count += raw_sample.period
            process = event.get_process(raw_sample.pid)
            process.event_count += raw_sample.period
            thread = process.get_thread(raw_sample.tid, raw_sample.thread_comm)
            thread.event_count += raw_sample.period

            lib_id = self.libs.get_lib_id(symbol.dso_name)
            func_id = self.functions.get_func_id(lib_id, symbol)
            callstack = [(lib_id, func_id, symbol.vaddr_in_file)]
            for i in range(callchain.nr):
                symbol = callchain.entries[i].symbol
                lib_id = self.libs.get_lib_id(symbol.dso_name)
                func_id = self.functions.get_func_id(lib_id, symbol)
                callstack.append((lib_id, func_id, symbol.vaddr_in_file))
            thread.add_callstack(raw_sample.period, callstack, need_addr_hit_map)

        for event in self.events.values():
            for process in event.processes.values():
                for thread in process.threads.values():
                    for lib in thread.libs.values():
                        for funcId in lib.functions.keys():
                            function = lib.functions[funcId]
                            function.update_subtree_event_count()

    def limit_percents(self, min_func_percent, min_callchain_percent):
        hit_func_ids = set()
        for event in self.events.values():
            min_limit = event.event_count * min_func_percent * 0.01
            for process in event.processes.values():
                for thread in process.threads.values():
                    for lib in thread.libs.values():
                        for func_id in lib.functions.keys():
                            function = lib.functions[func_id]
                            if function.call_graph.subtree_event_count < min_limit:
                                del lib.functions[func_id]
                            else:
                                function.limit_callchain_percent(min_callchain_percent, hit_func_ids)
        
        for function in self.functions.funcs_by_name.values():
            if function.func_id not in hit_func_ids:
                del self.functions.funcs_by_id[function.func_id]
        self.functions.funcs_by_name = None

    def _get_event(self, event_name):
        if event_name not in self.events:
            self.events[event_name] = EventScope(event_name)
        return self.events[event_name]

    def add_source_code(self, source_dirs, addr2line_path):
        """ Collect source code information:
            step 1: Build source line range for each function in FunctionSet.
            step 2: Build source lines for each addr in FunctionScope.addr_hit_map.
            step 3: Collect needed source code in SourceFileSet.
        """
        addr2line = LookAheadAddr2Line(addr2line_path, self.binary_cache_path)
        # Request source line range for each function in Func.
        for function in self.functions.funcs_by_id.values():
            lib_name = self.libs.get_lib_name(function.lib_id)
            addr2line.add_addr(lib_name, function.start_addr, function.start_addr)
            addr2line.add_addr(lib_name, function.start_addr, function.start_addr + function.addr_len - 1)
        # Request source lines for each addr in FunctionScope.addr_hit_map.
        for event in self.events.values():
            for process in event.processes.values():
                for thread in process.threads.values():
                    for lib in thread.libs.values():
                        lib_name = self.libs.get_lib_name(lib.lib_id)
                        for function in lib.functions.values():
                            func_addr = self.functions.funcs_by_id[function.call_graph.func_id].start_addr
                            for addr in function.addr_hit_map:
                                addr2line.add_addr(lib_name, func_addr, addr)
        addr2line.convert_addrs_to_lines()

        # Set source line range for each function in FunctionSet.
        for function in self.functions.funcs_by_id.values():
            lib_name = self.libs.get_lib_name(function.lib_id)
            dso = addr2line.get_dso(lib_name)
            start_sources = addr2line.get_sources(dso, function.start_addr)
            end_sources = addr2line.get_sources(dso, function.start_addr + function.addr_len - 1)
            if not start_sources or not end_sources:
                continue
            # sources[0:-1] are for inlined functions, don't use them.
            start_file_id, start_line = start_sources[-1]
            end_file_id, end_line = end_sources[-1]
            if start_file_id != end_file_id:
                # A function is expected to only exist in one source file.
                continue
            if start_line > end_line:
                continue
            function.line_range = LineRange(start_line, end_line)
            source_file = self.source_files.get_source_file(addr2line.get_file_path(start_file_id))
            source_file.add_line_range(start_line, end_line)
            function.source_file_id = source_file.file_id
        
        # Build FunctionScope.source_code_hit_map.
        for event in self.events.values():
            for process in event.processes.values():
                for thread in process.threads.values():
                    for lib in thread.libs.values():
                        lib_name = self.libs.get_lib_name(lib.lib_id)
                        dso = addr2line.get_dso(lib_name)
                        for function in lib.functions.values():
                            for addr in function.addr_hit_map:
                                addr_count = function.addr_hit_map[addr]
                                sources = addr2line.get_sources(dso, addr)
                                if not sources:
                                    continue
                                if function.source_code_hit_map is None:
                                    function.source_code_hit_map = {}
                                for file_id, line in sources:
                                    file_path = addr2line.get_file_path(file_id)
                                    source_file = self.source_files.get_source_file(file_path)
                                    source_file.add_line_range(line - 5, line + 5)
                                    key = (source_file.file_id, line)
                                    count_info = function.source_code_hit_map.get(key)
                                    if count_info is None:
                                        count_info = function.source_code_hit_map[key] = [0, 0]
                                    count_info[0] += addr_count[0]
                                    count_info[1] += addr_count[1]

        # Collect needed source code in SourceFileSet.
        self.source_files.sort_and_merge_line_ranges()
        source_file_loader = SourceFileLoader(source_dirs)
        for source_file in self.source_files.source_files.values():
            source_file_loader.load_code_for_source_file(source_file)


    def add_disassemble_code(self):
        """ Collect disassemble code information:
            step 1: Collect disassemble code in each FuncInfo.
            step 2: Read each sample to build disassemble_code_hit_map in each FunctionScope.
        """
        self.need_disassemble_code = True
        objdump = Objdump(self.binary_cache_path)
        for function in self.functions.funcs_by_id.values():
            lib_name = self.libs.get_lib_name(function.lib_id)
            code = objdump.disassemble_function(lib_name, function.start_addr, function.addr_len)
            function.disassemble_code = code

    def gen_record_info(self, out):
        record_info = {}
        timestamp = self.meta_info.get('timestamp')
        if timestamp:
            t = datetime.datetime.fromtimestamp(int(timestamp))
        else:
            t = datetime.datetime.now()
        record_info['recordTime'] = t.strftime('%Y-%m-%d (%A) %H:%M:%S')

        product_props = self.meta_info.get('product_props')
        machine_type = self.arch
        if product_props:
            manufacturer, model, name = product_props.split(':')
            machine_type = '%s (%s) by %s, arch %s' % (model, name, manufacturer, self.arch)
        record_info['machineType'] = machine_type
        record_info['androidVersion'] = self.meta_info.get('android_version', '')
        record_info['recordCmdline'] = self.cmdline
        record_info['totalSamples'] = self.total_samples
        record_info['processNames'] = self._gen_process_names()
        record_info['threadNames'] = self._gen_thread_names()
        record_info['libList'] = self._gen_lib_list()
        record_info['functionMap'] = self._gen_function_map()
        record_info['sampleInfo'] = self._gen_sample_info()
        record_info['sourceFiles'] = self._gen_source_files()
        out.add(json.dumps(record_info))

    def _gen_process_names(self):
        process_names = {}
        for event in self.events.values():
            for process in event.processes.values():
                process_names[process.pid] = process.name
        return process_names

    def _gen_thread_names(self):
        thread_names = {}
        for event in self.events.values():
            for process in event.processes.values():
                for thread in process.threads.values():
                    thread_names[thread.tid] = thread.name
        return thread_names

    def _modify_text_for_html(self, text):
        # TODO: this is only needed for funcNames.
        array = []
        for c in text:
            if c == '\\' or c == "'":
                c = '\\' + c
            elif c == '\t':
                c = '&nbsp;' * 4
            elif c == '\n':
                c = '<br/>'
            elif c == '\r':
                c = ''
            elif c == '>':
                c = '&gt;'
            elif c == '<':
                c = '&lt;'
            elif c == '&':
                c = '&amp;'
            elif c == ' ':
                c = '&nbsp;'
            array.append(c)
        return ''.join(array)

    def _gen_lib_list(self):
        return [self._modify_text_for_html(x) for x in self.libs.lib_id_to_name]

    def _gen_function_map(self):
        func_map = {}
        for func_id in sorted(self.functions.funcs_by_id.keys()):
            function = self.functions.funcs_by_id[func_id]
            data = {}
            data['l'] = function.lib_id
            data['f'] = self._modify_text_for_html(function.func_name)
            if function.source_file_id is not None:
                data['s'] = function.source_file_id
                data['r'] = (function.line_range.start_line, function.line_range.end_line)
            if function.disassemble_code:
                data['d'] = function.disassemble_code
            func_map[func_id] = data
        return func_map

    def _gen_sample_info(self):
        add_addr_hit_map = self.need_disassemble_code
        return [event.get_sample_info(add_addr_hit_map) for event in self.events.values()]

    def _gen_source_files(self):
        result = []
        source_files = sorted(self.source_files.source_files.values(), key=lambda x: x.file_id)
        
        for source_file in source_files:
            data = {}
            data['path'] = source_file.file_path
            #modified_code = {}
            #for line in source_file.code:
            #    modified_code[line] = self._modify_text_for_html(source_file.code[line])
            data['code'] = source_file.code
            result.append(data)
        return result


class ReportGenerator(object):

    def __init__(self, html_path):
        self.hw = HtmlWriter(html_path)
        self.hw.open_tag('html')
        self.hw.open_tag('head')
        self.hw.open_tag('link', rel='stylesheet', type='text/css',
            href='https://code.jquery.com/ui/1.12.0/themes/smoothness/jquery-ui.css'
                         ).close_tag()

        self.hw.open_tag('link', rel='stylesheet', type='text/css',
             href='https://cdn.datatables.net/1.10.16/css/jquery.dataTables.min.css'
                         ).close_tag()
        self.hw.open_tag('script', src='https://www.gstatic.com/charts/loader.js').close_tag()
        self.hw.open_tag('script').add(
            "google.charts.load('current', {'packages': ['corechart', 'table']});").close_tag()
        self.hw.open_tag('script', src='https://code.jquery.com/jquery-3.2.1.js').close_tag()
        self.hw.open_tag('script', src='https://code.jquery.com/ui/1.12.1/jquery-ui.js'
                         ).close_tag()
        self.hw.open_tag('script',
            src='https://cdn.datatables.net/1.10.16/js/jquery.dataTables.min.js').close_tag()
        self.hw.open_tag('script',
            src='https://cdn.datatables.net/1.10.16/js/dataTables.jqueryui.min.js').close_tag()
        self.hw.open_tag('style', type='text/css').add("""
            .colForCount { width: 100px; }
            .colForLine { width: 50px; }
            """).close_tag()

        self.hw.close_tag('head')
        self.hw.open_tag('body')
        self.record_info = {}

    def write_content_div(self):
        self.hw.open_tag('div', id='report_content').close_tag()

    def write_record_data(self, record_data):
        self.hw.open_tag('script', id='record_data', type='application/json')
        record_data.gen_record_info(self.hw)
        self.hw.close_tag()

    def write_flamegraph(self, flamegraph):
        self.hw.add(flamegraph)

    def write_script(self):
        #self.hw.open_tag('script').add_file('report_html.js').close_tag()
        self.hw.open_tag('script', src='report_html.js').close_tag()

    def finish(self):
        self.hw.close_tag('body')
        self.hw.close_tag('html')
        self.hw.close()


def gen_flamegraph(record_file):
    fd, flamegraph_path = tempfile.mkstemp()
    os.close(fd)
    inferno_script_path = os.path.join(get_script_dir(), 'inferno', 'inferno.py')
    subprocess.check_call([sys.executable, inferno_script_path, '-sc', '-o', flamegraph_path,
                           '--record_file', record_file, '--embedded_flamegraph', '--no_browser'])
    with open(flamegraph_path, 'r') as fh:
        data = fh.read()
    remove(flamegraph_path)
    return data


def main():
    parser = argparse.ArgumentParser(description='report profiling data')
    parser.add_argument('-i', '--record_file', default='perf.data', help="""
                        Set profiling data file to report.""")
    parser.add_argument('-o', '--report_path', default='report.html', help="""
                        Set output html file.""")
    parser.add_argument('--min_func_percent', default=0.01, type=float, help="""
                        Set min percentage of functions shown in the report.
                        For example, when set to 0.01, only functions taking >= 0.01%% of total
                        event count are collected in the report.""")
    parser.add_argument('--min_callchain_percent', default=0.01, type=float, help="""
                        Set min percentage of callchains shown in the report.
                        It is used to limit nodes shown in the function flamegraph. For example,
                        when set to 0.01, only callchains taking >= 0.01%% of the event count of
                        the starting function are collected in the report.""")
    parser.add_argument('--add-source-code', action='store_true', help='Add source code.')
    parser.add_argument('--source-dirs', nargs='+', help='Find source code in selected directories.')
    parser.add_argument('--addr2line', nargs=1, help="Set the path of addr2line.")
    parser.add_argument('--add-disassemble-code', action='store_true', help='Add disassemble code.')
    parser.add_argument('--no_browser', action='store_true', help="Don't open report in browser.")
    args = parser.parse_args()

    binary_cache_path = 'binary_cache'
    if args.add_source_code or args.add_disassemble_code:
        if not os.path.isdir(binary_cache_path):
            log_exit("""binary_cache doesn't exist. Can't add source code or disassemble code
                     without collected binaries. Please run binary_cache_builder.py to collect
                     binaries for current profiling data, or run app_profiler.py without -nb
                     option.""")
    if not os.path.isdir(binary_cache_path):
        binary_cache_path = None

    report_generator = ReportGenerator(args.report_path)
    report_generator.write_content_div()
    record_data = RecordData(args.record_file, binary_cache_path)
    need_addr_hit_map = args.add_source_code or args.add_disassemble_code
    record_data.load_record_file(need_addr_hit_map)
    record_data.limit_percents(args.min_func_percent, args.min_callchain_percent)

    if args.add_source_code:
        if not args.source_dirs:
            log_exit('--source-dirs is needed to add source code.')
        addr2line_path = None if not args.addr2line else args.addr2line[0]
        record_data.add_source_code(args.source_dirs, addr2line_path)
    if args.add_disassemble_code:
        record_data.add_disassemble_code()

    report_generator.write_record_data(record_data)
    report_generator.write_script()
    flamegraph = gen_flamegraph(args.record_file)
    report_generator.write_flamegraph(flamegraph)
    report_generator.finish()

    if not args.no_browser:
        open_report_in_browser(args.report_path)
    log_info("Report generated at '%s'." % args.report_path)


if __name__ == '__main__':
    main()
