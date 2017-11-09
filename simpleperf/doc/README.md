# Simpleperf

Simpleperf is a native CPU profiling tool for Android. It can:
  1. profile both Android applications and native processes running on Android.
  2. profile both Java and C++ code on Android.
  3. be used on Android L and above.

Simpleperf is part of the Android Open Source Project.
Its source code is [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/).
Its latest document is [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/doc/README.md).
Bugs and feature requests can be submitted at http://github.com/android-ndk/ndk/issues.


## Table of Contents

- [Introduction](#introduction)
- [Tools in ndk release](#tools-in-ndk-release)
- [Android application profiling](#android-application-profiling)
    - [Prepare an Android application](#prepare-an-android-application)
    - [Record and report profiling data](#record-and-report-profiling-data)
    - [Record call graph](#record-call-graph)
    - [Report call graph](#report-call-graph)
    - [Report in html interface](#report-in-html-interface)
    - [Show flame graph](#show-flame-graph)
    - [Parse the profiling data manually](#parse-the-profiling-data-manually)
    - [Trace off CPU time](#trace-off-cpu-time)
    - [Profile from launch](#profile-from-launch)

- [Native executable profiling]
    - Profile an native executable in aosp, like dex2oat.

- [System wide profiling]
    - Do system wide profiling. only supported by cmdline.

- [Executable commands reference](#executable-commands-reference)
    - [How simpleperf executable works?](#how-simpleperf-executable-works)
    - [Commands](#commands)
    - [The list command](#the-list-command)
    - [The stat command](#the-stat-command)
        - [Select events to stat](#select-events-to-stat)
        - [Select target to stat](#select-target-to-stat)
        - [Decide how long to stat](#decide-how-long-to-stat)
        - [Decide the print interval](#decide-the-print-interval)
        - [Display counters in systrace](#display-counters-in-systrace)
    - [The record command](#the-record-command)
        - [Select events to record](#select-events-to-record)
        - [Select target to record](#select-target-to-record)
        - [Set the frequency to record](#set-the-frequency-to-record)
        - [Decide how long to record](#decide-how-long-to-record)
        - [Set the path to store records](#set-the-path-to-store-records)
        - [Record call graph](#record-call-graph)
        - [Record off CPU time](#record-off-cpu-time)
    - [The report command](#the-report-command)
        - [Set the path to read records](#set-the-path-to-read-records)
        - [Set the path to find binaries](#set-the-path-to-find-binaries)
        - [Filter records](#filter-records)
        - [Group records into sample entries](#group-records-into-sample-entries)
        - [Report call graph](#report-call-graph)
- [Scripts reference](#scripts-reference)
    - [app_profiler py](#app_profiler-py)
        - [Profile from launch of an application](#profile-from-launch-of-an-application)
    - [report.py](#report-py)
    - [report_html.py](#report_html-py)
    - [inferno](#inferno)
- [Suggestions](#suggestions)
    - [profile on android >= N devices](#profile-on-android-n-devices)
    - [How to solve missing symbols in report](#how-to-solve-missing-symbols-in-report)
    - [How to improve call graph result](#how-to-improve-call-graph-result)

## Introduction

Simpleperf contains two parts: simpleperf executable and python scripts.

Simpleperf executable works similar to linux-tools-perf, but having some specific features
for Android profiling environment:

1. It collects more info in profiling data. Since the common workflow is "record on device,
   and report on host". Simpleperf collects needed symbols in profiling data, as well as device
   info, recording time.

2. It delivers new features for recording.
   a. When recording dwarf based call graph, unwind the stack before writing a sample to file. It
      saves storage space on device.
   b. Trace both on CPU time and off CPU time with --trace-offcpu option.

3. It relates closely to Android platform.
   a. Relies on many libraries maintained in android open source projects, to do jobs like stack
      unwinding.
   b. Is aware of Android environment, like using system property to enable profiling, using run-as
      to profile in application's context.
   c. Supports reading symbols and debug information from .gnu_debugdata section. Because system
      libraries are built with .gnu_debugdata section starting from Android O.
   d. Supports profiling embedded shared libraries in apk files.

4. It builds executables and shared libraries for different usages.
   a. Static executable on device. Since static executables don't rely on any library, simpleperf
      executable can be pushed on any Android device and used to record profiling data.
   b. Executables on different hosts: Linux, Mac and Windows. These executables can be used to
      report on host.
   c. Report shared libraries on different hosts. The report library is used by different python
      scripts to parse profiling data.

The detail of simpleperf executable is [here](#executable-commands-reference).

Python scripts are split into three parts according to their functions:

1. Scripts used for making recording simple, like app_profiler.py. app_profiler.py records
   profiling data for Android applications and native executables, pulls profiling data and related
   binaries on host.

2. Scripts used for reporting, like report.py, report_html.py, inferno.

3. Script used for parsing profiling data. It is simpleperf_report_lib.py, which provides a simple
   interface for users to parse profiling data, and convert it to whatever they want.

The detail of python scripts is [here](#scripts-reference).

## Tools in ndk release

bin/: contains executables and shared libraries.

bin/android/${arch}/simpleperf: static simpleperf executable on device.

bin/${host}/${arch}/simpleperf: simpleperf executable on host, only supports reporting.

bin/${host}/${arch}/libsimpleperf_report.${so/dylib/dll}: report shared library on host.

app_profiler.py: for recording.

binary_cache_builder.py: building binary cache for profiling data.

report.py: stdio interface for reporting.

report_html.py: html interface for reporting.

inferno.sh (or inferno.bat on Windows): generating flamegraph.

pprof_proto_generator.py: convert profiling data to format used by [pprof](https://github.com/google/pprof).

report_sample.py: convert profiling data to format used by [FlameGraph](https://github.com/brendangregg/FlameGraph).

simpleperf_report_lib.py: script used for parsing profiling data.

inferno/: implementation of inferno. Used by inferno.sh.

doc/: documentation of simpleperf and inferno.

## Android application profiling

This section shows how to profile an Android application.
Some examples are [Here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/README.md).

Simpleperf only supports profiling native instructions in binaries in ELF format. If the Java code
is executed by interpreter, or with jit cache, it can’t be profiled by simpleperf. As Android
supports Ahead-of-time compilation, it can compile Java bytecode into native instructions with
debug information. On devices with Android version <= M, we need root privilege to compile Java
bytecode with debug information. However, on devices with Android version >= N, we don't need root
privilege to do so.

Profiling an Android application involves three steps:
1. Prepare the application.
2. Record profiling data.
3. Report profiling data.

### Prepare an Android application

Before profiling, we need to install the application on Android device. To get valid profiling
results, please check following items:

1. The application should be debuggable.
It means setting true of [android:debuggable](https://developer.android.com/guide/topics/manifest/application-element.html#debug)
So instead of release build type, we use debug [build type](https://developer.android.com/studio/build/build-variants.html#build-types).
Or you can add a profiling build type as [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExampleWithNative/app/profiling.gradle).
It is for security reason that we can only profile debuggable apps on users' device. However, on
a rooted device, the application doesn't need to be debuggable.

2. Run on an Android >= N device.
We suggest profiling on an Android >= N device. The reason is [here](#profile-on-android-n-devices).

3. On Android O, add `wrap.sh` in the apk.
To profile Java code, we need ART running in oat mode. But on Android O, debuggable applications
are forced to run in jit mode. To work around this, we need to add a `wrap.sh` in the apk. So if
you are running on Android O device and need to profile java code, add `wrap.sh` as in [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExampleWithNative/app/profiling.gradle).

4. Make sure C++ code is compiled with optimizing flags.
If the application contains C++ code, it can be compiled with -O0 flag in debug build type.
This makes C++ code slow, to avoid that, check [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExampleWithNative/app/profiling.gradle).

5. Use native libraries with debug info in the apk when possible.
If the application contains C++ code or pre-compiled native libraries, try to use unstripped
libraries in the apk. This helps simpleperf generating better profiling results.
To use unstripped libraries, check [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExampleWithNative/app/profiling.gradle).

Here we use application [SimpleperfExampleWithNative](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExampleWithNative).
It builds an app-profiling.apk for profiling.

```sh
$ git clone https://android.googlesource.com/platform/system/extras
$ cd extras/simpleperf/demo
# Open SimpleperfExamplesWithNative project with Android studio, and build this project
# successfully, otherwise the `./gradlew` command below will fail.
$ cd SimpleperfExampleWithNative

# On windows, use "gradlew" instead.
$ ./gradlew clean assemble
$ adb install -r app/build/outputs/apk/app-profiling.apk
```

### Record and report profiling data

We can use `app-profiler.py` to profile Android applications.
Its detail is [here](#app_profiler-py).

```sh
# Record perf.data.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative
```

If running successfully, it will collect profiling data in perf.data in the current directory,
and collect related native binaries in binary_cache/.

Normally we need to use the app when profiling, otherwise we may record no samples. But in this
case, the MainActivity starts a busy thread. So we don't need to use the app while profiling.

```sh
# Report perf.data in stdio interface.
$ python report.py
Cmdline: /data/local/tmp/simpleperf record -e task-clock:u -g -f 1000 --duration 10 ...
Arch: arm64
Event: cpu-cycles:u (type 0, config 0)
Samples: 9966
Event count: 22661027577

Overhead  Command          Pid    Tid    Shared Object            Symbol
59.69%    amplewithnative  10440  10452  /system/lib64/libc.so    strtol
8.60%     amplewithnative  10440  10452  /system/lib64/libc.so    isalpha
...
```

The detail of report.py is [here](#report-py). If there are a lot of unknown symbols in the report,
check [here](#how-to-solve-missing-symbols-in-report).

```sh
# Report perf.data in html interface.
$ python report_html.py

# Add source code and disassembly. Change the path of source_dirs if it not correct.
$ python report_html.py --add_source_code --source_dirs ../demo/SimpleperfExampleWithNative \
      --add_disassembly
```

Normally it should pop up a browser tab to show report.html after generating report.html.
The detail of report_html.py is [here](#report_html-py).

### Record call graph

A call graph is a tree showing function call relations. Below is an example.

```
main() {
    FunctionOne();
    FunctionTwo();
}
FunctionOne() {
    FunctionTwo();
    FunctionThree();
}
callgraph:
    main-> FunctionOne
       |    |
       |    |-> FunctionTwo
       |    |-> FunctionThree
       |
       |-> FunctionTwo
```

A call graph shows how a function calls other functions, and a reversed call graph shows how
a function is called by other functions. To show call graph, we need to first record it, then
report it.

There are two ways to record call graph, one is dwarf based call graph, the other is stack frame
based call graph.

```sh
# Add "-g" in -r option to record dwarf based call graph.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
        -r "-e task-clock:u -f 1000 --duration 10 -g"

# Add "--call-graph fp" in -r option to record stack frame based call graph.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
        -r "-e task-clock:u -f 1000 --duration 10 --call-graph fp"
```

Recording dwarf based call graph needs support of debug information in native binaries.

Recording stack frame based call graphs needs support of stack frame register.
Note that on arm architecture, the stack frame register is not well supported, even if compiled
using -O0 -g -fno-omit-frame-pointer options. It is because the kernel can't unwind user stack
containing both arm/thumb code. So please consider using dwarf based call graph on arm
architecture, or profiling in arm64 environment.

Please check [here](#how-to-improve-call-graph-result) for details to improve call graph result.

### Report call graph

```sh
# Report call graph in stdio interface.
$ python report.py -g

# Report call graph in a python Tk interface.
$ python report.py -g --gui
    # Double-click an item started with '+' to show its callgraph.
```

However, the recommended way to show call graph is to show it as a flame graph in html interface.

### Report in html interface

[report_html.py](#report_html-py) is the recommended way to show profiling result.

```
$ python report_html.py
```

### Show flame graph

To show flame graph, we also need to record call graph.

report_html.py shows flame grpah in the "Flamegraph" tab.

Or you can use [inferno](#inferno) to generate flame graph directly.

```sh
# On windows, use inferno.bat instead of ./inferno.sh.
$ ./inferno.sh -sc
```

You can also build flame graph based on scripts in
https://github.com/brendangregg/FlameGraph. Please make sure you have perl installed.

```sh
$ git clone https://github.com/brendangregg/FlameGraph.git
$ python report_sample.py --symfs binary_cache >out.perf
$ FlameGraph/stackcollapse-perf.pl out.perf >out.folded
$ FlameGraph/flamegraph.pl out.folded >a.svg
```

### Parse the profiling data manually

`simpleperf_report_lib.py` provides an interface to read samples from perf.data. By using it, You
can write python scripts to read perf.data and save profiling data in other formats. Examples
are report_sample.py, report_html.py.

### Trace off CPU time

The detail of trace off CPU time is [here](#record-off-cpu-time).
First check if --trace-offcpu option is supported on the device.

```sh
$ python run_simpleperf_on_device.py list --show-features
dwarf-based-call-graph
trace-offcpu
```

If trace-offcpu is supported, it will be shown in the feature list.
Then we can try it.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity \
    -r "-g -e task-clock:u -f 1000 --duration 10 --trace-offcpu"
$ python report_html.py --add_disassembly --add_source_code --source_dirs ../demo
```

### Profile from launch

The detal is [here](#profile-from-launch-of-an-application).

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .MainActivity \
    --arch arm64 --profile_from_launch
```

## Executable commands reference

### How simpleperf executable works?

Modern CPUs have a hardware component called the performance monitoring unit (PMU). The PMU has
several hardware counters, counting events like how many cpu cycles have happened, how many
instructions have executed, or how many cache misses have happened.

The Linux kernel wraps these hardware counters into hardware perf events. In addition, the Linux
kernel also provides hardware independent software events and tracepoint events. The Linux kernel
exposes all this to userspace via the perf_event_open system call, which simpleperf uses.

Simpleperf executable has three main commands: stat, record and report.

The stat command gives a summary of how many events have happened in the profiled processes in a
time period. Here’s how it works:
1. Given user options, simpleperf enables profiling by making a system call to the kernel.
2. The kernel enables counters when the profiled processes are running.
3. After profiling, simpleperf reads counters from the kernel, and reports a counter summary.

The record command records samples of the profiled processes in a time period. Here’s how it works:
1. Given user options, simpleperf enables profiling by making a system call to the kernel.
2. Simpleperf creates mapped buffers between simpleperf and the kernel.
3. The kernel enables counters when the profiled processes are running.
4. Each time a given number of events happen, the kernel dumps a sample to the mapped buffers.
5. Simpleperf reads samples from the mapped buffers and generates profiling data in a file called
   perf.data.

The report command reads perf.data and any shared libraries used by the profiled processes,
and outputs a report showing where the time was spent.

### Commands

Simpleperf executable supports several commands, listed as below:

```
The dump command: dump content in perf.data, used for debugging simpleperf.
The help command: print help information for other commands.
The kmem command: collect kernel memory allocation information (will be replaced by python scripts).
The list command: list all event types supported on the Android device.
The record command: profile processes, and record sampling info in perf.data.
The report command: report sampling info in perf.data.
The report-sample command: report raw sample info in perf.data, used for supporting integration of
                           simpleperf in Android Studio.
The stat command: profile processes and print counter summary.
```

Each command supports different options, which can be seen through help message.

```sh
# List all commands.
$ simpleperf --help

# Print help message for record command.
$ simpleperf record --help
```

Below describes the mainly used commands, which are list, stat, record and report.

### The list command

The list command lists all events available on the device. Different devices may support different
events because of differences in hardware and kernel.

```sh
$ simpleperf list
List of hw-cache events:
  branch-loads
  ...
List of hardware events:
  cpu-cycles
  instructions
  ...
List of software events:
  cpu-clock
  task-clock
  ...
```

On arm/arm64, simpleperf list also shows a list of raw events, they are the events supported by
the ARM PMU on the device. The kernel has wrapped part of them into hardware events and hw-cache
events. For example, raw-cpu-cycles is wrapped into cpu-cycles, raw-instruction-retired is wrapped
into instructions. The raw events are provided in case we want to use some events supported on the
device, but unfortunately not wrapped by the kernel.

### The stat command

The stat command is used to get event counter values of the profiled processes. By passing options,
we can select which events to use, which processes/threads to monitor, how long to monitor and the
print interval.

```sh
# Stat using default events (cpu-cycles,instructions,...), and monitor process 7394 for 10 seconds.
$ simpleperf stat -p 7394 --duration 10
Performance counter statistics:

 1,320,496,145  cpu-cycles         # 0.131736 GHz                     (100%)
   510,426,028  instructions       # 2.587047 cycles per instruction  (100%)
     4,692,338  branch-misses      # 468.118 K/sec                    (100%)
886.008130(ms)  task-clock         # 0.088390 cpus used               (100%)
           753  context-switches   # 75.121 /sec                      (100%)
           870  page-faults        # 86.793 /sec                      (100%)

Total test time: 10.023829 seconds.
```

#### Select events to stat

We can select which events to use via -e option.

```sh
# Stat event cpu-cycles.
$ simpleperf stat -e cpu-cycles -p 11904 --duration 10

# Stat event cache-references and cache-misses.
$ simpleperf stat -e cache-references,cache-misses -p 11904 --duration 10
```

When running the stat command, if the number of hardware events is larger than the number of
hardware counters available in the PMU, the kernel shares hardware counters between events, so each
event is only monitored for part of the total time. In the example below, there is a percentage at
the end of each row, showing the percentage of the total time that each event was actually
monitored.

```sh
# Stat using event cache-references, cache-references:u,....
$ simpleperf stat -p 7394 -e cache-references,cache-references:u,cache-references:k \
      -e cache-misses,cache-misses:u,cache-misses:k,instructions --duration 1
Performance counter statistics:

4,331,018  cache-references     # 4.861 M/sec    (87%)
3,064,089  cache-references:u   # 3.439 M/sec    (87%)
1,364,959  cache-references:k   # 1.532 M/sec    (87%)
   91,721  cache-misses         # 102.918 K/sec  (87%)
   45,735  cache-misses:u       # 51.327 K/sec   (87%)
   38,447  cache-misses:k       # 43.131 K/sec   (87%)
9,688,515  instructions         # 10.561 M/sec   (89%)

Total test time: 1.026802 seconds.
```

In the example above, each event is monitored about 87% of the total time. But there is no
guarantee that any pair of events are always monitored at the same time. If we want to have some
events monitored at the same time, we can use --group option.

```sh
# Stat using event cache-references, cache-references:u,....
$ simpleperf stat -p 7964 --group cache-references,cache-misses \
      --group cache-references:u,cache-misses:u --group cache-references:k,cache-misses:k \
      -e instructions --duration 1
Performance counter statistics:

3,638,900  cache-references     # 4.786 M/sec          (74%)
   65,171  cache-misses         # 1.790953% miss rate  (74%)
2,390,433  cache-references:u   # 3.153 M/sec          (74%)
   32,280  cache-misses:u       # 1.350383% miss rate  (74%)
  879,035  cache-references:k   # 1.251 M/sec          (68%)
   30,303  cache-misses:k       # 3.447303% miss rate  (68%)
8,921,161  instructions         # 10.070 M/sec         (86%)

Total test time: 1.029843 seconds.
```

#### Select target to stat

We can select which processes or threads to monitor via -p option or -t option. Monitoring a
process is the same as monitoring all threads in the process. Simpleperf can also fork a child
process to run the new command and then monitor the child process.

```sh
# Stat process 11904 and 11905.
$ simpleperf stat -p 11904,11905 --duration 10

# Stat thread 11904 and 11905.
$ simpleperf stat -t 11904,11905 --duration 10

# Start a child process running `ls`, and stat it.
$ simpleperf stat ls

# Stat a debuggable Android application.
$ simpleperf stat --app com.example.simpleperf.simpleperfexamplewithnative

# Stat system wide using -a option.
$ simpleperf stat -a --duration 10
```

#### Decide how long to stat

When monitoring existing threads, we can use --duration option to decide how long to monitor. When
monitoring a child process running a new command, simpleperf monitors until the child process ends.
In this case, we can use Ctrl-C to stop monitoring at any time.

```sh
# Stat process 11904 for 10 seconds.
$ simpleperf stat -p 11904 --duration 10

# Stat until the child process running `ls` finishes.
$ simpleperf stat ls

# Stop monitoring using Ctrl-C.
$ simpleperf stat -p 11904 --duration 10
^C
```

If you want to write program to control how long to monitor, you can send one of SIGINT, SIGTERM,
SIGHUP signals to simpleperf to stop monitoring.

#### Decide the print interval

When monitoring perf counters, we can also use --interval option to decide the print interval.

```sh
# Print stat for process 11904 every 300ms.
$ simpleperf stat -p 11904 --duration 10 --interval 300

# Print system wide stat at interval of 300ms for 10 seconds. Note that system wide profiling needs
# root privilege.
$ su 0 simpleperf stat -a --duration 10 --interval 300
```

#### Display counters in systrace

Simpleperf can also work with systrace to dump counters in the collected trace. Below is an example
to do a system wide stat.

```sh
# Capture instructions (kernel only) and cache misses with interval of 300 milliseconds for 15
# seconds.
$ su 0 simpleperf stat -e instructions:k,cache-misses -a --interval 300 --duration 15
# On host launch systrace to collect trace for 10 seconds.
(HOST)$ external/chromium-trace/systrace.py --time=10 -o new.html sched gfx view
# Open the collected new.html in browser and perf counters will be shown up.
```

### The record command

The record command is used to dump records of the profiled program. By passing options, we can
select which events to use, which processes/threads to monitor, what frequency to dump records,
how long to monitor, and where to store records.

```sh
# Record on process 7394 for 10 seconds, using default event (cpu-cycles), using default sample
# frequency (4000 samples per second), writing records to perf.data.
$ simpleperf record -p 7394 --duration 10
simpleperf I cmd_record.cpp:316] Samples recorded: 21430. Samples lost: 0.
```

#### Select events to record

By default, the cpu-cycles event is used to evaluate consumed cpu time. But We can also use other
events via -e option.

```sh
# Record using event instructions.
$ simpleperf record -e instructions -p 11904 --duration 10
```

#### Select target to record

The way to select target in record command is similar to that in the stat command.

```sh
# Record process 11904 and 11905.
$ simpleperf record -p 11904,11905 --duration 10

# Record thread 11904 and 11905.
$ simpleperf record -t 11904,11905 --duration 10

# Record a child process running `ls`.
$ simpleperf record ls

# Record a debuggable Android application.
$ simpleperf record --app com.example.simpleperf.simpleperfexamplewithnative

# Record system wide.
$ simpleperf record -a --duration 10
```

#### Set the frequency to record

We can set the frequency to dump records via the -f or -c options. For example, -f 4000 means
dumping approximately 4000 records every second when the monitored thread runs. If a monitored
thread runs 0.2s in one second (it can be preempted or blocked in other times), simpleperf dumps
about 4000 * 0.2 / 1.0 = 800 records every second. Another way is using -c option. For example,
-c 10000 means dumping one record whenever 10000 events happen.

```sh
# Record with sample frequency 1000: sample 1000 times every second running.
$ simpleperf record -f 1000 -p 11904,11905 --duration 10

# Record with sample period 100000: sample 1 time every 100000 events.
$ simpleperf record -c 100000 -t 11904,11905 --duration 10
```

#### Decide how long to record

The way to decide how long to monitor in record command is similar to that in the stat command.

```sh
# Record process 11904 for 10 seconds.
$ simpleperf record -p 11904 --duration 10

# Record until the child process running `ls` finishes.
$ simpleperf record ls

# Stop monitoring using Ctrl-C.
$ simpleperf record -p 11904 --duration 10
^C
```

If you want to write program to control how long to monitor, you can send one of SIGINT, SIGTERM,
SIGHUP signals to simpleperf to stop monitoring.

#### Set the path to store records

By default, simpleperf stores records in perf.data in current directory. We can use -o option to
set the path to store records.

```sh
# Write records to data/perf2.data.
$ simpleperf record -p 11904 -o data/perf2.data --duration 10
```

#### Record call graph

```sh
# Record dwarf based call graph
$ simpleperf record -p 11904 -g --duration 10

# Record stack frame based call graph
$ simpleperf record -p 11904 --call-graph fp --duration 10
```

#### Record off CPU time

Simpleperf is a CPU profiler, it generates samples for a thread only when it is running on a CPU.
However, sometimes we want to find out where time of a thread is spent, whether it is running on
CPU, preempted by other threads, doing I/O work, or waiting for some events. To support this, we
added the --trace-offcpu option in the simpleperf record command. When --trace-offcpu is used,
simpleperf generates a sample when a running thread is scheduled out, so we know the callstack of
a thread when it is scheduled out. And when reporting a perf.data generated with --trace-offcpu,
we use timestamp to the next sample (instead of event counts from the previous sample) as the
weight of current sample. As a result, we can get a callgraph based on timestamp, including both
on CPU time and off CPU time.

trace-offcpu is implemented using sched:sched_switch tracepoint event, which may not work well
on old kernels. But it is guaranteed to be supported on devices after Android O MR1. We can check
whether trace-offcpu is supported as below.

```sh
$ python run_simpleperf_on_device.py list --show-features
dwarf-based-call-graph
trace-offcpu
```

If trace-offcpu is supported, it will be shown in the feature list. Then we can try it.

```sh
# Record with --trace-offcpu option.
$ simpleperf record -g -p 11904 --duration 10 --trace-offcpu

# Record with --trace-offcpu using app_profiler.py.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity \
    -r "-g -e task-clock:u -f 1000 --duration 10 --trace-offcpu"
```

Below is an example comparing the profiling result with / without --trace-offcpu option.
First we record without --trace-offcpu option.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity

$ python report_html.py --add_disassembly --add_source_code --source_dirs ../demo
```

The result is [here](./without_trace_offcpu.html). In the result, all time is taken by
RunFunction(), and sleep time is ignored. But if we add --trace-offcpu option, we can consider
both run time and sleep time.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity \
    -r "-g -e task-clock:u --trace-offcpu -f 1000 --duration 10"

$ python report_html.py --add_disassembly --add_source_code --source_dirs ../demo
```

The result is [here](./trace_offcpu.html). In the result, half of the time is taken by
RunFunction(), and another half is taken by SleepFunction().

### The report command

The report command is used to report perf.data generated by the record command. The report command
groups records into different sample entries, sorts sample entries based on how many events each
sample entry contains, and prints out each sample entry. By passing options, we can select where
to find perf.data and executable binaries used by the monitored program, filter out uninteresting
records, and decide how to group records.

Below is an example. Records are grouped into 4 sample entries, each entry is a row. There are
several columns, each column shows piece of information belonging to a sample entry. The first
column is Overhead, which shows the percentage of events inside current sample entry in total
events. As the perf event is cpu-cycles, the overhead can be seen as the percentage of CPU
time used in each function.

```sh
# Reports perf.data, using only records sampled in libsudo-game-jni.so, grouping records using
# thread name(comm), process id(pid), thread id(tid), function name(symbol), and showing sample
# count for each row.
$ simpleperf report --dsos /data/app/com.example.sudogame-2/lib/arm64/libsudo-game-jni.so \
      --sort comm,pid,tid,symbol -n
Cmdline: /data/data/com.example.sudogame/simpleperf record -p 7394 --duration 10
Arch: arm64
Event: cpu-cycles (type 0, config 0)
Samples: 28235
Event count: 546356211

Overhead  Sample  Command    Pid   Tid   Symbol
59.25%    16680   sudogame  7394  7394  checkValid(Board const&, int, int)
20.42%    5620    sudogame  7394  7394  canFindSolution_r(Board&, int, int)
13.82%    4088    sudogame  7394  7394  randomBlock_r(Board&, int, int, int, int, int)
6.24%     1756    sudogame  7394  7394  @plt
```

#### Set the path to read records

By default, the report command reads perf.data in current directory. We can use -i option to select
another file to read records.

```sh
$ simpleperf report -i data/perf2.data
```

#### Set the path to find binaries

To report function symbols, simpleperf needs to read executable binaries used by the monitored
processes to get symbol table and debug information. By default, the paths are the executable
binaries used by monitored processes while recording. However, these binaries may not exist when
reporting or not contain symbol table and debug information. So we can use --symfs to redirect
the paths.

```sh
# In this case, when simpleperf wants to read executable binary /A/b, it reads file in /A/b.
$ simpleperf report

# In this case, when simpleperf wants to read executable binary /A/b, it prefers file in
# /debug_dir/A/b to file in /A/b.
$ simpleperf report --symfs /debug_dir
```

#### Filter records

When reporting, it happens that not all records are of interest. Simpleperf supports five filters
to select records of interest.

```sh
# Report records in threads having name sudogame.
$ simpleperf report --comms sudogame

# Report records in process 7394 or 7395
$ simpleperf report --pids 7394,7395

# Report records in thread 7394 or 7395.
$ simpleperf report --tids 7394,7395

# Report records in libsudo-game-jni.so.
$ simpleperf report --dsos /data/app/com.example.sudogame-2/lib/arm64/libsudo-game-jni.so
```

#### Group records into sample entries

The report command uses --sort option to decide how to group sample entries.

```sh
# Group records based on their process id: records having the same process id are in the same
# sample entry.
$ simpleperf report --sort pid

# Group records based on their thread id and thread comm: records having the same thread id and
# thread name are in the same sample entry.
$ simpleperf report --sort tid,comm

# Group records based on their binary and function: records in the same binary and function are in
# the same sample entry.
$ simpleperf report --sort dso,symbol

# Default option: --sort comm,pid,tid,dso,symbol. Group records in the same thread, and belong to
# the same function in the same binary.
$ simpleperf report
```

#### Report call graph

```
$ simpleperf report -g
``` 

## Scripts reference

<a name="app_profiler-py"></a>
### app_profiler.py

app_profiler.py can be used to record profiling data for Android applications and native
executables.

```sh
# Record an Android application.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative

# Record an Android application, but no need to recompile the app.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -nc

# Record an Android application, no need to recompile, but start an activity.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -nc \
      -a .SleepActivity

# Record a native process.
$ python app_profiler.py -np surfaceflinger

# Record a command.
$ python app_profiler.py -cmd \
    "dex2oat --dex-file=/data/local/tmp/app-profiling.apk --oat-file=/data/local/tmp/a.oat" \
    --arch arm

# Record using custom options for the simpleperf record command.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
    -r "-e cpu-clock -g --duration 30"

# Record off CPU time
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
    -r "-e task-clock -g -f 1000 --duration 10 --trace-offcpu"

# Profile activity startup time using --profile_from_launch.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
    --profile_from_launch --arch arm64
```

#### Profile from launch of an application

Sometimes we want to profile the launch-time of an application. To support this,
we added the --app option in the simpleperf record command. The --app option
sets the package name of the Android application to profile. If the app is not
already running, the simpleperf record command will poll for the app process in
a loop with an interval of 1ms. So to profile from launch of an application,
we can first start simpleperf record with --app, then start the app.
Below is an example.

```sh
$ python run_simpleperf_on_device record --app com.example.simpleperf.simpleperfexamplewithnative \
    -g --duration 1 -o /data/local/tmp/perf.data
# Start the app manually or using the `am` command.
```

To make it convenient to use, app_profiler.py combines these in the --profile_from_launch option.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .MainActivity \
    --arch arm64 --profile_from_launch
```

<a name="report-py"></a>
### report.py

report.py is a wrapper of the simpleperf report command on host. It accepts all options of the
report command.

```sh
# Report call graph
$ python report.py -g

# Report call graph in a GUI implemented by python Tk.
$ python report.py -g --gui
```

<a name="report_html-py"></a>
### report_html.py

report_html.py generates a report.html based on the profiling data. Then the report.html can show
the profiling result without depending on other files. So it can be shown in local browsers or
passed to other machines. The content of the report.html can include: chart statistics,
sample table, flame graph, annotated source code for each function, annotated disassembly for each
function.

```sh
# Generate chart statistics, sample table and flame graph, based on perf.data.
$ python report_html.py

# Add source code.
$ python report_html.py --add_source_code --source_dirs ../demo/SimpleperfExampleWithNative

# Add disassembly.
$ python report_html.py --add_disassembly
```

[Here](./report_html.html) is an example generated for SimpleperfExampleWithNative project.
By opening the report html, there are several tabs:

The first tab is "Chart Statistics". You can click in the pie chart to show time consumed
by each process, thread, library, and function.

The second tab is "Sample Table". It shows the time taken by each function. By clicking one
row in the table, we can jump to a new tab called "Function".

The third tab is "Flamegraph". It shows the flame graph result generated by inferno.

The forth tab is "Function". It only appears when users click a row in the "Sample Table" tab.
It shows information of a function, including:
```
1. A flame graph showing functions called by that function.
2. A flame graph showing functions calling that function.
3. Annotated source code of that function. It only appears when there are source code files for
   that function.
4. Annotated disassembly of that function. It only appears when there are binaries containing that
   function.
```

### inferno

inferno is a tool used to generate flame graph in a html file. Details is [here](./inferno.md).

```sh
# Generate flame graph based on perf.data.
$ ./inferno.sh -sc --record_file perf.data

# Record a native program and generate flame graph.
$ ./inferno.sh -np surfaceflinger
```

## Suggestions

### profile on Android >= N devices

We suggest profiling on Android >= N devices. Below are the reasons.

```
1. Running on a device reflects a real running situation, so we suggest profiling on real devices
   instead of emulators.
2. To profile Java code, we need ART running in oat mode, which is only available >= L for
   rooted devices, and >= N for non-rooted devices.
3. Old Android versions are likely to be shipped with old kernels (< 3.18), which may not support
   profiling features like dwarf based call graph.
4. Old Android versions are likely to be shipped with Arm32 chips. In Arm32 mode, stack frame based
   call graph doesn't work well.
```

### How to solve missing symbols in report?

```
The simpleperf record command already collects symbols on device in perf.data. But if the native
libraries you use on device are stripped, you can still result in a lot of unknown symbols in
the report. A solution is to build binary_cache on host.

```sh
# Collect binaries needed by perf.data in binary_cache/.
$ python binary_cache_builder.py -lib NATIVE_LIB_DIR,...
```

The NATIVE_LIB_DIRs passed in -lib option are the directories containing
unstripped native libraries on host. After running it, the native libraries
containing symbol tables are collected in binary_cache/. And you can use it
when reporting.

```sh
$ python report.py --symfs binary_cache

# report_html.py searches binary_cache/ automatically, so you don't need to
# pass it any argument.
$ python report_html.py
```

### How to improve call graph result?

```
1. Try both dwarf based call graph and stack frame based call graph. By default, app_profiler.py
   uses dwarf based call graph. But there are situations when stack frame based call graph is
   better. So it's better to try both.

2. Dwarf based call graph needs to have unstripped native binaries on device to generate good call
   graph. You can support it in two ways:
   a. Use unstripped native binaries when building the apk, as [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExampleWithNative/app/profiling.gradle).
   b. Pass directory containing unstripped native libraries to app_profiler.py via -lib option.
      And it will download the unstripped native libraries on device.

      $ python app_profiler.py -lib NATIVE_LIB_DIR
```
