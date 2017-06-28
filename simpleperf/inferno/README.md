<img style="float:left;" src="inferno.png" width="140"><div style="display:inline-block; font-size:120px;">INFERNO</div> 

## Description

Inferno is a flamegraph generator for native (C/C++) Android apps. It was originally written to profile and improve
surfaceflinger performance (Android compositior) but it can be used for any native Android application (since ART does
not generate frame pointers, Inferno cannot be used for Java/Kotlin apps). You can see a sample report generated with
Inferno [here](../report.html).

Notice there is no concept of time in a flame graph since callstacks are collapsed together. As a result, the width of a
flamegraph represents 100% of the number of samples and the height is related to the number of functions on the stack
when sampling occurred.


<img style="display:inline-block;" src="main_thread_flamegraph.png">

## How it works
Inferno relies on simpleperf to record the callstack of a native application thousands of times per second.
When recording stops, `simpleperf` dumps all the data it collected to `perf.data`. This file is pulled from the
Android device and processed on the host. The callstacks are merged together to visualize in which part of an
app the CPU cycles are spent.



## How to use it

Open a terminal and from `simpleperf` directory type:
```
./inferno.sh  (on Linux/Mac)
./inferno.bat (on Windows)
```

Inferno will collect data, process them and automatically open your web browser to display the HTML report.

## Parameters

You can select how long to sample for, the color of the node and many other things. Use `-h` to get a list of all
supported parameters.

```
inferno -h
```

## Troubleshooting

### Messy flame graph
A healthy flame graph features a single call site at its base.
If you don't see a unique call site like `_start` or `_start_thread` at the base from which all flames originate, it is
likely that stack unwinding failed while recording callstacks. To solve this problem, double check you compiled without
omitting frame pointers.

### run-as: package not debuggable
If you cannot run as root, make sure the app is debuggable otherwise simpleperf will not be able to profile it.