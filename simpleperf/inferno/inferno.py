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

"""
    Inferno is a tool to generate flamegraphs for android programs. It was originally written
    to profile surfaceflinger (Android compositor) but it can be used for other C++ program.
    It uses simpleperf to collect data. Programs have to be compiled with frame pointers which
    excludes ART based programs for the time being.

    Here is how it works:

    1/ Data collection is started via simpleperf and pulled locally as "perf.data".
    2/ The raw format is parsed, callstacks are merged to form a flamegraph data structure.
    3/ The data structure is used to generate a SVG embedded into an HTML page.
    4/ Javascript is injected to allow flamegraph navigation, search, coloring model.

"""

from scripts.simpleperf_report_lib import *
import argparse
from data_types import *
from svg_renderer import *
import datetime
import webbrowser

def create_process(args):
    """ Retrieves target process pid and create a process contained.

    :param args: Argument as parsed by argparse
    :return: Process objectk
    """
    piof_output = subprocess.check_output(["adb", "shell", "pidof", args.process_name])

    try:
      process_id = int(piof_output)
    except ValueError:
      process_id = 0

    process = Process(args.process_name, process_id)
    return process


def collect_data(process):
    """ Start simpleperf on device and collect data. Pull perf.data into cwd.

    :param process:  Process object
    :return: Populated Process object
    """
    process.cmd = "simpleperf record \
    --dump-symbols \
    -o /data/local/perf.data \
    --call-graph fp \
    -p %s \
    --duration %s \
    -f 6000" % (
        process.pid,
        process.args.capture_duration)

    print("Process '%s' PID = %d" % (process.name, process.pid))

    if process.args.skip_collection:
       print("Skipping data collection, expecting perf.data in folder")
       return process

    print("Sampling for %s seconds..." % process.args.capture_duration)

    err = subprocess.call(["adb", "shell", process.cmd])
    err = subprocess.call(["adb", "pull", "/data/local/perf.data", "."])
    return process


def parse_samples(process, args):
    """ read record_file, and print each sample"""

    record_file = args.record_file
    symfs_dir = args.symfs
    kallsyms_file = args.kallsyms

    lib = ReportLib()

    lib.ShowIpForUnknownSymbol()
    if symfs_dir is not None:
        lib.SetSymfs(symfs_dir)
    if record_file is not None:
        lib.SetRecordFile(record_file)
    if kallsyms_file is not None:
        lib.SetKallsymsFile(kallsyms_file)

    while True:
        sample = lib.GetNextSample()
        if sample is None:
            lib.Close()
            break
        symbol = lib.GetSymbolOfCurrentSample()
        callchain = lib.GetCallChainOfCurrentSample()
        process.get_thread(sample.tid).add_callchain(callchain, symbol, sample)
        process.num_samples += 1

    print("Parsed %s callchains." % process.num_samples)


def collapse_callgraphs(process):
    """
    For each thread, collapse all callgraph into one flamegraph.
    :param process:  Process object
    :return: None
    """
    for _, thread in process.threads.items():
        thread.collapse_flamegraph()


def get_local_asset_content(local_path):
    """
    Retrieves local package text content
    :param local_path: str, filename of local asset
    :return: str, the content of local_path
    """
    f = open(os.path.join(os.path.dirname(__file__), local_path), 'r')
    content = f.read()
    f.close()
    return content


def output_report(process):
    """
    Generates a HTML report representing the result of simpleperf sampling as flamegraph
    :param process: Process object
    :return: str, absolute path to the file
    """
    f = open('report.html', 'w')
    filepath = os.path.realpath(f.name)
    f.write("<html>")
    f.write("<body style='font-family: Monospace;'>")
    f.write('<style type="text/css"> .n:hover { stroke:black; stroke-width:0.5; cursor:pointer; } </style>')
    f.write('<style type="text/css"> .t:hover { cursor:pointer; } </style>')
    f.write('<img height="180" alt = "Embedded Image" src ="data')
    f.write(get_local_asset_content("inferno.b64"))
    f.write('"/>')
    f.write("<div style='display:inline-block;'> \
    <font size='8'>\
    <u>Inferno Flamegraph Report</u></font><br/><br/> \
    Process : %s (%d)<br/>\
    Date&nbsp;&nbsp;&nbsp;&nbsp;: %s<br/>\
    Threads : %d <br/>\
    Samples : %d</br>\
    Duration: %s seconds<br/>\
    Machine : %s (%s) by %s<br/>\
    Capture : %s<br/><br/></div>"
            % (
                process.name,process.pid,
                datetime.datetime.now().strftime("%Y-%m-%d (%A) %H:%M:%S"),
                len(process.threads),
                process.num_samples,
                process.args.capture_duration,
                process.props["ro.product.model"], process.props["ro.product.name"],
                process.props["ro.product.manufacturer"],
                process.cmd))
    f.write(get_local_asset_content("script.js"))

    # Output tid == pid Thread first.
    main_thread = [x for _, x in process.threads.items() if x.tid == process.pid]
    for thread in main_thread:
        f.write("<br/><br/><b>Main Thread %d :</b><br/>\n\n\n\n" % thread.tid)
        renderSVG(thread.flamegraph, f, process.args.color)

    other_threads = [x for _, x in process.threads.items() if x.tid != process.pid]
    for thread in other_threads:
        f.write("<br/><br/><b>Thread %d :</b><br/>\n\n\n\n" % thread.tid)
        renderSVG(thread.flamegraph, f, process.args.color)

    f.write("</body>")
    f.write("</html>")
    f.close()
    return "file://" + filepath

def generate_flamegraph_offsets(flamegraph):
    rover = flamegraph.offset
    for callsite in flamegraph.callsites:
        callsite.offset = rover
        rover += callsite.num_samples
        generate_flamegraph_offsets(callsite)


def generate_threads_offsets(process):
    for _, thread in process.threads.items():
        generate_flamegraph_offsets(thread.flamegraph)


def collect_machine_info(process):
    props = subprocess.check_output(["adb", "shell", "getprop"])
    lines = props.split("\n")
    for line in lines:
        tokens = line.split(": ")
        if len(tokens) < 2:
            continue
        key = tokens[0].replace("[", "").replace("]", "")
        value = tokens[1].replace("[", "").replace("]", "")
        process.props[key] = value


def setup_adb():
    err = subprocess.call(["adb", "root"])
    return err
    # TODO
    # If adb cannot run as root, there is still a way to collect data but it is much more complicated.
    # 1. Identify the platform os and abi, use process.props:
    #      - ro.product.cpu.abi for ABI
    #      - ??? for OS
    # 2. Push the precompiled scripts/bin/[OS]/[ARCH]/simpleperf to device /data/local/simpleperf
    # 3. Get target app installation folder via YYY
    # 4. Use run-as to copy /data/local/simplerperf -> /apps/installation_path/simpleperf
    # 5. Use run-as to run: /apps/installation_path/simpleperf -p APP_PID -o /apps/installation_path/perf.data
    # 6. Use run-as fork+pipe trick to copy /apps/installation_path/perf.data to /data/local/perf.data
    # 7. adb pull /data/local/perf.data .

def main():

    parser = argparse.ArgumentParser(description='Report samples in perf.data.')
    parser.add_argument('--symfs', help='Set the path to find binaries with symbols and debug info.')
    parser.add_argument('--kallsyms', help='Set the path to find kernel symbols.')
    parser.add_argument('--record_file', default='perf.data', help='Default is perf.data.')
    parser.add_argument('--capture_duration', default=10, help='Capture duration in seconds.')
    parser.add_argument('--process_name', default='surfaceflinger', help='Default is surfaceflinger.')
    parser.add_argument('--color', default='hot', choices=['hot', 'dso', 'legacy'],
                        help='hot=percentage of samples, dso=callsite DSO name, legacy=brendan style')
    parser.add_argument('--skip_collection', default=False, help='Skip data collection', action="store_true")
    args = parser.parse_args()

    # Since we may attempt to sample privileged process, let's try to be root.
    err = setup_adb()
    if err != 0:
        print("Unable to setup adb properly (error code %s)." % err)
        return

    # Create a process object
    process = create_process(args)
    process.args = args

    print("Starting data collection stage for process '%s'." % args.process_name)
    collect_data(process)
    if process.pid == 0:
        print("Unable to retrive pid for process '%s'. Terminating." % process.name)
        return

    collect_machine_info(process)
    parse_samples(process, args)
    collapse_callgraphs(process)
    generate_threads_offsets(process)
    report_path = output_report(process)
    webbrowser.open(report_path, new=0, autoraise=True)
    print "Report generated at '%s'." % report_path

if __name__ == "__main__":
    main()