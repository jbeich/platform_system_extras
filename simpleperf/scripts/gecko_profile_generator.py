#!/usr/bin/env python3
#
# Copyright (C) 2021 The Android Open Source Project
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

"""gecko_profile_generator.py: converts perf.data to Gecko Profile Format,
    which can be read by https://profiler.firefox.com/.

  Example:
    ./app_profiler.py
    ./gecko_profile_generator.py | gzip > gecko-profile.json.gz

  Then open gecko-profile.json.gz in https://profiler.firefox.com/
"""

import argparse
import json
import sys

from dataclasses import dataclass, field
from simpleperf_report_lib import ReportLib
from simpleperf_utils import flatten_arg_list
from typing import List, Dict, Optional, Set, Tuple


StringID = int
StackID = int
FrameID = int
CategoryID = int
Frame = Tuple[
    StringID,
    bool,
    int,
    None,
    None,
    None,
    None,
    CategoryID,
    int
]
Stack = Tuple[Optional[StackID], FrameID, CategoryID]
Timestamp = float
Sample = Tuple[Optional[StackID], Timestamp, int]
GeckoProfile = Dict


@dataclass
class Thread:
  comm: str
  pid: int
  tid: int
  samples: List[Sample] = field(default_factory=list)
  # interned stack frame ID -> stack frame.
  frameTable: List[Frame] = field(default_factory=list)
  # interned string ID -> string.
  stringTable: List[str] = field(default_factory=list)
  # TODO: this is redundant with frameTable, could we remove this?
  stringMap: Dict[str, int] = field(default_factory=dict)
  # interned stack ID -> stack.
  stackTable: List[Stack] = field(default_factory=list)
  # Maps from (stack prefix ID, leaf stack frame ID) to interned Stack ID.
  stackMap: Dict[Tuple[Optional[int], int], int] = field(default_factory=dict)
  # Map from stack frame string to Frame ID.
  frameMap: Dict[str, int] = field(default_factory=dict)

  # Gets a matching stack, or saves the new stack. Returns a Stack ID.
  def intern_stack(self, frame_id: int, prefix_id: Optional[int]) -> int:
    key = (prefix_id, frame_id)
    stack_id = self.stackMap.get(key)
    if stack_id is not None:
      return stack_id
    stack_id = len(self.stackTable)
    category = 0
    self.stackTable.append((prefix_id, frame_id, category))
    self.stackMap[key] = stack_id
    return stack_id

  # Gets a matching string, or saves the new string. Returns a String ID.
  def intern_string(self, string: str) -> int:
    string_id = self.stringMap.get(string)
    if string_id is not None:
      return string_id
    string_id = len(self.stringTable)
    self.stringTable.append(string)
    self.stringMap[string] = string_id
    return string_id

  # Gets a matching stack frame, or saves the new frame. Returns a Frame ID.
  def intern_frame(self, frame_str: str) -> int:
    frame_id = self.frameMap.get(frame_str)
    if frame_id is not None:
      return frame_id
    frame_id = len(self.frameTable)
    self.frameMap[frame_str] = frame_id
    string_id = self.intern_string(frame_str)

    relevantForJS = False
    innerWindowID = 0
    implementation = None
    optimizations = None
    line = None
    column = None
    category = 0
    if "kallsyms" in frame_str:
      category = 1
    subcategory = 0
    self.frameTable.append((
      string_id,
      relevantForJS,
      innerWindowID,
      implementation,
      optimizations,
      line,
      column,
      category,
      subcategory,
    ))
    return frame_id

  def add_sample(self, comm: str, stack: List[str], time: float):
    # Unix threads often don't set their name immediately upon creation.
    # Use the last name
    if self.comm != comm:
      self.comm = comm

    prefix_stack_id = None
    for frame in stack:
      frame_id = self.intern_frame(frame)
      prefix_stack_id = self.intern_stack(frame_id, prefix_stack_id)

    stack_id = prefix_stack_id
    responsiveness = 0
    self.samples.append((stack_id, time, responsiveness))

  def to_json_object(self) -> Dict:
    # The samples aren't guaranteed to be in order. Sort them by time.
    self.samples.sort(key=lambda s: s[1])
    return {
        "tid": self.tid,
        "pid": self.pid,
        "name": self.comm,
        "markers": {
            "schema": {
                "name": 0,
                "startTime": 1,
                "endTime": 2,
                "phase": 3,
                "category": 4,
                "data": 5,
            },
            "data": [],
        },
        "samples": {
            "schema": {
                "stack": 0,
                "time": 1,
                "responsiveness": 2,
            },
            "data": self.samples
        },
        "frameTable": {
            "schema": {
                "location": 0,
                "relevantForJS": 1,
                "innerWindowID": 2,
                "implementation": 3,
                "optimizations": 4,
                "line": 5,
                "column": 6,
                "category": 7,
                "subcategory": 8,
            },
            "data": self.frameTable,
        },
        "stackTable": {
            "schema": {
                "prefix": 0,
                "frame": 1,
                "category": 2,
            },
            "data": self.stackTable,
        },
        "stringTable": self.stringTable,
        "registerTime": 0,
        "unregisterTime": None,
        "processType": "default",
    }


def gecko_profile(
    record_file: str,
    symfs_dir: str,
    kallsyms_file: str,
    proguard_mapping_file: List[str],
    comm_filter: Set[str]) -> GeckoProfile:
  """convert a simpleperf profile to gecko format"""
  lib = ReportLib()

  lib.ShowIpForUnknownSymbol()
  for file_path in proguard_mapping_file:
    lib.AddProguardMappingFile(file_path)
  if symfs_dir is not None:
    lib.SetSymfs(symfs_dir)
  if record_file is not None:
    lib.SetRecordFile(record_file)
  if kallsyms_file is not None:
    lib.SetKallsymsFile(kallsyms_file)

  categories = [
      {
          "name": 'User',
          "color": 'yellow',
          "subcategories": ['Other']
      },
      {
          "name": 'Kernel',
          "color": 'orange',
          "subcategories": ['Other']
      },
      {
          "name": 'Other',
          "color": 'grey',
          "subcategories": ['Other']
      },
  ];

  arch = lib.GetArch()
  meta_info = lib.MetaInfo()
  record_cmd = lib.GetRecordCmd()

  # Map from tid to Thread
  threadMap: Dict[int, Thread] = {}

  while True:
    sample = lib.GetNextSample()
    if sample is None:
        lib.Close()
        break
    if comm_filter:
      if sample.thread_comm not in comm_filter:
        continue
    event = lib.GetEventOfCurrentSample()
    symbol = lib.GetSymbolOfCurrentSample()
    callchain = lib.GetCallChainOfCurrentSample()
    sample_time_ms = sample.time / 1000000

    stack = []
    stack.append('%s (in %s)' % (symbol.symbol_name, symbol.dso_name))
    for i in range(callchain.nr):
        entry = callchain.entries[i]
        stack.append('%s (in %s)' % (entry.symbol.symbol_name, entry.symbol.dso_name))
    stack.reverse()

    # add thread sample
    thread = threadMap.get(sample.tid)
    if thread is None:
      thread = Thread(comm=sample.thread_comm, pid=sample.pid, tid=sample.tid)
      threadMap[sample.tid] = thread
    thread.add_sample(comm=sample.thread_comm, stack=stack, time=sample_time_ms)

  threads = [thread.to_json_object() for thread in threadMap.values()]

  gecko_profile_meta = {
      "interval": 1,
      "processType": 0,
      "product": record_cmd,
      "device": meta_info.get("product_props"),
      "platform": meta_info.get("android_build_fingerprint"),
      "stackwalk": 1,
      "debug": 0,
      "gcpoison": 0,
      "asyncstack": 1,
      "startTime": int(meta_info.get('timestamp')) * 1000,
      "shutdownTime": None,
      "version": 24,
      "presymbolicated": True,
      "categories": categories,
      "markerSchema": [],
      "abi": arch,
      "oscpu": meta_info.get("android_build_fingerprint"),
  }

  # https://github.com/firefox-devtools/profiler/blob/main/docs-developer/gecko-profile-format.md
  return {
      "meta": gecko_profile_meta,
      "libs": [],
      "threads": threads,
      "processes": [],
      "pausedRanges": [],
  }


def main():
  parser = argparse.ArgumentParser(
      description='Converts simplerperf\'s perf.data to Gecko Profile Format ' +
                  'JSON for opening in https://profiler.firefox.com/')
  parser.add_argument('--symfs',
                      help='Set the path to find binaries with symbols and debug info.')
  parser.add_argument('--kallsyms', help='Set the path to find kernel symbols.')
  parser.add_argument('record_file', nargs='?', default='perf.data',
                      help='Default is perf.data.')
  parser.add_argument(
      '--proguard-mapping-file', nargs='+',
      help='Add proguard mapping file to de-obfuscate symbols',
      default = [])
  parser.add_argument('--comm', nargs='+', action='append', help="""
      Use samples only in threads with selected names.""")
  args = parser.parse_args()
  profile = gecko_profile(
      record_file=args.record_file,
      symfs_dir=args.symfs,
      kallsyms_file=args.kallsyms,
      proguard_mapping_file=args.proguard_mapping_file,
      comm_filter=set(flatten_arg_list(args.comm)))

  json.dump(profile, sys.stdout, sort_keys=True)


if __name__ == '__main__':
    main()
