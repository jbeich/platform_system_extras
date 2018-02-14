#!/usr/bin/python
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
#

from __future__ import print_function
import os
import subprocess
import sys

SIMPLEPERF_VERSION = 1

def get_script_dir():
    return os.path.dirname(os.path.realpath(__file__))

def get_revision():
    revision = subprocess.check_output("git -C %s rev-parse --short=12 HEAD 2>/dev/null" %
                                       get_script_dir(), shell=True)
    return revision.strip()

def gen_version_cpp(revision, outfile):
    with open(outfile, 'w') as fh:
        fh.write("""
                 static const char* simpleperf_version = "%d.%s";
                 const char* GetSimpleperfVersion() { return simpleperf_version; }
                 """ % (SIMPLEPERF_VERSION, revision))

def gen_deps(depfile, outfile):
    """ Collect all .cpp, .h files under simpleperf source directory.
        Add them to the deps of simpleperf version.
    """
    deps = set()
    top_dir = get_script_dir()
    deps.add(top_dir)
    for entry in os.listdir(top_dir):
        if os.path.splitext(entry)[1] in ['.cpp', '.h']:
            deps.add(os.path.join(top_dir, entry))
    with open(depfile, 'w') as fh:
        fh.write("%s : %s" % (outfile, ' '.join(deps)))

def main():
    assert len(sys.argv) == 3
    depfile, outfile = sys.argv[1:]
    revision = get_revision()
    gen_version_cpp(revision, outfile)
    gen_deps(depfile, outfile)

if __name__ == '__main__':
    main()